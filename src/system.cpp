//===========================================================================
// system.cpp simulates inclusive multi-level cache system with NoC and memory
//===========================================================================
/*
Copyright (c) 2015 Princeton University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Princeton University nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <inttypes.h>
#include <cmath>
#include <assert.h>

#include "system.h"
#include "common.h"




void System::init(XmlSys* xml_sys_in)
{
    int i,j;
    xml_sys = xml_sys_in;
    sys_type = xml_sys->sys_type;
    protocol_type = xml_sys->protocol_type;
    num_cores = xml_sys->num_cores;
    dram_access_time = xml_sys->dram_access_time; //Need to be fixed for PMC.
    page_size = xml_sys->page_size;
    tlb_enable = xml_sys->tlb_enable;
    shared_llc = xml_sys->shared_llc;
    num_levels = xml_sys->num_levels;
    verbose_report = xml_sys->verbose_report;
    total_bus_contention = 0;
    total_num_broadcast = 0;
    max_num_sharers = xml_sys->max_num_sharers;
    directory_cache = NULL;
    tlb_cache = NULL;

    proactive_flushing = false; //always false unless PF+

    pmodel = NONB; //FULL BARRIER SEMANTICS
    pmodel = (PersistModel) xml_sys->pmodel;
    printf("Persistent Model %d \n", pmodel);

    if(pmodel== BBPF){
        pmodel = BEPB;
        proactive_flushing = true; //BEPB with PF: LB++
    }
    else if(pmodel == RPPF){
        pmodel = RLSB;
        proactive_flushing = true; //BEPB with PF: LB++
    }
    else if(pmodel == PBPB){
        //NONB + persist buffer based design.
        pmodel = NONB;
        pbuff_enabled = true;  //Persist Buffer based design.
    }

    printf("Persistent Model %d and Proactive-Flushing Enabled:%s,  Persist-Buffer Enabled:%s \n", pmodel, (proactive_flushing)?"Yes":"No", (pbuff_enabled)?"Yes":"No" );

    hit_flag = new bool [num_cores];
    delay = new int [num_cores];
    delay_tmp = new int [num_cores]; // For the flush operations.
    delay_llc = new int [num_cores];
    delay_mc = new int [num_cores];
    return_delay = new int [num_cores];
    return_timer = new int [num_cores];

    for (i=0; i<num_cores; i++) {
        hit_flag[i] = false;
        delay[i] = 0;
        delay_tmp[i] = 0;
        delay_llc[i] = 0;
        delay_mc[i] = 0;
        return_delay[i] = 0;
        return_timer[i] = 0;
    }

    delay_persist = new int [num_cores];
    for (i=0; i<num_cores; i++) {
        delay_persist[i] = 0;
    }

    cache_level = new CacheLevel [num_levels];
    for (i=0; i<num_levels; i++) {
        cache_level[i].level = xml_sys->cache[i].level;
        cache_level[i].share = xml_sys->cache[i].share;
        cache_level[i].num_caches = (int)ceil((double)num_cores / cache_level[i].share);
        cache_level[i].access_time = xml_sys->cache[i].access_time;
        cache_level[i].size = xml_sys->cache[i].size;
        cache_level[i].block_size = xml_sys->cache[i].block_size;
        cache_level[i].num_ways = xml_sys->cache[i].num_ways;
        cache_level[i].ins_count = 0;
        cache_level[i].miss_count = 0;
        cache_level[i].miss_rate = 0;
        cache_level[i].lock_time = 0;
    }
    cache = new Cache** [num_levels];
    for (i=0; i<num_levels; i++) {
        cache[i] = new Cache* [cache_level[i].num_caches];
        for (j=0; j<cache_level[i].num_caches; j++) {
            cache[i][j] = NULL;
        }
    }

    network.init(cache_level[num_levels-1].num_caches, &(xml_sys->network));
    home_stat = new int [network.getNumNodes()];
    for (i=0; i<network.getNumNodes(); i++) {
        home_stat[i] = 0;
    }
    if (xml_sys->directory_cache.size > 0) {
        directory_cache = new Cache* [network.getNumNodes()];
        for (i = 0; i < network.getNumNodes(); i++) {
            directory_cache[i] = NULL;
        }
    }

    if (tlb_enable && xml_sys->tlb_cache.size > 0) {
        tlb_cache = new Cache [num_cores];
        for (i = 0; i < num_cores; i++) {
            tlb_cache[i].init(&(xml_sys->tlb_cache), TLB_CACHE, 0, page_size, 0, i);
        }
    }

    cache_lock = new pthread_mutex_t* [num_levels];
    for (i=0; i<num_levels; i++) {
        cache_lock[i] = new pthread_mutex_t [cache_level[i].num_caches];
        for (j=0; j<cache_level[i].num_caches; j++) {
            pthread_mutex_init(&cache_lock[i][j], NULL);
        }
    }

    directory_cache_lock = new pthread_mutex_t [network.getNumNodes()];
    for (i = 0; i < network.getNumNodes(); i++) {
        pthread_mutex_init(&directory_cache_lock[i], NULL);
    }

    cache_init_done = new bool* [num_levels];
    for (i=0; i<num_levels; i++) {
        cache_init_done[i] = new bool [cache_level[i].num_caches];
        for (j=0; j<cache_level[i].num_caches; j++) {
            cache_init_done[i][j] = false;
        }
    }   

    directory_cache_init_done = new bool [network.getNumNodes()];
    for (i = 0; i < network.getNumNodes(); i++) {
        directory_cache_init_done[i] = false;
    }

    //Persist Buffer Based Design.
    persist_buffer = new PBuff[num_cores];  
    int pbuff_delay = 5;
    int mcq_delay = 30;
    int pbuff_offset = (int) log2(xml_sys->directory_cache.block_size);

    for (i=0; i<num_cores; i++) {
        //initialize persist buffers.
        persist_buffer[i].init(PBUFF_SIZE, pbuff_delay, pbuff_offset);
        persist_buffer[i].core_id = i;

        //Dependnecy - check
        persist_buffer[i].dp_vector = (bool *) malloc(num_cores*sizeof(bool));
        //persist_buffer[i].dp_vector = new bool[num_cores];
        persist_buffer[i].has_dp = false;  
        //DPO-Ext: adding the addr vector for the last addr. Then, later flush all cahclines below that line.
        persist_buffer[i].dp_addr_vector = (uint64_t*) malloc(num_cores*sizeof(uint64_t));
    }

    mem_ctrl_queue =  new MCQBuff[1];
    mem_ctrl_queue->init(MCQBUFF_SIZE, mcq_delay); //MCQ delay is too much
    //initialize- 
    unique_write_id = new int [num_cores];
    
    is_last_access_dep = new bool [num_cores];
    last_access_core = new int [num_cores];
    last_access_addr = new int [num_cores];
    last_access_cache_tag = new int [num_cores];
    is_last_acc_barrier = new bool [num_cores];
    unique_last_write_id = new int [num_cores];

    for (i=0; i<num_cores; i++) {
        unique_write_id[i] = 0;

        is_last_access_dep[i] = false;
        last_access_core[i] = 0;
        last_access_addr[i] = 0;
        last_access_cache_tag[i] = 0;
        is_last_acc_barrier[i] = false;
        unique_last_write_id[i] = 0;
    }


    page_table.init(page_size, xml_sys->page_miss_delay);
    dram.init(dram_access_time);
}

void System::setDRAMOut(ofstream * dram_out)
{
    dram.setOut(dram_out); //set DRAM output
}

// This function models an access to memory system and returns the delay.
int System::access(int core_id, InsMem* ins_mem, int64_t timer)
{    
    //uint64_t tmp_addr = ins_mem->addr_dmem;  
    #ifdef DEBUG
        printf("\n[System] Core: %d, Atomic Type: %d \n", core_id, ins_mem->epoch_id);
        if(ins_mem->atom_type == FULL) printf("\n[System] Core: %d, Atomic Type: FULL to Address 0x%lx Memory Type: %d \n", core_id, (uint64_t) ins_mem->addr_dmem, ins_mem->mem_type);
        else if(ins_mem->atom_type == RELEASE) printf("\n[System] Core: %d, Atomic Type: Release to Address 0x%lx Memory Type: %d \n", core_id, (uint64_t) ins_mem->addr_dmem, ins_mem->mem_type);
        else if(ins_mem->atom_type == ACQUIRE) printf("\n[System] Core: %d, Atomic Type: Acquire to Address 0x%lx Memory Type: %d \n", core_id, (uint64_t) ins_mem->addr_dmem, ins_mem->mem_type);
        //if(ins_mem->addr_dmem == 0x6020e8) printf("\n[System] Core: %d, Atomic Type: Non-Atomic to Address 0x%lx Memory Type: %d \n", core_id, (uint64_t) ins_mem->addr_dmem, ins_mem->mem_type);
        //else if(ins_mem->addr_dmem == 0x602068 || 0x60206c) printf("\n[SYSTEM-ACCESS] Core: %d, Atomic Type: Non-Atomic to Address 0x%lx Memory Type: %d \n", core_id, (uint64_t) ins_mem->addr_dmem, ins_mem->mem_type);
    #endif
    int cache_id;
    if (core_id >= num_cores) {
        cerr << "Error: Not enough cores!\n";
        return -1;
    }

    hit_flag[core_id] = false;
    delay[core_id] = 0;
    cache_id = core_id / cache_level[0].share;

    //Place where the epoch meters were intially placed.
    if (tlb_enable) {
        delay[core_id] = tlb_translate(ins_mem, core_id, timer);
    }

    //persist-buffer clean up- NOTE: TAKE CARE OF WRITES AFTER READ DEPENDENT.
    ins_mem->return_unique_wr_id = 0;
    ins_mem->response_core_id = 0;
    ins_mem->is_return_dep = false; //CAN BE READ AD WELL.
    ins_mem->is_return_dep_barrier = false; 

    if (sys_type == DIRECTORY) {

        if(ins_mem->mem_type == WR){
            unique_write_id[core_id]++; 
            ins_mem->unique_wr_id = unique_write_id[core_id]; 
        } else{
            ins_mem->unique_wr_id = 0;
        }//Only for persist-buffer design

        mesi_directory(cache[0][core_id], 0, cache_id, core_id, ins_mem, timer + delay[core_id]);
        //Delay has been added to the Core-Id.
        //persist buffer-based design. delay += (persist buffer design). cache_cur = cache[0][core_id];
          
        if(pmodel == NONB && pbuff_enabled){
            //Access Persist Buffer.
            printf("\n\n Access Persist Buffer : Core %d - Address %lx \n", core_id, ins_mem->addr_dmem); //
            //Adding to add delays.
            if(ins_mem->is_return_dep){
                //Insert into the dep vector.
                if(ins_mem->is_return_dep_barrier){
                    persist_buffer[core_id].has_dp = true;
                    persist_buffer[core_id].dp_vector[ins_mem->response_core_id] = true;
                    //DPO_ext
                    persist_buffer[core_id].dp_addr_vector[ins_mem->response_core_id] = ins_mem->addr_dmem;
                }
            }

            if(ins_mem->mem_type == WR){
                //need to clean the dep vector.
                delay[core_id] += accessPersistBuffer(timer+delay[core_id], core_id, ins_mem);
            }

        }

    }//Mesi DIrectory
    else {
        mesi_bus(cache[0][core_id], 0, cache_id, core_id, ins_mem, timer + delay[core_id]);
    }

    #ifdef DEBUG
        if(ins_mem->atom_type == RELEASE)
            {
                printf("[System] Core: %d accessed address 0x%lx within %d ns - atomics %s \n", core_id, (uint64_t) ins_mem->addr_dmem, delay[core_id], (ins_mem->atom_type == RELEASE)?" is RELEASE":"is ACQUIRE");
                printf("---------------------------------------------------------------------------------------\n");
            }
        if(ins_mem->atom_type == ACQUIRE){
            printf("[System] Core: %d accessed address 0x%lx within %d ns - atomics %s \n", core_id, (uint64_t) ins_mem->addr_dmem, delay[core_id], (ins_mem->atom_type == RELEASE)?" is RELEASE":"is ACQUIRE");
            printf("---------------------------------------------------------------------------------------\n");
    
            }
     #endif
    return delay[core_id];
}



//PERSIST-BUFFER - Based on the Delegated Persist Ordering (DPO) of Asheesh Kolli.
int System::accessPersistBuffer(uint64_t timer, int core_id, InsMem * ins_mem){
    int delay = 0;
    //lock
    int offset = (int) log2(xml_sys->directory_cache.block_size);    
    //cout << "Check Offset : " << offset << endl;
    assert(offset == 6); //May be set the offset here.
    //addrParse(ins_mem->addr_dmem, &addr_temp); //Cache address parser

    PBuff * cur_pbuff = &persist_buffer[core_id];
    PBuffLine * cur_pbuff_line = cur_pbuff->access(ins_mem->addr_dmem); //Access Cacheline.

    //NOTE : write+barrier must always start a new epoch.--------->
    cur_pbuff->num_wr++; //increment on each write.

    //InsMem does not have it.
    if(cur_pbuff->has_dp){ 
            printf("Accumulate DP \n");
            ins_mem->atom_type = RELEASE;
            cur_pbuff_line = NULL; // New barrier logic

            cur_pbuff->num_acq_barr++;//Acquire barrier            
    }

    //pbuff hit. check barriers also. if a barrier presents, then no hit. cannot cross the barrier.
    if( (cur_pbuff_line != NULL) ){
        //Just Access.  No barrier In between. 
        printf("Persist Buffer < HIT > : 0x%lx cache 0x%lx \n", ins_mem->addr_dmem, ins_mem->addr_dmem >> offset);    
        delay += cur_pbuff->getAccessDelay(); 
        cur_pbuff_line->last_addr = ins_mem->addr_dmem; //Check this.
        cur_pbuff_line->unique_wr_id = ins_mem->unique_wr_id;
        cur_pbuff_line->last_unique_wr_id = ins_mem->unique_wr_id;
        cur_pbuff_line->num_wr_coal++;

    }else{
        printf("Persist Buffer < MISS > : 0x%lx cache 0x%lx \n", ins_mem->addr_dmem, ins_mem->addr_dmem >> offset);
        //Insert. and solve conflicts.
        if(cur_pbuff->isFull()){
            printf("Persist Buffer is Full \n");

            //Removing line from the queue. Buff should not be empty
            PBuffLine * rline = cur_pbuff->remove();  //rline = removed line from the local pbuff
            cur_pbuff->printBuffer();
            if(rline == NULL) printf("[REMOVE] NULL \n");
            printf("[REMOVE] Removed the pbuffline 0x%lx from buffer %d\n", rline->cache_tag, core_id);
            //NOTE: Check dependencies and enforce to the same cacheline.
            if(rline->has_dp){
                printf("[REMOVE] dependecies \n");
                printf("[REMOVE] dependecies2 \n");

                for(int i=0; i<num_cores;i++){
                    //Enforcing dependencies. //need a special function to accedd MCQ while solving dependencies of every core.
                    printf("[DEPEND] dependencies in pbuffer %d with buffer %d has %d \n", core_id, i, rline->dp_vector[i]);

                    if(i != core_id){
                        printf("[DEPEND] Solving dependencies in pbuffer %d with buffer %d has %d \n", core_id, i, rline->dp_vector[i]);
                        if(rline->dp_vector[i]){
                            printf("Persist Buffer dependnecy with core %d is %d \n", i, rline->dp_vector[i]);
                            //THIS ADDRESS IS NOT CORRECT.
                            uint64_t dep_addr = rline->dp_addr_vector[i];
                            printf("Dependnecy Address is 0x%lx in buffer %d", dep_addr, i);
                            delay += flushPersistBuffer(timer, i, dep_addr); //IDEALLY CACHE TAG
                            //delay += flushPersistBuffer(timer, i, rline->last_addr); //IDEALLY CACHE TAG
                            printf("[DEPEND] Solved %d \n", core_id);
                        } 
                    }
                }
            }
            printf("Now the buffer is not full");
            delay += accessMCQBuffer(timer+delay, core_id, rline);
        }  

        printf("Checkpoint\n");
        assert(!cur_pbuff->isFull());     
            //Create a new pbuffline. Only after solving conflicts
            //PBuffLine new_line;
            PBuffLine * new_line = (PBuffLine*) malloc(sizeof(PBuffLine)); 
            //unique-id.
            new_line->timestamp = timer+delay; 
            new_line->cache_tag = (ins_mem->addr_dmem) >> (cur_pbuff->getOffset()); //PBUFF Offset
            new_line->core_id = core_id;
            new_line->last_addr = ins_mem->addr_dmem;
            new_line->is_barrier = (ins_mem->atom_type != NON)? true: false; // Now: Only for the release.
            new_line->unique_wr_id = ins_mem->unique_wr_id;
            new_line->last_unique_wr_id = ins_mem->unique_wr_id;
            new_line->num_wr_coal = 1;
            new_line->next_line = NULL;

            //dp_vecotor : This is wrong. Acquire ceates a persist barrier.
            if(cur_pbuff->has_dp){  //AccumDP
                printf("Dependency Found \n");

                new_line->has_dp = true;
                new_line->dp_vector = cur_pbuff->dp_vector;
                new_line->dp_addr_vector = cur_pbuff->dp_addr_vector;
                //cur_pbuff_line->has_dp = true; //WRONG
                //cur_pbuff_line->dp_vector = cur_pbuff->dp_vector; //WRONG

                cur_pbuff->dp_vector = (bool *) malloc(num_cores*sizeof(bool));
                //cur_pbuff->dp_vector = new bool[num_cores]; //This also has the same effect.
                cur_pbuff->has_dp = false;
                //DPO-Ext(not clear in DPO)
                cur_pbuff->dp_addr_vector = (uint64_t*) malloc(num_cores*sizeof(uint64_t));

            }else{
                new_line->has_dp = false;
                new_line->dp_vector = NULL;
            }

            //persist-buffer dependencies
            ins_mem->return_unique_wr_id = 0;
            ins_mem->response_core_id = 0;
            ins_mem->is_return_dep = false;
            ins_mem->is_return_dep_barrier = false; //Read or write to the barrier.

            // NEED TO UPDATE ACQUIRE BARRIER BY TRACKING COHERENCE.
            //Update dependent details. This need to be coded. On Acquire we need to have a barrier on dependent side.
            cur_pbuff->insert(new_line);
            printf("[INSERT] : 0x%lx cache_tag 0x%lx \n", new_line->last_addr, new_line->cache_tag);
            //cur_pbuff->printBuffer();
        //solve conflicts.
    }
    //unlock
    return delay;
}

int System::flushPersistBuffer(uint64_t timer, int core_id, uint64_t addr){

    int delay=0;
    //Go throught he buffer. Flush all dependednt and required cachline.
    PBuff * cur_pbuff = &persist_buffer[core_id];
    printf("[FLUSH] Persist Buffer Dependency Mechanism Buffer: %d Address: 0x%lx cacheline 0x%lx \n", core_id, addr, addr >> cur_pbuff->getOffset());


    PBuffLine * cur_pbuff_line = cur_pbuff->access(addr); // CHECK this!!!!!!!!!!!!!!!!!!!

    //PBuffLine * tmp = cur_pbuff->head;
    if(cur_pbuff_line != NULL){

        cur_pbuff->num_dep_conflicts++;
         printf("[FLUSH] Dependency Found %d Address: 0x%lx cacheline 0x%lx \n", core_id, addr, addr >> cur_pbuff->getOffset());
        while(true){
            PBuffLine * rline = cur_pbuff->remove(); //This moves the HEAD pointer as well.
            delay += accessMCQBuffer(timer+delay, core_id, rline);    
            cur_pbuff->num_dep_cls_flushed++;
            if(rline == cur_pbuff_line)break;
         } 
    }

    return delay;
}

//MCQ-BUFFER
int System::accessMCQBuffer(uint64_t timer, int core_id, PBuffLine * new_line){
    
    int delay = 0; // delay_dram = 0;

    printf("Access MCQ buffer by core %d with cacheline 0x%lx and addr 0x%lx \n", core_id, new_line->cache_tag, new_line->last_addr);
    //lock
    delay += mem_ctrl_queue->getAccessDelay();

    if(mem_ctrl_queue->isFull()){
        //remove
        PBuffLine * rm_line = mem_ctrl_queue->remove();
        uint64_t addr = rm_line->last_addr;

        //Persist----------------------------------------check
        InsMem im;
        im.addr_dmem = addr;
        delay += dram.access(&im); //DRAM Access
        //persist ---------------------------------------check
    }

    assert(!mem_ctrl_queue->isFull());        
    delay += mem_ctrl_queue->getAccessDelay(); 
    //insert
    mem_ctrl_queue->insert(new_line, new_line->last_addr, core_id);
    //unlock
    return delay;
}


//Only call in level0
int System::epoch_meters(int core_id, InsMem * ins_mem){
    //epoch counters
    cache[0][core_id]->epoch_updated_id = ins_mem->epoch_id;
    
    if(ins_mem->mem_type == WR){ //Only for the stores/writes
        if(cache[0][core_id]->epoch_last != (uint64_t)ins_mem->epoch_id){
            
            if(cache[0][core_id]->epoch_last == 0){
                cache[0][core_id]->epoch_min = cache[0][core_id]->epoch_size;
                cache[0][core_id]->epoch_max = cache[0][core_id]->epoch_size; // this might be zero as well.
                if(cache[0][core_id]->epoch_size != 0) cache[0][core_id]->epoch_min_set = true;
            }else{
                if(cache[0][core_id]->epoch_max < cache[0][core_id]->epoch_size) cache[0][core_id]->epoch_max = cache[0][core_id]->epoch_size;
                
                if(cache[0][core_id]->epoch_min_set){
                    if(cache[0][core_id]->epoch_min > cache[0][core_id]->epoch_size) cache[0][core_id]->epoch_min = cache[0][core_id]->epoch_size;
                }else{
                    cache[0][core_id]->epoch_min = cache[0][core_id]->epoch_size;
                    cache[0][core_id]->epoch_min_set = true;
                }
            }

            cache[0][core_id]->epoch_sum += cache[0][core_id]->epoch_size;            
            cache[0][core_id]->epoch_size = 0;
            cache[0][core_id]->epoch_last = ins_mem->epoch_id;
            cache[0][core_id]->epoch_counted_id++;
        }
        else{
            cache[0][core_id]->epoch_size++;
        } 
    }

    return 1;
    
}
//Initialize caches on demand
Cache* System::init_caches(int level, int cache_id)
{
    int k;
    pthread_mutex_lock(&cache_lock[level][cache_id]);
    if (cache[level][cache_id] == NULL) {
        cache[level][cache_id] = new Cache();
        cache[level][cache_id]->init(&(xml_sys->cache[level]), DATA_CACHE, xml_sys->bus_latency, page_size, level, cache_id);
        if (level == 0) {
            cache[level][cache_id]->num_children = 0;
            cache[level][cache_id]->child = NULL;
        }
        else {
            cache[level][cache_id]->num_children = cache_level[level].share/cache_level[level-1].share;
            cache[level][cache_id]->child = new Cache* [cache[level][cache_id]->num_children];
            for (k=0; k<cache[level][cache_id]->num_children; k++) {
                cache[level][cache_id]->child[k] = cache[level-1][cache_id*cache[level][cache_id]->num_children+k];
            }
        }

        if (level == num_levels-1) {
            cache[level][cache_id]->parent = NULL;
        }
        else {
            init_caches(level+1, cache_id*cache_level[level].share/cache_level[level+1].share);
            cache[level][cache_id]->parent = cache[level+1][cache_id*cache_level[level].share/cache_level[level+1].share];
        }
    }
    else {
        for (k=0; k<cache[level][cache_id]->num_children; k++) {
            cache[level][cache_id]->child[k] = cache[level-1][cache_id*cache[level][cache_id]->num_children+k];
        }
    }
    cache_init_done[level][cache_id] = true;

    //printf("[Cache Initialization] Initiated Cache : %d of level %d \n", cache_id, level);
    pthread_mutex_unlock(&cache_lock[level][cache_id]);
    return cache[level][cache_id];
}

void System::init_directories(int home_id)
{
    pthread_mutex_lock(&directory_cache_lock[home_id]);
    if (directory_cache[home_id] == NULL) {
        directory_cache[home_id] = new Cache();
        //BUGGGG//directory_cache[home_id]->init(&(xml_sys->directory_cache), DIRECTORY_CACHE, xml_sys->bus_latency, page_size, 0, home_id);
        directory_cache[home_id]->init(&(xml_sys->directory_cache), DIRECTORY_CACHE, xml_sys->bus_latency, page_size, num_levels, home_id);

    }
    directory_cache_init_done[home_id] = true;
    //printf("Directory is created : Home Id - %d of level %d \n\n", home_id, num_levels);
    pthread_mutex_unlock(&directory_cache_lock[home_id]);
}



// This function models an acess to a multi-level cache sytem with bus-based
// MESI coherence protocol
char System::mesi_bus(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer)
{
    int i, shared_line;
    int delay_bus;
    Line*  line_cur;
    Line*  line_temp;
    InsMem ins_mem_old;
    
    if (!cache_init_done[level][cache_id]) {
        cache_cur = init_caches(level, cache_id); 
    }
    if (cache_cur->bus != NULL) {
        delay_bus = cache_cur->bus->access(timer+delay[core_id]);
        total_bus_contention += delay_bus;
        delay[core_id] += delay_bus;
    }
    delay[core_id] += cache_level[level].access_time;
    if(!hit_flag[core_id]) {
        cache_cur->incInsCount();
    }

    cache_cur->lockUp(ins_mem);
    line_cur = cache_cur->accessLine(ins_mem);
    //Cache hit
    if (line_cur != NULL) {
        line_cur->timestamp = timer+delay[core_id];
        hit_flag[core_id] = true; 
        //Write
        if (ins_mem->mem_type == WR) {
           if (level != num_levels-1) {
               if (line_cur->state != M) {
                   line_cur->state = I;
                   line_cur->state = mesi_bus(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer+delay[core_id]);
               }
           }
           else {
               if (line_cur->state == S) {
                   shared_line = 0;
                   for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                       if (i != cache_id) {
                           if (!cache_init_done[num_levels-1][i]) {
                               init_caches(num_levels-1, i);
                           }
                           line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                           if(line_temp != NULL) {
                               shared_line = 1;
                               line_temp->state = I;
                               inval_children(cache[num_levels-1][i], ins_mem, core_id,timer);
                           }
                       }
                   }
               }
               line_cur->state = M;
           }
           inval_children(cache_cur, ins_mem, core_id,timer);
           cache_cur->unlockUp(ins_mem);
           return M;
        }
        //Read
        else {
            if (line_cur->state != S) {
                share_children(cache_cur, ins_mem, core_id, timer);
            }
            cache_cur->unlockUp(ins_mem);
            return S;
        }
    }
    //Cache miss
    else
    {
        //Evict old line
        line_cur = cache_cur->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            inval_children(cache_cur, &ins_mem_old, core_id, timer);
        }
        line_cur->timestamp = timer+delay[core_id];
        if (level != num_levels-1) {
            line_cur->state = mesi_bus(cache_cur->parent, level+1, 
                                   cache_id*cache_level[level].share/cache_level[level+1].share, core_id, 
                                   ins_mem, timer+delay[core_id]);
        }
        else {
            //Write miss
            if (ins_mem->mem_type == WR) {
                shared_line = 0;
                for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                    if (i != cache_id) {
                        if (!cache_init_done[num_levels-1][i]) {
                            init_caches(num_levels-1, i);
                        }
                        line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                        if (line_temp != NULL) {
                            shared_line = 1;
                            inval_children(cache[num_levels-1][i], ins_mem, core_id, timer);
                            if (line_temp->state == M || line_temp->state == E) {
                                line_temp->state = I;
                                break;
                            }
                            else {
                                line_temp->state = I;
                            }
                         }
                    }
                 }
                 line_cur->state = M;
             }   
             //Read miss
             else {
                 shared_line = 0;
                 for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                    if (i != cache_id) {
                         if (!cache_init_done[num_levels-1][i]) {
                             init_caches(num_levels-1, i);
                         } 
                         line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                         if(line_temp != NULL)
                         {
                             shared_line = 1;
                             share_children(cache[num_levels-1][i], ins_mem, core_id, timer);
                             if (line_temp->state == M || line_temp->state == E) {
                                 line_temp->state = S;
                                 break;
                             }
                             else {
                                 line_temp->state = S;
                             }
                         }
                    }
                 }
                 if (shared_line) {
                     line_cur->state = S;
                 }
                 else {
                     line_cur->state = E;
                 }
             }
             delay[core_id] += dram.access(ins_mem);                    
        }  
        cache_cur->incMissCount();        
        cache_cur->unlockUp(ins_mem);
        return line_cur->state; 
    }
}

// This function models an acess to a multi-level cache sytem with directory-
// based MESI coherence protocol
char System::mesi_directory(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer)
{
    int id_home, delay_bus;
    char state_tmp;
    Line*  line_cur;
    InsMem ins_mem_old; 
    
    if (!cache_init_done[level][cache_id]) {
        //printf("\n[Cache] -----------------------Initiating Cache %d of %d --------------------\n", cache_id, level);
        cache_cur = init_caches(level, cache_id);       
        //printf("[Cache] -----------------------Initiated  Cache %d of %d --------------------\n\n\n", cache_id, level);  
    }

    assert(cache_cur != NULL);
    if (cache_cur->bus != NULL) {
        delay_bus = cache_cur->bus->access(timer+delay[core_id]);
        total_bus_contention += delay_bus;
        delay[core_id] += delay_bus;
    }

    if (!hit_flag[core_id]) {
        cache_cur->incInsCount();
    }

    cache_cur->lockUp(ins_mem);
    delay[core_id] += cache_level[level].access_time;

    //counters
    if(ins_mem->mem_type==WR && level==0){
        if(ins_mem->mem_op==2){
            cache_cur->rmw_count++;
            if(ins_mem->atom_type==FULL)cache_cur->rmw_full_count++;
            else if(ins_mem->atom_type==ACQUIRE)cache_cur->rmw_acq_count++;
            else if(ins_mem->atom_type==RELEASE)cache_cur->rmw_rel_count++;
        }
        else {
            cache_cur->wrt_relaxed_count++;

            if(ins_mem->atom_type==FULL)cache_cur->wrt_full_count++;
            else if(ins_mem->atom_type==ACQUIRE)cache_cur->wrt_acq_count++;
            else if(ins_mem->atom_type==RELEASE)cache_cur->wrt_rel_count++;
        }
    }
    else if(ins_mem->mem_type==RD){
        //after-asplos //Whatever read. 
        cache_cur->rd_count++;
    }else{
        //Do nothing.        
    }

    if(cache_cur->largest_epoch_id < ins_mem->epoch_id) cache_cur->largest_epoch_id = ins_mem->epoch_id; 

    if(level==0){
        //Epoch meters
        epoch_meters(core_id, ins_mem);
        cache_cur->clk_timestamp = timer; //Last Cycle accessed.

        //Last Release cycle
        if(ins_mem->mem_type == WR && (ins_mem->atom_type != NON)){
            cache_cur->atom_clk_timestamp = timer; //Last Cycle accessed.
        }
    }

    //Full barrier flush in reelase persistency - Special case   
    //Full flush - Full barrier Release Persistency (on the critical path)
    if(pmodel == FBRP && level==0){
        //Nedd to flush all cachelines with epoch id > min epoch id
        //Need to free the syncmap
        if(ins_mem->mem_type==WR && ins_mem->atom_type==FULL){
            fullBarrierFlush(timer+delay[core_id], cache_cur, ins_mem->epoch_id, core_id, 0); //taken as a visibility conflict. 

            #ifdef ASPLOS_AFTER
               //Check if I need to flush the barrier writes?. 
            #endif
            
        }        
        //Full barrier semantics on Release persistency
    }

    line_cur = cache_cur->accessLine(ins_mem);

 
    //Cache hit
    if (line_cur != NULL) {
        //printf("Messi hit 0x%lx cache %d level %d \n", ins_mem->addr_dmem, cache_id, level);
        line_cur->timestamp = timer + delay[core_id];
        hit_flag[core_id] = true; 
        
        #ifdef DEBUG
        if(ins_mem->atom_type != NON){
        //if(level >=0 && cache_id>=0){
            printf("[MESI]   Op: %d Type: %d Hits 0x%lx, cache level : %d and Cache Id: %d in State : %d and Line atom: %d RMW: %s Memory: %s Epoch: %d \n", 
                ins_mem->mem_type , ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, line_cur->state, line_cur->atom_type, (ins_mem->mem_op==2)?"yes":"no", (ins_mem->mem_type==0)?"read":"write", ins_mem->epoch_id);
        }
        #endif

        //Write
        if (ins_mem->mem_type == WR) {

            if (level != num_levels-1) {

                if(line_cur->state == M  || line_cur->state == E){ //Cache hit
                    //Check for buffered epoch persistency - BEP
                    if(pmodel == BEPB && level==0){
                        if(line_cur->dirty && ins_mem->epoch_id > line_cur->max_epoch_id){ //Both same address visibility and multi-word block conflicts
                            //Buffered Epoch Persistency- Intra-thread conflict
                            //delay[core_id] += persist(cache_cur, ins_mem, line_cur, core_id); //BEP
                            
                            //persist(cache_cur, ins_mem, line_cur, core_id);
                            persist(timer+delay[core_id], cache_cur, ins_mem, line_cur, core_id,0);

                               int delay_wb =100; //invoked writebacks, CLWB happenes in the critical path
                                cache_cur->critical_write_back_count++;     
                                cache_cur->critical_write_back_delay += delay_wb; //delay must be
                                cache_cur->write_back_count++;
                                cache_cur->write_back_delay+=delay_wb;

                                cache_cur->same_block_count++;
                                cache_cur->same_block_delay+=delay_wb;
                            
                            //also need to write back this cache line - NOT 
                            #ifdef ASPLOS_AFTER
                                //writing back cacheline with clwb with non-invalidations. //mesi writeback without invalidating. clwb
                                //1. create the new ins_mem with mem_type.                                  
                                    ins_mem->mem_type = CLWB;
                                    ins_mem->lazy_wb = false; //No-Need

                                //2. MESI write-backs. return the same cache state
                                    mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer + delay[core_id]);
                                //3. This about the delay and timer

                                //final:change the mem_type back to the WR 
                                    ins_mem->mem_type = WR;

                            #endif

                        }   
                    }
                }//End of BEP flush

               if (line_cur->state != M) {
                   line_cur->state = I;                 

                   line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer + delay[core_id]);

               }                              
                //if(ins_mem->atom_type != NON) line_cur->atom_type = ins_mem->atom_type;                   
    
                    if(level==0){ //Changed only for RP, BEP

                        //for the persist buffers - PBUFF
                        line_cur->unique_wr_id = ins_mem->unique_wr_id;

                        if(ins_mem->atom_type != NON){
                            line_cur->atom_type = ins_mem->atom_type; //Update only when atomic word is inserted.                                        
                             
                            if(pmodel==RLSB){
                                SyncLine * new_line =  cache_cur->addSyncLine(ins_mem); //Can write isFull();
                                if(new_line == NULL) {
                                    syncConflict(cache_cur, new_line, line_cur);
                                    cache_cur->addSyncLine(ins_mem); //Dont erase the first line
                                } //dont erase new write
                                assert(new_line !=NULL);
                            }
                            else if(pmodel==FBRP){
                                SyncLine * new_line =  cache_cur->addSyncLine(ins_mem); //Can write isFull();
                                if(new_line == NULL) {
                                    syncConflict(cache_cur, new_line, line_cur);
                                    cache_cur->addSyncLine(ins_mem); //Dont erase the first line
                                } //dont erase new write
                                assert(new_line !=NULL);
                            }
                            //Need to check the overflow- Then flush both cache and sync reg                       
                        }
                        
                        if(line_cur->dirty){
                           if(ins_mem->epoch_id < line_cur->min_epoch_id){
                            line_cur->min_epoch_id = ins_mem->epoch_id;
                             //Release epoch id can be overwritten later.
                            }

                            if(ins_mem->epoch_id > line_cur->max_epoch_id){ //Latest epoch ID.
                            line_cur->max_epoch_id = ins_mem->epoch_id;
                            //check dirty bit?
                            }
                            
                        }else{
                            line_cur->min_epoch_id = ins_mem->epoch_id;
                            line_cur->max_epoch_id = ins_mem->epoch_id;
                            line_cur->dirty = 1;
                        }                    
    
                        //if(ins_mem->epoch_id > line_cur->max_epoch_id){
                        //    line_cur->max_epoch_id = ins_mem->epoch_id;                           
                        //}                         
                    } 
           }
           //LLC Write
           else {
               if (line_cur->state == S) {
                   id_home = getHomeId(ins_mem);
                   delay[core_id] += network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   
                   if (shared_llc) {
                       delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    
                   }    
                   else {
                       delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                   }
                   delay[core_id] += network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

               }    
                          
               //ERROR: Missing M state. Need to get data from owner.
               else if(line_cur->state == M || line_cur->state==E){
                    //Handle M state- Error in States.
                    delay[core_id] += inval_children(cache_cur, ins_mem, core_id, timer+delay[core_id] );
                    //--NEED WRITE-BACK DELAY ----///////////////////////////////////////////////////////////////CODE                    
                    //printf("ERROR \n");
               }              
               
               //line_cur->atom_type = ins_mem->atom_type;
               line_cur->state = M; 
               //Modified state does not track owners. 
               //Error: 2 Owners can exist in the same time.
           }
           cache_cur->unlockUp(ins_mem);

            #ifdef DEBUG
            if(ins_mem->atom_type != NON)printf("[MESI]   Op: %d Type: %d Hits 0x%lx, cache level : %d and Cache Id: %d in State : %d and Line atom: %d Line Tag : 0x%lx Line Index Write Ends\n", 
                            ins_mem->mem_type, ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, line_cur->state, line_cur->atom_type, line_cur->tag);
            #endif
           return M;
        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //Write-Back
        else if(ins_mem->mem_type == WB){ //mem_type=2

            #ifdef DEBUG
                    printf("[MESI-WB]   Op: %d Type: %d Hits 0x%lx, cache level : %d and Cache Id: %d in State : %d and Line atom: %d Line Tag : 0x%lx Line Index Write Ends\n", 
                            ins_mem->mem_type, ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, line_cur->state, line_cur->atom_type, line_cur->tag);
            #endif

            if(level!=num_levels-1){
                line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer + delay[core_id]);
                line_cur->state = I; //Only for directory cache 
                line_cur->dirty = 0;
                //asplos-after: No need to change the 
                return I;
            }
            else{

                    //New Logic
                if(pmodel != NONB){
                    assert (line_cur->state == M || line_cur->state==E);
                    //delay[core_id] += inval_children(cache_cur, &ins_mem); //Invalidating all S->I, M->I

                    id_home = getHomeId(ins_mem);
                    delay[core_id] += network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   
                    if (shared_llc) {
                        if(!ins_mem->lazy_wb)
                            delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                        else
                            accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    }    
                    else {
                        if(!ins_mem->lazy_wb)
                            delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                        else{
                            accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                        }
                    }
                    delay[core_id] += network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

                    line_cur->state = I; //Only for directory cache 
                    line_cur->dirty = 0;
                    return I;
                }
                else{ //Adding no delay. Becuase its not in the critical path
                    //For no persistency write-back is not in the critical path
                    
                    assert (line_cur->state == M || line_cur->state==E);
                    //delay[core_id] += inval_children(cache_cur, &ins_mem); //Invalidating all S->I, M->I

                    id_home = getHomeId(ins_mem);
                    network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   
                    if (shared_llc) {
                       accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    
                    }    
                    else {
                       accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    }
                    network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

                    line_cur->state = I; //Only for directory cache 
                    line_cur->dirty = 0;
                    return I;
                }
            }
            
        }
        //afterasplos: new CLWB support without invalidating and lazy options. (also clflush)
        else if (ins_mem->mem_type == CLWB){
            //New clwb
            if(level!=num_levels-1){
                //Two version: Lazy and non-lazy: timing aware and not.
                //Keep ins_mem->mem_type intact
                mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer + delay[core_id]);
                //line_cur->state = I; //Only for directory cache 
                //line_cur->dirty = 0;
                //return I;
                return line_cur->state;
            }
            else{

                //New Logic: //BEP and LRP. CLWB.
                //two options: lazy: no-delay, nlazy:delayed.

                ///Also need s lazy varinat of this! ins_mem--> lazy_flag.
                if(pmodel != NONB){
                    assert (line_cur->state == M || line_cur->state==E);
                    //delay[core_id] += inval_children(cache_cur, &ins_mem); //Invalidating all S->I, M->I

                    id_home = getHomeId(ins_mem);
                    delay[core_id] += network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   
                    if (shared_llc) {
                       delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    
                    }    
                    else {
                       delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    }
                    delay[core_id] += network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

                    //line_cur->state = I; //Only for directory cache 
                    //line_cur->dirty = 0;
                    //return I;
                    return line_cur->state;
                }

                 //No persistency --> NONB. BEP andn RP this never happens
                else{ //Adding no delay. Becuase its not in the critical path
                    //For no persistency write-back is not in the critical path
                    
                    assert (line_cur->state == M || line_cur->state==E);
                    //delay[core_id] += inval_children(cache_cur, &ins_mem); //Invalidating all S->I, M->I

                    id_home = getHomeId(ins_mem);
                    network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   
                    if (shared_llc) {
                       accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    
                    }    
                    else {
                       accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    }
                    network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

                    //line_cur->state = I; //Only for directory cache 
                    //line_cur->dirty = 0;
                    //return I;
                    return line_cur->state;
                }
            }

        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //Read
        else {
            //ERROR COHERENCE - MAHESH //Can be handled with Exclusive as well //New Error: New Error when level=0           
                if (line_cur->state != S) {
                    delay[core_id] += share_children(cache_cur, ins_mem, core_id, timer+delay[core_id]); //Add core_id             
                    //Error: Need to write-back modified data and make state to S. 
                    //line_cur->state = S;                      
                    //Still a Error : Consider level 0;                                       
                    if (level == num_levels-1) {                        
                        //Added    
                        id_home = getHomeId(ins_mem);
                        delay[core_id] += network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                        
                        if (shared_llc) {
                            delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                         
                        }    
                        else {
                            delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                        }
                        delay[core_id] += network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

                        line_cur->state = S;
                        //Added        
                    }
                    else if (level == 0) {
                        //Dont do anything!. This fixed the error!.
                    }                   
                }

            cache_cur->unlockUp(ins_mem);

            #ifdef DEBUG
            if(ins_mem->atom_type != NON)printf("[MESI]   Op: %d Type: %d Hits 0x%lx, cache level : %d and Cache Id: %d in State : %d and Line atom: %d  Line Tag : 0x%lx Line Index Write Ends\n", 
                            ins_mem->mem_type, ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, line_cur->state, line_cur->atom_type, line_cur->tag);
            #endif
            return S;
        }

        //2. Error Where is Writeback ralted to 1.
    }


    //Cache miss
    else {

        //Serious Bug: There are some cacheline in the L0 that are not onvalidaed ot shared and no state in the higher level cache. Check invalidate..
        //Same probelm Everywhere. Happenes when epoch sizes are large. Temporary solution is to ignore writebacks for missed cachelines.
        //Invariant: If something is in the cacheline, highler levels should also ahve it. i.e. Inclusive caches.
       
        if(ins_mem->mem_type == WB){
            return I;
        }
        else if(ins_mem->mem_type == CLWB){
            return I; //This does ont effect right now. asplos-after
        }

        //Set atomic type of the cache blocks
       // printf("Messi miss 0x%lx cache %d level %d \n", ins_mem->addr_dmem, cache_id, level); //printf("Mesi Miss 2\n");
        line_cur = cache_cur->replaceLine(&ins_mem_old, ins_mem); 
        //Cacheline replacement logic.
        #ifdef DEBUG 
        if(ins_mem->atom_type != NON){
        //if(level >=0 && cache_id>=0){
            printf("[MESI]   Op: %d Type: %d Miss 0x%lx, cache level : %d and Cache Id: %d RMW: %s Memory: %s Epoch: %d \n", 
                ins_mem->mem_type , ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, (ins_mem->mem_op==2)?"yes":"no", (ins_mem->mem_type==0)?"read":"write", ins_mem->epoch_id);
        }
        #endif
                  
        if (line_cur->state) {
            cache_cur->incEvictCount();
            //delay[core_id] += inval_children(cache_cur, &ins_mem_old);

            //asplos-after; this must be dirty cacheline
            if(line_cur->state == M || (level==0 && line_cur->state == E && line_cur->dirty) || (level!=0 && line_cur->state == E)){  //              
                cache_cur->incWbCount();
                if (level == num_levels-1) {
                    delay[core_id] += inval_children(cache_cur, &ins_mem_old, core_id, timer+delay[core_id]); //Previous One
                    id_home = getHomeId(&ins_mem_old);

                    ins_mem_old.mem_type = WB; 

                    network.transmit(cache_id, id_home, cache_level[num_levels-1].block_size,  timer+delay[core_id]);
                    if (shared_llc) {
                        accessSharedCache(cache_id, id_home, &ins_mem_old, timer+delay[core_id], &state_tmp, core_id);
                    }
                    else {
                        accessDirectoryCache(cache_id, id_home, &ins_mem_old, timer+delay[core_id], &state_tmp, core_id);
                    }
                }
                else{                    
                    //Missing Piece //1. Error //if(cache_id>=0 && level==0) printf("Flush 0x%lx \n", ins_mem_old.addr_dmem);
                    
                    #ifdef DEBUG
                    if(cache_id>=0 && level==0 && ins_mem_old.atom_type !=NON){
                        printf("[ERROR] Ohhhhh boy : Flush 0x%lx from cache %d at level %d \n", ins_mem->addr_dmem, cache_id, level);
                    }
                    #endif
                    //Replacement Logic///////////////////////////////////////////////////////////////////////
                    //InsMem newMem = ins_mem_old;
                    //if(level==0 && (ins_mem_old.atom_type !=NON) ){ ///BIg Wrong Wronnnnnnnnnng 2019/04/15

                    //bool hasReleasePersisted = false;
                    if(level==0){ //Fixed
                        
                        #ifdef DEBUG
                        printf("Release Persistency in Replacement \n");
                        #endif
                        //delay[core_id] += releasePersist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline  
                        if(pmodel == BEPB){
                            //delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //BEP-on repacement
                            //Update 2019/05/16 - Delay 
                            //persist(cache_cur, &ins_mem_old, line_cur, core_id); //BEP-on repacement
                            persist(timer+delay[core_id], cache_cur, &ins_mem_old, line_cur, core_id,1); //BEP-on repacement

                            //asplos-after: need to count the conflicts seperately

                        }
                        else if(pmodel == RLSB){
                            if(ins_mem_old.atom_type !=NON){
                                //delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                                //persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                                persist(timer+delay[core_id], cache_cur, &ins_mem_old, line_cur, core_id,1); //Whichline

                                //asplos-after: need to count the conflicts seperately: conflicting and no-conflicting (meter)

                            }
                        }
                        else if(pmodel == FBRP){
                            if(ins_mem_old.atom_type !=NON){
                                //delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                                delay[core_id] += persist(timer+delay[core_id], cache_cur, &ins_mem_old, line_cur, core_id,1); //Whichline
                                //hasReleasePersisted = true;
                            }
                        }
                        else{
                            //NOP do nothing.
                        }


                        #ifdef DEBUG
                        printf("Release Persistency in Replacement End \n");
                        #endif

                        /*
                        if(pmodel != BEPB){ //Need to change 2019-04-15
                            if(ins_mem_old.atom_type !=NON) 
                                delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline  
                        }else{
                            //Check if epoch id is overwritten in replace - NO
                            //printf("Buffered Epoch Peristency on replace");
                            delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //BEP-on repacement
                        }
                        */                 
                    }                                       
                    //printf("E");
                    //printf("[INSMEM]   Op: %d Type: %d Miss 0x%lx, cache level : %d and Cache Id: %d RMW: %s Memory: %s Epoch: %d \n", 
                    //    ins_mem_old.mem_type , ins_mem_old.atom_type, ins_mem_old.addr_dmem, level, cache_id, (ins_mem_old.mem_op==2)?"yes":"no", (ins_mem_old.mem_type==0)?"read":"write", ins_mem_old.epoch_id);
                    assert(line_cur !=NULL);
                    assert(ins_mem !=NULL);   
                    assert(cache_cur!=NULL);
                    
                    //printf("E\n"); 

                    //ASPLOS-WB
                    bool is_natural_wb = false;  //Writeback
                    if(line_cur->state == M ){
                            is_natural_wb = true;
                    }                    

                    ins_mem_old.mem_type = WB;
                    ins_mem_old.lazy_wb = false;
                    //asplos-after:
                    //1. WB has to be fixed for NONB.                   
                        int before_delay = delay[core_id];
                   
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem_old, timer + delay[core_id]); 
                        
                        int after_delay = delay[core_id];
                        int delay_wb = after_delay - before_delay;

                        //ASPLOS-before
                        if(is_natural_wb){
                            cache_cur->natural_eviction_count++; //Naturally evicted. not in the critical path of RP.
                            cache_cur->natural_eviction_delay += delay_wb;
                        }

                        cache_cur->all_natural_eviction_count++;
                        cache_cur->all_natural_eviction_delay += delay_wb;

                        //natural writebacks of the no-persistency does not count.
                        if(level ==0){
                            if(pmodel == RLSB){ //Becuase write-backs happenes in the criticial path
                                delay[core_id] = before_delay; //No write-back cost. happenes outside the critical path of execution
                            }else if(pmodel != NONB){
                                if(core_id == 0){
                                    delay[core_id] = before_delay;  //Check this correctness
                                }
                            }
                        }

                        if(level == 0 && is_natural_wb){
                            cache_cur->write_back_count++;
                            cache_cur->write_back_delay+=delay_wb;

                            if(pmodel == RLSB || pmodel ==NONB){
                                cache_cur->noncritical_write_back_count++;     
                                cache_cur->noncritical_write_back_delay += delay_wb;
                            }
                            //This is also not in the critical path. For BEP evictions are lower.
                            else if(pmodel == BEPB || pmodel == FBRP){ //This should also belongs to noncritical since conflicting blokc
                                cache_cur->critical_write_back_count++;     
                                cache_cur->critical_write_back_delay += delay_wb;
                            }

                        }

                    //asplos-after: check this
                    line_cur->state = I;
                    line_cur->dirty = 0;
                    line_cur->atom_type = NON;

                    #ifdef ASPLOS_AFTER
                    line_cur->min_epoch_id = 0;
                    line_cur->max_epoch_id = 0;   
                    #endif                                       
                    //printf("B\n");
                    /////////////////////////////////////////////////////////////////////////////////////////// 
                }
                //Something is missing here. Intermediate modifided data ust be written-back to memory when the cacheline is replaced?.
                //Can a cacheline be replaced randomly in the intermediate stage. //FINAL ERROR : [MESI-INVAL] Release -> Invalidating address [0x19]/[0xca0e8] of cache [1] in level [1]                
            }
        }
                
        line_cur->timestamp = timer + delay[core_id];
        
        if (level != num_levels-1) {

            //This shold properly return the persist-buffer (PBUFF) dependencies. RD or WR. Just record the dependencies.

            line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                   cache_id*cache_level[level].share/cache_level[level+1].share, core_id, 
                                   ins_mem, timer);

            if(level==0 && ins_mem->mem_type==WR){
                //Persist Buffer Design: PBUFF
                line_cur->unique_wr_id = ins_mem->unique_wr_id;
            }

            // line_cur->state == M 3 modified
            if(level==0 && ins_mem->atom_type!=NON && ins_mem->mem_type==WR){ //in Modified State
                        line_cur->atom_type = ins_mem->atom_type; //Acquire or Release write.RMW

                        if(pmodel == RLSB){ //No need to NOP and BEP
                            SyncLine * new_line = cache_cur->addSyncLine(ins_mem); //Add word
                            if(new_line == NULL){
                                syncConflict(cache_cur, new_line, line_cur);
                                cache_cur->addSyncLine(ins_mem);
                            } //Dont erase the added line
                            assert(new_line !=NULL);    //Sync line  
                        } 
                        else if(pmodel == FBRP){ //No need to NOP and BEP
                            SyncLine * new_line = cache_cur->addSyncLine(ins_mem); //Add word
                            if(new_line == NULL){
                                syncConflict(cache_cur, new_line, line_cur);
                                cache_cur->addSyncLine(ins_mem);
                            } //Dont erase the added line
                            assert(new_line !=NULL);    //Sync line  
                        }                 
            }

            // line_cur->state == M 3 modified
            if(level==0 && ins_mem->mem_type==WR){ //in SModified State
                
                    if(line_cur->dirty){
                           if(ins_mem->epoch_id < line_cur->min_epoch_id){
                            line_cur->min_epoch_id = ins_mem->epoch_id;
                             //Release epoch id can be overwritten later.
                            }

                            if(ins_mem->epoch_id > line_cur->max_epoch_id){ //Latest epoch ID.
                            line_cur->max_epoch_id = ins_mem->epoch_id;
                            //check dirty bit?
                            }
                            
                    }else{
                            line_cur->min_epoch_id = ins_mem->epoch_id;
                            line_cur->max_epoch_id = ins_mem->epoch_id;
                            line_cur->dirty = 1;
                    } 
            }
        }
        else {

            //persist-buffer design (PBUFF).

            id_home = getHomeId(ins_mem);
            delay[core_id] += network.transmit(cache_id, id_home, 0,  timer+delay[core_id]);
            if (shared_llc) {
                delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
            }
            else {
                delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
            }
            line_cur->state = state_tmp;
            delay[core_id] += network.transmit(id_home, cache_id, cache_level[num_levels-1].block_size, timer+delay[core_id]);
        }  
        
        cache_cur->incMissCount();        
        cache_cur->unlockUp(ins_mem);

        #ifdef DEBUG
        if(ins_mem->atom_type != NON){
                printf("[MESI] Type: %d Miss 0x%lx, cache level : %d  and Cache Id: %d in State : %d and Line atom: %s RMW:%s \n", 
                    ins_mem->atom_type, ins_mem->addr_dmem, level , cache_id, line_cur->state, "No line", (ins_mem->mem_op==2)?"yes":"no");
        }
        #endif

        return line_cur->state;
        
    } //END of CACHE MISS
}


int System::fullBarrierFlush(int64_t clk_timer, Cache *cache_cur, int epoch_id, int req_core_id, int psrc){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer; //timing  int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;
    
    int level = cache_cur->getLevel(); 
    int persist_count =0;   

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {
            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {
                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);                
                //printf("B (%lu,%lu) 0x%lx 0x%lx %d \n", i,j, line_cur->tag, ins_mem.addr_dmem, line_cur->state);                
                if(line_cur != NULL && (line_cur->state==M) && line_cur->dirty){ //Need to think about E here
                    //printf("[Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    //    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     //without sync conflicts                    
                    if(line_cur->max_epoch_id < epoch_id){ //This  covers modified vs exclusive as well
                    //if(true){
                        //Cacheline needs to be written back
                        //uint64_t address_tag = line_cur->tag;
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON; 
                        ins_mem.lazy_wb = false;     //Precaution. 34 probelm.    
                        delay[core_id] = start_delay;

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;

                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 

    //same cores.
    delay[core_id] = delay_tmp[core_id];

   //aspolos-after
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
        } 

        //
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];

    //Same-core
    
    cache_cur->critical_conflict_persists += persist_count;
    cache_cur->write_back_count += persist_count;
    cache_cur->critical_write_back_count += persist_count;
    
    cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay
    cache_cur->write_back_delay += delay[core_id];
    cache_cur->critical_write_back_delay += delay[core_id];

    #ifdef DEBUG
    printf("Number of persists on FUll FLush : %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);  
    return delay[core_id];
}

//SyncMap conflicts syncConflict
int System::syncConflict(Cache * cache_cur, SyncLine * syncline, Line* line_call){
        
    //this functiona act as checkpointing. conflicts   
    int core_id = cache_cur->getId();
    int cache_id = core_id;
    printf("syncmap conflicts need to resolved %d \n", core_id);
    int level = 0;
    Line * line_cur;
    int timer =0;
    int persist_count =0;

    //sync conflicts
    cache_cur->critical_conflict_count++;
    //fullFlush(cache_cur, syncline, line_cur, syncline->epo)
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                 
                    if(true){ //Epoch Id < conflictigng epoch id - similar to full flush
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON; 
                        ins_mem.lazy_wb = false;                                             
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount(); //conflicting persist count - critical path.
                        persist_count++;
                        //sync conflict                       
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    }

    //same cores.
    cache_cur->sync_conflict_persists += persist_count;
    cache_cur->critical_conflict_persists += persist_count;
    cache_cur->write_back_count += persist_count;
    cache_cur->critical_write_back_count += persist_count;


    cache_cur->sync_conflict_persist_cycles += delay[core_id];
    cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay
    cache_cur->write_back_delay += delay[core_id];
    cache_cur->critical_write_back_delay += delay[core_id];

    //Ideally need to order synchronization variables.    
    cache_cur->initSyncMap(0);
    return 0; //Delay is added to the core delay with core id;
    
}

//non-Invalidating verision
//asplos-after: adding source bit
int System::bepochPersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer ; //timing int timer = 0;
    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;

    int level = cache_cur->getLevel(); 
    int persist_count =0;

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;          
                        delay[core_id] = start_delay;

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;

                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                                                
                        line_cur->dirty = 0;
                        //line_cur->state = I; //Stay Still
                    }
                }
        }
    } 
    //Check. Need to flush the calling line as well
    //printf("Here \n");
    delay[core_id] = delay_tmp[core_id];
            //Counts
    if(core_id == req_core_id){
        
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
        } 

        //
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        
        if(persist_count>0){
             cache_cur->inter_conflicts++;
        }      

        cache_cur->inter_allconflicts++;
        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}

//Buffered Epoch Persistence- Inv-version
//asplos-after: adding psrc bit
//Original BEP persistency model withot counters or PF. This was the testing algo.
//Without PF nut after originally changed version. 33, 34 run. 34 has different SB resutls to previous.
int System::epochPersistWithoutPF(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    
    //after-asplos
    //persist source: psrc: 0-same-block, 1-evctions/replacements, 2-inter-thread/writebacks

    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer ; //timing int timer = 0;
    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    int level = cache_cur->getLevel(); 
    int persist_count =0;

    //Check thi: method-local intialization
    
    uint64_t writes_per_epoch [20000];
    uint64_t is_epoch_checked [20000]; //This can be improved with last epoch checked. Without keeping 10000 size which is a waste.
    //int last_checked_epoch = 0; //this could be set global.

    for(int ei=0;ei<20000;ei++){
        writes_per_epoch[ei] = 0;
        is_epoch_checked[ei] = false;
    }

    int lowest_epoch_id = 20000;    
    int largest_max_epoch_id = 0;

    //asplos-after: build the epoch table from the cache?. Can be implemented as a linkedlist or hashmap. But not good.
    //asplos-after: we need to know what to flush. Building Epoch table
    int et_lowest_epoch_id = 20000; //lowest epoch id.
    // /int et_lowest_epoch_size = 0;
    
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) { 
            InsMem ins_mem;
            line_cur = cache_cur->directAccess(i,j,&ins_mem);
            if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //dirty-lines
                //asplos-after: only need the lowest epoch id and size.
                if(line_cur->max_epoch_id < conflict_epoch_id){    
                    if(line_cur->max_epoch_id < et_lowest_epoch_id) et_lowest_epoch_id = line_cur->max_epoch_id;
                    //lowest epoch first epoch to be flushed.
                }
            }
        }
    }

    //Proactive Flushing (PF): reduce the conflicts. allow lazy write-backs and coalecsing inside the cache-line.

    //Actual flush: min epoch id flushed = et_lowest_epoch_id
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        ins_mem.lazy_wb =false; 
                        //asplos-after: et_lowest_epoch_id: proactive_flushing
                        if(proactive_flushing && (line_cur->max_epoch_id == et_lowest_epoch_id))
                            ins_mem.lazy_wb =true; //new lazy write-backs: no delay is added.
                       

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        ins_mem.lazy_wb =false;
                        
                        cache_cur->incPersistCount();
                        persist_count++;                       

                        //asplos-after: counting the average number of cachelines per epoch that is flushing
                        if(line_cur->max_epoch_id >= 20000){
                            //Error
                            cerr<<"Error: Wrong home id\n";
                            printf("Errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
                            return -1;
                        }

                        if(!is_epoch_checked[line_cur->max_epoch_id ]) is_epoch_checked[line_cur->max_epoch_id]=true;
                        writes_per_epoch[line_cur->max_epoch_id]++;

                        if(line_cur->max_epoch_id < lowest_epoch_id) lowest_epoch_id = line_cur->max_epoch_id;
                        if(line_cur->max_epoch_id > largest_max_epoch_id) largest_max_epoch_id = line_cur->max_epoch_id;

                        //bug fixed. this must come after the above.
                        line_cur->dirty = 0;
                        line_cur->state = I; //changed the 
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;

                    }
                }
        }
    } 

    //Check. Need to flush the calling line as well
    //printf("Here \n");

    //asplos-after: distinct epochs that is flushed and the amount of writes in the lowest epoch id
    //1. average number of writes in the minimum epoch.
    uint64_t lowest_epoch_size = writes_per_epoch[lowest_epoch_id];//persists per lowest id epoch
    if(persist_count>0) cache_cur->nzero_conflicts++; // meaning that size of the lowest epoch id is not zero.
    uint64_t total_epochs_flushed = 0;    
    uint64_t largest_epoch_size = 0;

    for(int e = lowest_epoch_id;e<=largest_max_epoch_id;e++){
        if(is_epoch_checked[e]){
            if(writes_per_epoch[e]>0){
                total_epochs_flushed++;
                if(writes_per_epoch[e]>largest_epoch_size) largest_epoch_size=writes_per_epoch[e];
            }
        }
    }    
    //find epoch flushes per conflict
    cache_cur->lowest_epoch_size += lowest_epoch_size;
    if(lowest_epoch_size > cache_cur->largest_lowest_epoch_size) cache_cur->largest_lowest_epoch_size = lowest_epoch_size;
    cache_cur->num_epochs_flushed += (largest_max_epoch_id-lowest_epoch_id+1); //Totoal non-zeor epochs flushed //conflicting epochs.
    cache_cur->num_nzero_epochs_flushed += total_epochs_flushed; //Totoal non-zeor epochs flushed //conflicting epochs.
    if(largest_epoch_size > cache_cur->largest_epoch_size) cache_cur->largest_epoch_size = largest_epoch_size; //cacheline_size.
    
    //if(cache_cur->largest_epoch_id_flushed < largest_max_epoch_id) cache_cur->largest_epoch_id_flushed = largest_max_epoch_id;
    //then it is better to get the breakdown to intra-inter seperately. NEXT step:

    //Counts
    if(core_id == req_core_id){
        //asplos-after: psrc: 0-visibility conflics, 1-evictions         
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_E_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
                cache_cur->intra_evi_allconflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;
        } 

        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        //asplos-after: psrc: 2-writebacks
        if(persist_count>0){
             cache_cur->inter_conflicts++;
             if(line_call->state==M)cache_cur->inter_M_conflicts++;
             //else cache_cur->inter_E_conflicts++;

        }  
        cache_cur->inter_allconflicts++;
            if(line_call->state==M) cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        if(line_call->state == M) cache_cur->inter_M_persists += persist_count;
        //else cache_cur->inter_E_persists += persist_count;
        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency END): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}


//Arpit Model
int System::epochPersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    
    //after-asplos
    //persist source: psrc: 0-same-block, 1-evctions/replacements, 2-inter-thread/writebacks

    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;

    int timer = clk_timer; // timing int timer = 0;

    //timing
    //delay_tmp[core_id] = 0;
    //int start_delay = 0;

    //delay[core_id] = 0;   //Check this. Need to change.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;


    int level = cache_cur->getLevel(); 
    int persist_count = 0;
    
    //Check thi: method-local intialization
    //Proactive Flushing (PF): reduce the conflicts. allow lazy write-backs and coalecsing inside the cache-line.

    //Actual flush: min epoch id flushed = et_lowest_epoch_id
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        ins_mem.lazy_wb =false; 
                        //asplos-after: et_lowest_epoch_id: proactive_flushing
                        //if(proactive_flushing && (line_cur->max_epoch_id == et_lowest_epoch_id))
                        //    ins_mem.lazy_wb =true; //new lazy write-backs: no delay is added.                      

                        delay[core_id] = start_delay;
                        //dramsim2: add access delay.
                        //timing - delay[core_id] = start_delay;
                        //dramsim2: resolve llc conflicts. Go and check llc and push down cachelines. 

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        //ins_mem.lazy_wb =false;

                        //timing
                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        
                        cache_cur->incPersistCount();
                        persist_count++;      
                        cache_cur->all_persists++;                 

                        //asplos-after: counting the average number of cachelines per epoch that is flushing
                        //bug fixed. this must come after the above.
                        line_cur->dirty = 0;
                        line_cur->state = I; //changed the 
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;

                    }
                }
        }
    } 

    delay[core_id] = delay_tmp[core_id];

    //Counts
    if(core_id == req_core_id){
        //asplos-after: psrc: 0-visibility conflics, 1-evictions         
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_E_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
                cache_cur->intra_evi_allconflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;
        } 

        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        //asplos-after: psrc: 2-writebacks
        if(persist_count>0){
             cache_cur->inter_conflicts++;
             if(line_call->state==M)cache_cur->inter_M_conflicts++;
             //else cache_cur->inter_E_conflicts++;

        }  
        cache_cur->inter_allconflicts++;
            if(line_call->state==M) cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        if(line_call->state == M) cache_cur->inter_M_persists += persist_count;
        //else cache_cur->inter_E_persists += persist_count;
        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency END): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}

//Originally changed epochPersist in 32, 33, 34:
int System::epochPersistWithPF(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    
    //after-asplos
    //persist source: psrc: 0-same-block, 1-evctions/replacements, 2-inter-thread/writebacks

    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer;  //timing int timer = 0;

    //timing
    //delay_tmp[core_id] = 0;
    //int start_delay = 0;
    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;

    //delay_tmp[core_id] += delay[core_id];
    //start_delay = delay_tmp[core_id];

    int level = cache_cur->getLevel(); 
    int persist_count =0;

    //Check thi: method-local intialization
    
    uint64_t writes_per_epoch [40000];
    bool is_epoch_checked [40000]; //This can be improved with last epoch checked. Without keeping 10000 size which is a waste.
    //int last_checked_epoch = 0; //this could be set global.

    for(int ei=0;ei<40000;ei++){
        writes_per_epoch[ei] = 0;
        is_epoch_checked[ei] = false;
    }

    int lowest_epoch_id = 40000;    
    int largest_max_epoch_id = 0;

    //asplos-after: build the epoch table from the cache?. Can be implemented as a linkedlist or hashmap. But not good.
    //asplos-after: we need to know what to flush. Building Epoch table
    int et_lowest_epoch_id = 40000; //lowest epoch id.
    // /int et_lowest_epoch_size = 0;
    
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) { 
            InsMem ins_mem;
            line_cur = cache_cur->directAccess(i,j,&ins_mem);
            if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //dirty-lines
                //asplos-after: only need the lowest epoch id and size.
                if(line_cur->max_epoch_id < conflict_epoch_id){    
                    if(line_cur->max_epoch_id < et_lowest_epoch_id) et_lowest_epoch_id = line_cur->max_epoch_id;
                    //lowest epoch first epoch to be flushed.
                }
            }
        }
    }

    //Proactive Flushing (PF): reduce the conflicts. allow lazy write-backs and coalecsing inside the cache-line.

    //Actual flush: min epoch id flushed = et_lowest_epoch_id
    int pf_flag=0;

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        ins_mem.lazy_wb =false;
                        //asplos-after: et_lowest_epoch_id: proactive_flushing
                        if(proactive_flushing && (line_cur->max_epoch_id == et_lowest_epoch_id)){

                            if((pf_flag % 4) <= 1){ //%75 percent of the first epoch. reduce the overhead by 10%
                                ins_mem.lazy_wb =true; //new lazy write-backs: no delay is added.
                                cache_cur->all_pf_persists++;
                                //counters.                            
                                if (psrc==0) cache_cur->vis_pf_persists++; //Intra
                                else if (psrc==1)   cache_cur->evi_pf_persists++; //Intra
                                else if (psrc==2) cache_cur->inter_pf_persists++; //inter
                            }  
                            pf_flag++;                     

                        }                       

                        //timing- delay[core_id] = start_delay;
                        delay[core_id] = start_delay;
                        //involes writebacks.
                        
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        
                        ins_mem.lazy_wb =false;

                        //FUTURE NOTE: also we can do eager when epoch count>20000 or the limit. 

                        //timing-
                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        //delay_tmp[core_id] += delay[core_id];
                        //timer += return_delay[core_id];

                        //delay[core_id] = return_delay[core_id];
                        
                        cache_cur->incPersistCount();
                        persist_count++;   
                        cache_cur->all_persists++;                        

                        //delay_tmp[core_id] += delay[core_id];
                        //cache_cur->all_pf_persists++;

                        //asplos-after: counting the average number of cachelines per epoch that is flushing
                        if(line_cur->max_epoch_id >= 40000){
                            //Error
                            //cerr<<"Error: PF overflows: only for PF. This is not for standard BB or LRP.\n";
                            //printf("Errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
                            return -1;
                        }

                        if(!is_epoch_checked[line_cur->max_epoch_id ]) is_epoch_checked[line_cur->max_epoch_id]=true;
                        writes_per_epoch[line_cur->max_epoch_id]++;

                        if(line_cur->max_epoch_id < lowest_epoch_id) lowest_epoch_id = line_cur->max_epoch_id;
                        if(line_cur->max_epoch_id > largest_max_epoch_id) largest_max_epoch_id = line_cur->max_epoch_id;

                        //bug fixed. this must come after the above.
                        line_cur->dirty = 0;
                        line_cur->state = I; //changed the 
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;

                    }
                }
        }
    } 

    //Check. Need to flush the calling line as well
    //printf("Here \n");
    delay[core_id] = delay_tmp[core_id];

    //asplos-after: distinct epochs that is flushed and the amount of writes in the lowest epoch id
    //1. average number of writes in the minimum epoch.
    uint64_t lowest_epoch_size = writes_per_epoch[lowest_epoch_id];//persists per lowest id epoch
    if(persist_count>0) cache_cur->nzero_conflicts++; // meaning that size of the lowest epoch id is not zero.
    uint64_t total_epochs_flushed = 0;    
    uint64_t largest_epoch_size = 0;

    for(int e = lowest_epoch_id;e<=largest_max_epoch_id;e++){
        if(is_epoch_checked[e]){
            if(writes_per_epoch[e]>0){
                total_epochs_flushed++;
                if(writes_per_epoch[e]>largest_epoch_size) largest_epoch_size=writes_per_epoch[e];
            }
        }
    }    
    //find epoch flushes per conflict
    cache_cur->lowest_epoch_size += lowest_epoch_size;
    if(lowest_epoch_size > cache_cur->largest_lowest_epoch_size) cache_cur->largest_lowest_epoch_size = lowest_epoch_size;
    cache_cur->num_epochs_flushed += (largest_max_epoch_id-lowest_epoch_id+1); //Totoal non-zeor epochs flushed //conflicting epochs.
    cache_cur->num_nzero_epochs_flushed += total_epochs_flushed; //Totoal non-zeor epochs flushed //conflicting epochs.
    if(largest_epoch_size > cache_cur->largest_epoch_size) cache_cur->largest_epoch_size = largest_epoch_size; //cacheline_size.
    
    //if(cache_cur->largest_epoch_id_flushed < largest_max_epoch_id) cache_cur->largest_epoch_id_flushed = largest_max_epoch_id;
    //then it is better to get the breakdown to intra-inter seperately. NEXT step:

    //Counts
    if(core_id == req_core_id){
        //asplos-after: psrc: 0-visibility conflics, 1-evictions         
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_E_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
                cache_cur->intra_evi_allconflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;
        } 

        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        //asplos-after: psrc: 2-writebacks
        if(persist_count>0){
             cache_cur->inter_conflicts++;
             if(line_call->state==M)cache_cur->inter_M_conflicts++;
             //else cache_cur->inter_E_conflicts++;

        }  
        cache_cur->inter_allconflicts++;
            if(line_call->state==M) cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        if(line_call->state == M) cache_cur->inter_M_persists += persist_count;
        //else cache_cur->inter_E_persists += persist_count;
        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency END): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}

int System::epochPersistWithPFGen(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    
    //after-asplos
    //persist source: psrc: 0-same-block, 1-evctions/replacements, 2-inter-thread/writebacks

    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer;  //timing int timer = 0;

    //timing
    //delay_tmp[core_id] = 0;
    //int start_delay = 0;
    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;

    //delay_tmp[core_id] += delay[core_id];
    //start_delay = delay_tmp[core_id];

    int level = cache_cur->getLevel(); 
    int persist_count =0;

    //Check thi: method-local intialization
    
    /*final-asplos
    uint64_t writes_per_epoch [40000];
    bool is_epoch_checked [40000]; //This can be improved with last epoch checked. Without keeping 10000 size which is a waste.
    //int last_checked_epoch = 0; //this could be set global.

    for(int ei=0;ei<40000;ei++){
        writes_per_epoch[ei] = 0;
        is_epoch_checked[ei] = false;
    }

    int lowest_epoch_id = 40000;    
    int largest_max_epoch_id = 0;
    */ //final-asplos

    //asplos-after: build the epoch table from the cache?. Can be implemented as a linkedlist or hashmap. But not good.
    //asplos-after: we need to know what to flush. Building Epoch table
    int et_lowest_epoch_id = 40000; //lowest epoch id.
    // /int et_lowest_epoch_size = 0;
    
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) { 
            InsMem ins_mem;
            line_cur = cache_cur->directAccess(i,j,&ins_mem);
            if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //dirty-lines
                //asplos-after: only need the lowest epoch id and size.
                if(line_cur->max_epoch_id < conflict_epoch_id){    
                    if(line_cur->max_epoch_id < et_lowest_epoch_id) et_lowest_epoch_id = line_cur->max_epoch_id;
                    //lowest epoch first epoch to be flushed.
                }
            }
        }
    }

    //Proactive Flushing (PF): reduce the conflicts. allow lazy write-backs and coalecsing inside the cache-line.

    //Actual flush: min epoch id flushed = et_lowest_epoch_id
    int pf_flag=0;

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        ins_mem.lazy_wb =false;
                        //asplos-after: et_lowest_epoch_id: proactive_flushing
                        if(proactive_flushing && (line_cur->max_epoch_id == et_lowest_epoch_id)){

                            if((pf_flag % 4) <= 1){ //%75 percent of the first epoch. reduce the overhead by 10%
                                ins_mem.lazy_wb =true; //new lazy write-backs: no delay is added.
                                cache_cur->all_pf_persists++;
                                //counters.                            
                                if (psrc==0) cache_cur->vis_pf_persists++; //Intra
                                else if (psrc==1)   cache_cur->evi_pf_persists++; //Intra
                                else if (psrc==2) cache_cur->inter_pf_persists++; //inter
                            }  
                            pf_flag++;                     

                        }                       

                        //timing- delay[core_id] = start_delay;
                        delay[core_id] = start_delay;
                        //involes writebacks.
                        
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        
                        ins_mem.lazy_wb =false;

                        //FUTURE NOTE: also we can do eager when epoch count>20000 or the limit. 

                        //timing-
                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        //delay_tmp[core_id] += delay[core_id];
                        //timer += return_delay[core_id];

                        //delay[core_id] = return_delay[core_id];
                        
                        cache_cur->incPersistCount();
                        persist_count++;   
                        cache_cur->all_persists++;                        

                        //delay_tmp[core_id] += delay[core_id];
                        //cache_cur->all_pf_persists++;

                        //asplos-after: counting the average number of cachelines per epoch that is flushing
                        if(line_cur->max_epoch_id >= 40000){
                            //Error
                            //cerr<<"Error: PF overflows: only for PF. This is not for standard BB or LRP.\n";
                            //printf("Errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
                            //return -1;
                        }

                        /* final-asplos
                        if(!is_epoch_checked[line_cur->max_epoch_id ]) is_epoch_checked[line_cur->max_epoch_id]=true;
                        writes_per_epoch[line_cur->max_epoch_id]++;

                        if(line_cur->max_epoch_id < lowest_epoch_id) lowest_epoch_id = line_cur->max_epoch_id;
                        if(line_cur->max_epoch_id > largest_max_epoch_id) largest_max_epoch_id = line_cur->max_epoch_id;
                        */

                        //bug fixed. this must come after the above.
                        line_cur->dirty = 0;
                        line_cur->state = I; //changed the 
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;

                    }
                }
        }
    } 

    //Check. Need to flush the calling line as well
    //printf("Here \n");
    delay[core_id] = delay_tmp[core_id];

    //asplos-after: distinct epochs that is flushed and the amount of writes in the lowest epoch id
    //1. average number of writes in the minimum epoch.
    

    /*final asplos--------
    uint64_t lowest_epoch_size = writes_per_epoch[lowest_epoch_id];//persists per lowest id epoch
    if(persist_count>0) cache_cur->nzero_conflicts++; // meaning that size of the lowest epoch id is not zero.
    uint64_t total_epochs_flushed = 0;    
    uint64_t largest_epoch_size = 0;

    for(int e = lowest_epoch_id;e<=largest_max_epoch_id;e++){
        if(is_epoch_checked[e]){
            if(writes_per_epoch[e]>0){
                total_epochs_flushed++;
                if(writes_per_epoch[e]>largest_epoch_size) largest_epoch_size=writes_per_epoch[e];
            }
        }
    }    
    //find epoch flushes per conflict
    cache_cur->lowest_epoch_size += lowest_epoch_size;
    if(lowest_epoch_size > cache_cur->largest_lowest_epoch_size) cache_cur->largest_lowest_epoch_size = lowest_epoch_size;
    cache_cur->num_epochs_flushed += (largest_max_epoch_id-lowest_epoch_id+1); //Totoal non-zeor epochs flushed //conflicting epochs.
    cache_cur->num_nzero_epochs_flushed += total_epochs_flushed; //Totoal non-zeor epochs flushed //conflicting epochs.
    if(largest_epoch_size > cache_cur->largest_epoch_size) cache_cur->largest_epoch_size = largest_epoch_size; //cacheline_size.
    */ //final asplos-----------

    //if(cache_cur->largest_epoch_id_flushed < largest_max_epoch_id) cache_cur->largest_epoch_id_flushed = largest_max_epoch_id;
    //then it is better to get the breakdown to intra-inter seperately. NEXT step:

    //Counts
    if(core_id == req_core_id){
        //asplos-after: psrc: 0-visibility conflics, 1-evictions         
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_E_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
                cache_cur->intra_evi_allconflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;
        } 

        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        //asplos-after: psrc: 2-writebacks
        if(persist_count>0){
             cache_cur->inter_conflicts++;
             if(line_call->state==M)cache_cur->inter_M_conflicts++;
             //else cache_cur->inter_E_conflicts++;

        }  
        cache_cur->inter_allconflicts++;
            if(line_call->state==M) cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        if(line_call->state == M) cache_cur->inter_M_persists += persist_count;
        //else cache_cur->inter_E_persists += persist_count;
        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency END): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}



int System::fullFlush(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer;  //timing int timer = 0;


    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;
    
    int level = cache_cur->getLevel(); 
    int persist_count =0;   

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {
            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {
                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);                
                //printf("B (%lu,%lu) 0x%lx 0x%lx %d \n", i,j, line_cur->tag, ins_mem.addr_dmem, line_cur->state);                
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M || line_cur->state==E )){ //Need to think about E here
                    //printf("[Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    //    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     //without sync conflicts                    
                    if(true){ //This  covers modified vs exclusive as well
                    //if(true){
                        //Cacheline needs to be written back
                        //uint64_t address_tag = line_cur->tag;
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON; 
                        ins_mem.lazy_wb = false;     
                        delay[core_id] = start_delay; 

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;

                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 

    delay[core_id] = delay_tmp[core_id];

    //Counts
    if(core_id == req_core_id){

        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_S_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
               cache_cur->intra_evi_allconflicts++;
               if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;

        } 

        //New
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{ // Inter conflicts are not in the sritical path of the receiving core.
        
         if(persist_count>0){
            cache_cur->inter_conflicts++;
            if(line_call->state==M)cache_cur->inter_M_conflicts++;
            //else cache_cur->inter_E_conflicts++;
        }

        cache_cur->inter_allconflicts++;
            if(line_call->state==M)cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 
            if(line_call->state==M)cache_cur->inter_M_persists += persist_count;
            //else cache_cur->inter_E_conflicts++;

        //New
        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->noncritical_write_back_count += persist_count; //Is this true        
        cache_cur->noncritical_write_back_delay += delay[core_id];

        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];        
    }

    #ifdef DEBUG
    printf("Number of persists on FUll FLush : %d \n", persist_count); 
    #endif
        
    return delay[core_id];
}

//asplos-after: adding psrc bit. 
int System::releaseFlush(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer;  // timing int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;

    int level = cache_cur->getLevel(); 
    int persist_count =0;   

    //line_call is the release. 

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {
            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {
                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);                
                //printf("B (%lu,%lu) 0x%lx 0x%lx %d \n", i,j, line_cur->tag, ins_mem.addr_dmem, line_cur->state);                
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M || line_cur->state==E )){ //Need to think about E here
                    //M stads for dirty lines as well. this condition checks dirty==1
                    //printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                     //  i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     //without sync conflicts                    
                    if((line_cur->min_epoch_id <= rel_epoch_id) && line_cur->dirty){ //This  covers modified vs exclusive as well
                    //if(true){
                        //Cacheline needs to be written back
                        //printf();
                        #ifdef DEBUG
                        printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                        i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                        #endif
                        //uint64_t address_tag = line_cur->tag;
                        ins_mem.mem_type = WB;   //PutM
                        ins_mem.atom_type = NON;     
                        ins_mem.lazy_wb = false;  

                        delay[core_id] = start_delay;

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                      
                        cache_cur->incPersistCount(); //all
                        persist_count++;
                        cache_cur->all_persists++;   
                        //cache_cur->critical_conflict_persists++; //But not in the critical path of this core
                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id]; //delay[core_id] = delay_tmp[core_id];
                        //asplos-after counter bug found.
                        //cache_cur->write_back_count++;
                        //cache_cur->noncritical_write_back_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I; //Flushing
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;
                    }
                }
        }
    } 

    //printf("Here\n"); 
    //Counts
    delay[core_id] = delay_tmp[core_id];

    //For the relese delays
    cache_cur->last_rel_persists = persist_count; 
    cache_cur->last_rel_persists_cycles = delay[core_id];

    if(core_id == req_core_id){

        if(persist_count>0){
            cache_cur->intra_conflicts++;

            if(psrc==VIS){
               cache_cur->intra_vis_conflicts++;
            }else if(psrc == EVI){
               cache_cur->intra_evi_conflicts++;
               if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
            }
        }
        

        cache_cur->intra_allconflicts++; //conflistc+ persist count=0 conflicts
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
               cache_cur->intra_evi_allconflicts++;
               if(line_call->state == M) cache_cur->intra_evi_M_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
        } 

        //New
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        //+1 non-critical does not require. Its happening in the evicting thread.
        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];

        
    }else{ // Inter conflicts are not in the critical path of the receiving core.
        
        if(persist_count>0){
            cache_cur->inter_conflicts++;
            if(line_call->state == M) cache_cur->inter_M_conflicts++;
        }

        cache_cur->inter_allconflicts++; 
        if(line_call->state == M) cache_cur->inter_M_allconflicts++; 

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 
        if(line_call->state==M)cache_cur->inter_M_persists += persist_count;

        //New-
        cache_cur->write_back_count += persist_count+1; //1 is for release
        cache_cur->write_back_delay += delay[core_id]+1;

        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true, 1 is for the release
        cache_cur->noncritical_write_back_delay += delay[core_id]+ 1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];        
    }

    #ifdef DEBUG
    printf("Number of persists on release : %d \n", persist_count);    
    #endif
    
    return delay[core_id];
}


//asplos-after: adding psrc bit. 
int System::releaseFlushWithPF(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = clk_timer; //timing int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    delay_tmp[core_id] = delay[core_id];
    int start_delay = delay_tmp[core_id];
    delay_llc[core_id] = 0; return_delay[core_id] = 0; return_timer[core_id]= 0;

    int level = cache_cur->getLevel(); 
    int persist_count =0;   

   /*
    //line_call is the release. 
    uint64_t writes_per_epoch [20000];
    uint64_t is_epoch_checked [20000]; //This can be improved with last epoch checked. Without keeping 10000 size which is a waste.
    //int last_checked_epoch = 0; //this could be set global.

    for(int ei=0;ei<20000;ei++){
        writes_per_epoch[ei] = 0;
        is_epoch_checked[ei] = false;
    }
    */

    bool flipped = false;

    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {
            
            for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {
                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);                
                //printf("B (%lu,%lu) 0x%lx 0x%lx %d \n", i,j, line_cur->tag, ins_mem.addr_dmem, line_cur->state);                
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M || line_cur->state==E )){ //Need to think about E here
                    //M stads for dirty lines as well. this condition checks dirty==1
                    //printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                     //  i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     //without sync conflicts                    
                    if((line_cur->min_epoch_id <= rel_epoch_id) && line_cur->dirty){ //This  covers modified vs exclusive as well
                    //if(true){
                        //Cacheline needs to be written back
                        //printf();
                        #ifdef DEBUG
                        printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                        i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                        #endif
                        //uint64_t address_tag = line_cur->tag;
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;     
                        ins_mem.lazy_wb = false;

                        delay[core_id] = start_delay;

                        //option 1: randomly flush. Option 2: minimal epoch flush.
                        flipped = !flipped; //Interleaved Release Flush. NOTE: THIS SEEMS BUGGY.
                        if(!flipped){
                            ins_mem.lazy_wb = true;
                            cache_cur->all_pf_persists++;
                            //counters.                            
                            if (psrc==0) cache_cur->vis_pf_persists++; //Intra
                            else if (psrc==1)   cache_cur->evi_pf_persists++; //Intra
                            else if (psrc==2) cache_cur->inter_pf_persists++; //inter
                        }

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        //asplos-after: changes needed.
                        //1. average epochs. writes per epoch. lowest epoch and size.
                        //2. count inter- and -thra thread conflicts sepeartely.
                        //3. non conflicting evictions.
                        //4. count the number of release flushing. -> may be epoch boundaries.
                        ins_mem.lazy_wb = false;

                        cache_cur->incPersistCount(); //all
                        persist_count++;
                        cache_cur->all_persists++;   
                        //cache_cur->critical_conflict_persists++; //But not in the critical path of this core
                        delay_tmp[core_id] += delay[core_id]; //delay[core_id] = start_delay;
                        timer += return_delay[core_id];       //delay[core_id] = delay_tmp[core_id];
                        //asplos-after counter bug
                        //cache_cur->write_back_count++;
                        //cache_cur->noncritical_write_back_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I; //Flushing
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;
                    }
                }
        }
    } 

    //printf("Here\n"); 
    delay[core_id] = delay_tmp[core_id];
    //For the relese delays
    cache_cur->last_rel_persists = persist_count; 
    cache_cur->last_rel_persists_cycles = delay[core_id];

    //Counts
    if(core_id == req_core_id){

        if(persist_count>0){
            cache_cur->intra_conflicts++;

            if(psrc==VIS){
               cache_cur->intra_vis_conflicts++;
            }else if(psrc == EVI){
               cache_cur->intra_evi_conflicts++;
               if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
            }
        }
        

        cache_cur->intra_allconflicts++; //conflistc+ persist count=0 conflicts
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
               cache_cur->intra_evi_allconflicts++;
               if(line_call->state == M) cache_cur->intra_evi_M_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
        } 

        //New
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        //+1 non-critical does not require. Its happening in the evicting thread.
        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];

        
    }else{ // Inter conflicts are not in the critical path of the receiving core.
        
        if(persist_count>0){
            cache_cur->inter_conflicts++;
            if(line_call->state == M) cache_cur->inter_M_conflicts++;
        }

        cache_cur->inter_allconflicts++; 
        if(line_call->state == M) cache_cur->inter_M_allconflicts++; 

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 
        if(line_call->state==M)cache_cur->inter_M_persists += persist_count;

        //New-
        cache_cur->write_back_count += persist_count+1; //1 is for release
        cache_cur->write_back_delay += delay[core_id]+1;

        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true, 1 is for the release
        cache_cur->noncritical_write_back_delay += delay[core_id]+ 1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];        
    }

    #ifdef DEBUG
    printf("Number of persists on release : %d \n", persist_count);    
    #endif
    
    return delay[core_id];
}

//Mist return delay
//asplos after: introducing an extra flag to identify , i,e, psource ; 0-same-block, 1-evictions, 2-write-backs (inter)
int System::persist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc){
    //Persistence models

    //ASPLOS-AFTER
    //printf("Persist Source: %d \n", psrc);
    if(pmodel == NONB){
        //Do nothing - no persist;
        return 0;
    }
    //Beffired Epoch Persistency
    else if(pmodel == BEPB){
        //cache_cur->printCache(); 
        #ifdef DEBUG
        printf("\n[BEP] Start Buffered Epoch Persistency\n");
        #endif
        int delay_tmp=0;
        if(!proactive_flushing)
            delay_tmp = epochPersist(clk_timer, cache_cur, ins_mem, line_cur, req_core_id, psrc); //need to add calling cacheline
        else 
            delay_tmp = epochPersistWithPFGen(clk_timer, cache_cur, ins_mem, line_cur, req_core_id, psrc); //need to add calling cacheline


        #ifdef DEBUG
        printf("[BEP] End Buffered Epoch Persistency Persist [delay : %d]\n", delay_tmp);  
        #endif
        //cache_cur->printCache(); 
        return delay_tmp;
    }
    //Release Persistecy and Full FLush
    else{
       return releasePersist(clk_timer, cache_cur, ins_mem, line_cur, req_core_id, psrc);
    }
}


/*
int System::persist(Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id){
    //Persistence models

    if(pmodel == NONB){
        //Do nothing - no persist;
        return 0;
    }
    //Beffired Epoch Persistency
    else if(pmodel == BEPB){
        //cache_cur->printCache(); 
        #ifdef DEBUG
        printf("\n[BEP] Start Buffered Epoch Persistency\n");
        #endif

        int delay_tmp = epochPersist(cache_cur, ins_mem, line_cur, req_core_id); //need to add calling cacheline


        #ifdef DEBUG
        printf("[BEP] End Buffered Epoch Persistency Persist [delay : %d]\n", delay_tmp);  
        #endif
        //cache_cur->printCache(); 
        return delay_tmp;
    }
    //Release Persistecy and Full FLush
    else{
       return releasePersist(cache_cur, ins_mem, line_cur, req_core_id);
    }
}
*/


int System::releasePersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc){

    int delay_tmp = 0;
    
    #ifdef DEBUG
    cache_cur->printSyncMap();
    #endif

    SyncLine *syncline = cache_cur->searchSyncMap(ins_mem->addr_dmem); //Caclculate constant delay here.
    
    cache_cur->cnt_sm_rel_lookups++;
    if(psrc==0){
        cache_cur->cnt_sm_rel_intra_lookups++;
    } else cache_cur->cnt_sm_rel_inter_lookups++;
    //assert(syncline != NULL); - Mahesh 2019/04/15 Becuase of exclusive state
    if(syncline !=NULL){

        cache_cur->cnt_sm_rel_succ_lookups++;
        #ifdef DEBUG
        printf("[Persist] Match found 0x%lx Tag- 0x%lx of cache: %d epoch : %d\n", 
            syncline->address, syncline->tag, cache_cur->getId(), syncline->epoch_id);
        #endif
            //cache_cur->printCache();  //Cache L1 Print    
    }else{
        
        return 0;
    }
      
    int rel_epoch_id = syncline->epoch_id;

    #ifdef DEBUG
    printf("----------------------------------- Start Persist ---------------------------------------\n");
    #endif
    if(pmodel == RLSB){
        //printf("Release Persistence \n");
        cache_cur->last_rel_conflicts = 0;
        cache_cur->last_rel_persists = 0; 
        cache_cur->last_rel_persists_cycles = 0;        

        if(!proactive_flushing)
            delay_tmp = releaseFlush(clk_timer, cache_cur, syncline, line_cur, rel_epoch_id, req_core_id, psrc);
        else 
            delay_tmp = releaseFlushWithPF(clk_timer, cache_cur, syncline, line_cur, rel_epoch_id, req_core_id, psrc);
        
        //piggy-backing.
        //Its also ok to directly take delay_tmp

        int updated_delay_tmp = 0 ;
        if(cache_cur->last_rel_persists > 0){
            int avg_delay = cache_cur->last_rel_persists_cycles/cache_cur->last_rel_persists;
            if(avg_delay > (signed) cache_cur->last_rel_persists){
                updated_delay_tmp = 2*avg_delay;
            }
            else{
                updated_delay_tmp = cache_cur->last_rel_persists + avg_delay;
            }

            delay_tmp = updated_delay_tmp;
        }

        cache_cur->incPersistDelay(delay_tmp); //Counters
        
        cache_cur->last_rel_conflicts = 0;
        cache_cur->last_rel_persists = 0; 
        cache_cur->last_rel_persists_cycles = 0; 

        //simple solution is : delay_tmp =  delay_tmp/2;
    }
    //Bug-fixed
    else if(pmodel == FBRP){ //Epoch PErsistency
        //printf("Full barrier Release Persistence \n");
        delay_tmp = releaseFlush(clk_timer, cache_cur, syncline, line_cur, rel_epoch_id, req_core_id, psrc); 
        cache_cur->incPersistDelay(delay_tmp); //Counters
        //delay_tmp =  delay_tmp/2;
    }
    else if(pmodel == FLLB){
        //printf("Epoch Persistence \n");
        delay_tmp = fullFlush(clk_timer, cache_cur, syncline, line_cur, rel_epoch_id, req_core_id, psrc); //Full barrier
        cache_cur->incPersistDelay(delay_tmp); //Counters
        //delay_tmp =  delay_tmp/2;
    }
    else
    {
        //printf("No Persistence \n");
        //NO need to check syncmap
        delay_tmp = 0;
    }

    //printf("Here Persistence \n");
    
    #ifdef DEBUG
    printf("Persistency Total Delay : %d \n", delay_tmp);  
    printf("-----------------------------------  End  Persist ---------------------------------------\n");    
    #endif       
    //Resove conflicts and flush cachelines
    //Can do writebacks or comepletly new flushing. M->I. M->S   
    //cache_cur->printCache();  //Cache L1 Print//Can do only for persistent data. //Final: Send a write back message to lower. //Wait for Ack.
    cache_cur->deleteFromSyncMap(ins_mem->addr_dmem);

    #ifdef LOWER_EPOCH_FLUSH
      cache_cur->deleteLowerFromSyncMap(rel_epoch_id);//Delete all lower epoch ids from sync registry;
    #endif  

    #ifdef DEBUG 
    cache_cur->printSyncMap();
    #endif
    
    //printf("Q\n");
    return delay_tmp;
}

// This function propagates down shared state
int System::share(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line*  line_cur;
    
    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();

        if (line_cur != NULL && (line_cur->state == M || line_cur->state == E)) {
            
             for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = share(cache_cur->child[i], ins_mem, core_id, clk_timer);
                if (delay_tmp > delay_max) {
                    delay_max = delay_tmp;
                }
            }
            //Original line_cur->state = S;            
            //need to change here and Line to support release. Need to handle commit and invalidate release.            
            #ifdef DEBUG
            if(line_cur->atom_type!=NON){                
                printf("[MESI-SHARE] Atom- %d, State- %d -> Downgrading address [0x%lx]/[0x%lx] of cache [%d] in level [%d], Requesting core:%d\n", 
                        line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel(), core_id);
            }
            #endif
            //FLUSH INVOCATION - LAZY RELEASE (also check replace and invalidations)
            //if(cache_cur->getLevel()==0 && (line_cur->atom_type == RELEASE || line_cur->atom_type == ACQUIRE ||line_cur->atom_type == FULL)){ 
            
            if(cache_cur->getLevel()==0){ 
            //aspolos-after
            //if(cache_cur->getLevel()==0 && line_cur->state == M){  //or M state
                //Eviction implement: line_cur->atom_type == RELEASE -> DO not have this information
                //Before Write-back.
                
                #ifdef DEBUG
                    printf("[Share] Release Persist Starting.... and effected core \n");                              
                    printf("[MESI-SHARE] Atom- %d, State- %d -> Downgrading address [0x%lx]/[0x%lx] of cache [%d] in level [%d], Requesting core:%d\n", 
                            line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel(), core_id);
                #endif

                //delay += releasePersist(cache_cur, ins_mem, line_cur,core_id); // need both Acquire and Releas
                if(pmodel == BEPB) delay += persist(clk_timer, cache_cur, ins_mem, line_cur,core_id,2); // need both Acquire and Release // Persistence Model
                else if(line_cur->atom_type !=NON) delay += persist(clk_timer, cache_cur, ins_mem, line_cur,core_id,2);
                
                #ifdef DEBUG
                printf("[Share-Finish] Release Persist Ending.... \n");
                #endif

                cache_cur->shares++;
                if(line_cur->state ==M) cache_cur->shares_M++;
                else cache_cur->shares_E++;

                //PBUFF
                //persist-buffer----------------------------------------------------------------------
                    ins_mem->return_unique_wr_id = line_cur->unique_wr_id; //THIS IS NOT SURE.
                    ins_mem->response_core_id = cache_cur->getId();
                    ins_mem->return_atomic_type = line_cur->atom_type;

                    ins_mem->is_return_owned = true;
                    ins_mem->is_return_dep = false; //CHECK THIS. THIS MUST BE ENABLED IN THE DIRECTORY.
                    ins_mem->is_return_dep_barrier = (line_cur->atom_type != NON)? true: false; //ANY BARRIER.
                //persist-buffer code for dependencies- CHECK.
                //SIMPLE TECHNIQUE: FLUSH ALL OF THE PERSISTS FROM THE BUFFER. NO DEPENDENCY TRACKING.
            }
            //Need more care here to write-back the release andacquires.
            line_cur->state = S; //Check this
            //Update atomic type
            line_cur->atom_type = NON;     
            line_cur->dirty = 0;

            #ifdef ASPLOS_AFTER
            line_cur->min_epoch_id = 0;
            line_cur->max_epoch_id = 0;   
            #endif
           //
            delay += delay_max;        

        }else{
            //if(line_cur == NULL) printf("Line Cure NULL\n");
        }
        //cache_cur->printCache();
        cache_cur->unlockDown(ins_mem);
    }
    return delay;
}

// This function propagates down shared state starting from childern nodes
int System::share_children(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = share(cache_cur->child[i], ins_mem, core_id, clk_timer);
            if (delay_tmp > delay_max) {
                delay_max = delay_tmp;
            }
        }
        delay += delay_max;
    }
    return delay;
}



// This function propagates down invalidate state
int System::inval(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line* line_cur;

    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();
        if (line_cur != NULL) {
            
            for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = inval(cache_cur->child[i], ins_mem, core_id, clk_timer);
                if (delay_tmp > delay_max) {
                    delay_max = delay_tmp;
                }
            }
            //need to change here and Line to support release. Need to handle commit and invalidate release.            
            #ifdef DEBUG
            if(line_cur->atom_type !=NON){
            printf("\n[MESI-INVAL] Atom- %d, State- %d, Invalidating address [0x%lx]/[0x%lx] of cache [%d] in level [%d]\n", 
                line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel());
            } 
            #endif

            //if(cache_cur->getLevel()==0 && (line_cur->atom_type == RELEASE || line_cur->atom_type == ACQUIRE || line_cur->atom_type == FULL)){ 
            if(cache_cur->getLevel()==0){ 

                //Eviction implement: line_cur->atom_type == RELEASE -> DO not have this information
                //Before Write-back.
                if((line_cur->state == M || line_cur->state == E)){
                //asplos-after
                //if((line_cur->state == M || line_cur->state == E) && line_cur->state == M){
                    
                    #ifdef DEBUG
                    printf("[Inval] Release Persist Starting.... \n");                    
                    printf("\n[MESI-INVAL] Atom- %d, State- %d, Invalidating address [0x%lx]/[0x%lx] of cache [%d] in level [%d]\n", 
                        line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel());
                    
                    #endif
                    //delay += releasePersist(cache_cur, ins_mem, line_cur, core_id); // need both Acquire and Releas
                    if (pmodel == BEPB) delay += persist(clk_timer, cache_cur, ins_mem, line_cur, core_id,2); // need both Acquire and Release
                    else if (line_cur->atom_type != NON) delay += persist(clk_timer, cache_cur, ins_mem, line_cur, core_id,2); 
                   
                    #ifdef DEBUG
                    printf("[Inval-Finish] Release Persist Ending.... \n");
                    #endif

                    cache_cur->invals++;
                    if(line_cur->state ==M) cache_cur->invals_M++;
                    else cache_cur->invals_E++;

                    //persist-buffer----------------------------------------------------------------------
                    ins_mem->return_unique_wr_id = line_cur->unique_wr_id; //THIS IS NOT SURE.
                    ins_mem->response_core_id = cache_cur->getId();
                    ins_mem->return_atomic_type = line_cur->atom_type;

                    ins_mem->is_return_owned = true;
                    ins_mem->is_return_dep = false; //CHECK THIS. THIS MUST BE ENABLED IN THE DIRECTORY.
                    ins_mem->is_return_dep_barrier = (line_cur->atom_type != NON)? true: false; //ANY BARRIER.
                    //persist-buffer code for dependencies- CHECK.

                    //SIMPLE TECHNIQUE: FLUSH ALL OF THE PERSISTS FROM THE BUFFER. NO DEPENDENCY TRACKING.

                }//Delay of invalidation and shar is added to the cycles by coherence itself.
                //No need to write-back                 
            }
                         
            line_cur->state = I;
            //update_atomic
            line_cur->atom_type = NON; // If removed Invalidation messages will be printed
            line_cur->dirty = 0;
            
            //only need for level 0
            #ifdef ASPLOS_AFTER
            line_cur->min_epoch_id = 0;
            line_cur->max_epoch_id = 0;   
            #endif

            delay += delay_max;
        }
        cache_cur->unlockDown(ins_mem);
    }
    return delay;
}

// This function propagates down invalidate state starting from children nodes
int System::inval_children(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = inval(cache_cur->child[i], ins_mem, core_id, clk_timer);
            if (delay_tmp > delay_max) {
                delay_max = delay_tmp;
            }
        }
        delay += delay_max;
    }
    return delay;
}



//Access inclusive directory cache without backup directory
int System::accessDirectoryCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state, int core_id)
{

    int level;
    level = num_levels;
    level = level;
    if (!directory_cache_init_done[home_id]) {
        init_directories(home_id); 
    }
    int delay = 0, delay_temp =0, delay_pipe =0, delay_max = 0;
    InsMem ins_mem_old;
    IntSet::iterator pos;
    Line* line_cur;
    home_stat[home_id] = 1;    
    assert(directory_cache[home_id] != NULL);
    directory_cache[home_id]->lockUp(ins_mem);
    line_cur = directory_cache[home_id]->accessLine(ins_mem);
    directory_cache[home_id]->incInsCount();
    delay += directory_cache[home_id]->getAccessTime();
    //Directory cache miss
    //printf("Checkpoint 2 address 0x%lx %d %d \n", ins_mem->addr_dmem, ins_mem->mem_type, ins_mem->atom_type);    

    if ((line_cur == NULL) && (ins_mem->mem_type != WB)) {

        #ifdef DEBUG
        if(ins_mem->atom_type != NON){
            printf("[MESI-D]  Op:%d Type: %d Miss 0x%lx, cache level : %d and Cache Id: %d \n", ins_mem->mem_type, ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id);
            if(ins_mem->atom_type == RELEASE && ins_mem->mem_type==1) {
                //line_cur->atom_type = ins_mem->atom_type;
            }
        }
        #endif

        line_cur = directory_cache[home_id]->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            directory_cache[home_id]->incEvictCount();
            if (line_cur->state == M || line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], &ins_mem_old, core_id,  timer+delay+delay_temp);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }   
            //Broadcast
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for (int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], &ins_mem_old, core_id, timer+delay+delay_temp);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 

        }
        if (ins_mem->mem_type == WR) {
            line_cur->state = M;
        }
        else {
            line_cur->state = E;
        }

        directory_cache[home_id]->incMissCount();
        line_cur->sharer_set.clear();
        line_cur->sharer_set.insert(cache_id);
        delay += dram.access(ins_mem);
    }  
    //Directoy cache hit 
    else {
        
        #ifdef DEBUG
        if(ins_mem->atom_type != NON){
            printf("[MESI-D]   Op:%d Type: %d Hits 0x%lx, cache level : %d and Cache Id: %d in State : %d and Line atom: %d \n",ins_mem->mem_type, ins_mem->atom_type, ins_mem->addr_dmem, level, cache_id, line_cur->state, line_cur->atom_type);
            if(ins_mem->atom_type == RELEASE && ins_mem->mem_type==1) {
                line_cur->atom_type = ins_mem->atom_type;
            }
        }
        #endif

        if (ins_mem->mem_type == WR) {
            if (line_cur->state == M || line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem, core_id, timer+delay+delay_temp);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
                delay += dram.access(ins_mem);
            }
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for (int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], ins_mem, core_id, timer+delay+delay_temp);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
                delay += dram.access(ins_mem);
            } 

            //Update atomic types
            line_cur->atom_type = ins_mem->atom_type;
            
            line_cur->state = M;
            line_cur->sharer_set.clear();
            line_cur->sharer_set.insert(cache_id);
        }
        else if (ins_mem->mem_type == RD) {
             if (line_cur->state == M || line_cur->state == E) {
                //printf("++++++++++++++++++ M to S when Read Operations +++++++++++++++ home-id: %d and receiver node: %d state: %d \n", home_id, *line_cur->sharer_set.begin(), line_cur->state);

                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay); //Check here.
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id, timer+delay); //This handles Hit-Read problem of LLC 
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                line_cur->state = S;
            }
            else if (line_cur->state == S) {
                delay += dram.access(ins_mem);
                if ((protocol_type == LIMITED_PTR) && ((int)line_cur->sharer_set.size() >= max_num_sharers)) {
                    line_cur->state = B;
                }
                else {
                    line_cur->state = S;
                }

            } 
            else if (line_cur->state == B) {
                delay += dram.access(ins_mem);
                line_cur->state = B;
            } 
            line_cur->sharer_set.insert(cache_id);
        } 
        else if(ins_mem->mem_type == WB){ //Write-back
                //printf("writeback 0x%lx \n",ins_mem->addr_dmem);
                delay += dram.access(ins_mem);
                line_cur->state = I;
                line_cur->sharer_set.clear();
        }

        //after-asplos: add new clwb operations. 
        //next lazy and nlazy operations.
        else if(ins_mem->mem_type == CLWB){ //Write-back
                //printf("writeback 0x%lx \n",ins_mem->addr_dmem);
                delay += dram.access(ins_mem);
                //line_cur->state = I;
                //line_cur->sharer_set.clear(); //Not necessary
        }        

        else {
            line_cur->state = I;
            line_cur->sharer_set.clear();
            dram.access(ins_mem); //Where is memory access latencies?. So is replication?. //Need to change.
        }
    }
    if(line_cur->state == B) {
        (*state) = S;
    }
    else {
        (*state) = line_cur->state;
    }
    line_cur->timestamp = timer;
    directory_cache[home_id]->unlockUp(ins_mem);
    return delay;
}

//Access distributed shared LLC with embedded directory cache
int System::accessSharedCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state, int core_id)
{
    int level;
    level = num_levels;
    level = level;
    if (!directory_cache_init_done[home_id]) {
        init_directories(home_id); 
    }
    int delay = 0, delay_temp =0, delay_pipe =0, delay_max = 0;
    InsMem ins_mem_old;
    IntSet::iterator pos;
    Line* line_cur;
    home_stat[home_id] = 1;
    directory_cache[home_id]->lockUp(ins_mem);
    line_cur = directory_cache[home_id]->accessLine(ins_mem);
    directory_cache[home_id]->incInsCount();
    delay += directory_cache[home_id]->getAccessTime();

    //Shard llc miss: //Persist-Buffer: Note: need to avoid replacing line updating request side PBUFF dependencies
    if ((line_cur == NULL) && (ins_mem->mem_type != WB)) {
        line_cur = directory_cache[home_id]->replaceLine(&ins_mem_old, ins_mem);

        //check for persist-buffer: NO DEPENDENCY HERE. BE CAREFUL
        if (line_cur->state) {
            directory_cache[home_id]->incEvictCount();
            if (line_cur->state == M) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                //dram.access(&ins_mem_old); - DRAMOut
                dram.access(timer+delay, &ins_mem_old);

            }
            else if (line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id , 0, timer+delay);
                //dram.access(&ins_mem_old); - DRAMOut
                dram.access(timer+delay, &ins_mem_old);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], &ins_mem_old, core_id, timer+delay);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }
            //Broadcast
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], &ins_mem_old, core_id, timer+delay);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 

            //persist buffer
            line_cur->is_previously_owned = false;
        }

        if(ins_mem->mem_type == WR) {
            line_cur->state = M;
            //persist-buffer (PBUFF)
            line_cur->is_previously_owned = true;
        }
        else {
            line_cur->state = E; //Read.
        }

        directory_cache[home_id]->incMissCount();
        line_cur->sharer_set.clear();
        line_cur->sharer_set.insert(cache_id);
        //delay += dram.access(ins_mem); //Read data from DRAM
        delay += dram.access(timer+delay, ins_mem); // DRAMOut //Read data from DRAM
    }   
    //Shard llc hit (persist-buffer)
    else {
        if (ins_mem->mem_type == WR) {

            //asplos-after: This does not writeback data. need to change. 

            if ((line_cur->state == M) || (line_cur->state == E)) {

                ins_mem->is_return_owned = false;  //Persist-buffer code              
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                
                //asplos-after: 
                if(pmodel != NONB){
                    //Do something: (1) this is important when non-invalidating write is used.
                }else{ //NOP-No need to do anything. //delay += dram.access(ins_mem); 
                }

                //PBUFF: only for persist-buffer dependencies--------------
                if(ins_mem->is_return_owned){
                    //no need to set the owner. new owner will take off;
                    line_cur->previous_owner = ins_mem->response_core_id;
                    line_cur->previous_unique_wr_id = ins_mem->return_unique_wr_id;
                    line_cur->is_previous_barrier = ins_mem->is_return_dep_barrier; //Barrier Identification
                    ins_mem->is_return_dep = true;
                }
                else ins_mem->is_return_dep = false;

                line_cur->is_previously_owned = true;
                //--------------------------------------------------
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem, core_id, timer+delay);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if(delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;

                //persist-buffer-----------------------------------------------
                //There can be shared variables that previously owned.
                if(line_cur->is_previously_owned){
                    ins_mem->is_return_dep = true;
                    ins_mem->return_unique_wr_id = line_cur->previous_unique_wr_id;
                    ins_mem->response_core_id = line_cur->previous_owner; //previous owner is saved.
                    ins_mem->is_return_dep_barrier = line_cur->is_previous_barrier; //Include last owner barrier.
                }
                else ins_mem->is_return_dep = false;
                //-------------------------------------------------------------
            }

            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], ins_mem, core_id, timer+delay);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if(delay_temp > delay_max) 
                    {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 
            else if(line_cur->state == V) {
                //Do nothing
            }
            line_cur->state = M;
            line_cur->sharer_set.clear();
            line_cur->sharer_set.insert(cache_id);
        }
        
        else if (ins_mem->mem_type == RD) {

             if ((line_cur->state == M) || (line_cur->state == E)) {

                ins_mem->is_return_owned = false; 
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id, timer+delay);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                line_cur->state = S;

                //asplos-after: if in M state, need to writebackdata  
                if(pmodel != NONB){
                    //Do something
                }else{
                    //NOP: M->S transition.
                    //delay += dram.access(ins_mem); -DRAMOut
                    delay += dram.access(timer+delay, ins_mem);
                }      

                //PBUFF: only for persist-buffer dependencies--------------
                if(ins_mem->is_return_owned){
                    //no need to set the owner. new owner will take off;
                    line_cur->previous_owner = ins_mem->response_core_id;
                    line_cur->previous_unique_wr_id = ins_mem->return_unique_wr_id;
                    line_cur->is_previous_barrier = ins_mem->is_return_dep_barrier; //Barrier Identification
                    ins_mem->is_return_dep = true;
                }
                else ins_mem->is_return_dep = false;
                line_cur->is_previously_owned = true; //previously owned by someone else.
                //--------------------------------------------------
            }

            //This must be handled in the persist buffers.
            else if (line_cur->state == S) {

                //This is the only change with direcotry access: delay += dram.access(ins_mem);
                if ((protocol_type == LIMITED_PTR) && ((int)line_cur->sharer_set.size() >= max_num_sharers)) {
                    line_cur->state = B;
                }
                else {
                    line_cur->state = S;
                }

                //persist-buffer-----------------------------------------------
                //There can be shared variables that previously owned.
                if(line_cur->is_previously_owned){
                    ins_mem->is_return_dep = true;
                    ins_mem->return_unique_wr_id = line_cur->previous_unique_wr_id;
                    ins_mem->response_core_id = line_cur->previous_owner; //previous owner is saved.
                    ins_mem->is_return_dep_barrier = line_cur->is_previous_barrier; //Include last owner barrier.
                }else{
                   ins_mem->is_return_dep = false; 
                }
                //-------------------------------------------------------------
            } 

            else if (line_cur->state == B) {
                line_cur->state = B;
            } 
            else if (line_cur->state == V) {
                line_cur->state = E;
            } 
            line_cur->sharer_set.insert(cache_id);
        }

        //asplos-after: WB also needs a variant for lazy and nlazy.
        else if((ins_mem->mem_type == WB)){
            //printf("writeback 0x%lx \n",ins_mem->addr_dmem);
            return_delay[core_id] = this->delay[core_id];
            return_timer[core_id] = timer+delay;

            //asplos-after: nonp dont need to writeback data.
            if(pmodel != NONB){
                //delay += dram.access(ins_mem); - DRAMOut
                delay += dram.access(timer+delay, ins_mem);

                line_cur->state = I; //Remark:This need to be changed with. No need to invalidate.
            }                 
            else{
               //dram.access(ins_mem); 
                dram.access(timer+delay, ins_mem); 
            } 

            //line_cur->state = I;
            line_cur->sharer_set.clear();            
        }//Writeback 

        //after-asplos: add new clwb operations. //next lazy and nlazy operations.
        else if(ins_mem->mem_type == CLWB){ //Write-back

            return_delay[core_id] = this->delay[core_id];
            return_timer[core_id] = timer+delay;
                //Non-Invalidation writenbacks
            if(pmodel != NONB){ 
                delay += dram.access(timer+delay, ins_mem);
            }
            else {
                //dram.access(ins_mem); //delay is not added.
                dram.access(timer+delay, ins_mem); //delay is not added. - DRAMOut
            }
                //line_cur->state = I;
                //line_cur->sharer_set.clear(); //Not necessary
        }

        else {
            line_cur->state = V;
            line_cur->sharer_set.clear();
            //dram.access(ins_mem); -DRAMOut
            dram.access(timer+delay, ins_mem);
        }
    }
    if (line_cur->state == B) {
        (*state) = S;
    }
    else {
        (*state) = line_cur->state;
    }
    line_cur->timestamp = timer;
    directory_cache[home_id]->unlockUp(ins_mem);
    return delay;
}


//TLB translation from virtual addresses into physical addresses
int System::tlb_translate(InsMem *ins_mem, int core_id, int64_t timer)
{
    int delay = 0;
    Line* line_cur;
    InsMem ins_mem_old;
    line_cur = tlb_cache[core_id].accessLine(ins_mem);
    tlb_cache[core_id].incInsCount();
    delay += tlb_cache[core_id].getAccessTime();
    if (line_cur == NULL) {
        line_cur = tlb_cache[core_id].replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            tlb_cache[core_id].incEvictCount();
        }
        tlb_cache[core_id].incMissCount();
        line_cur->state = V;
        line_cur->ppage_num = page_table.translate(ins_mem);
        delay += page_table.getTransDelay();
    }
    line_cur->timestamp = timer;
    ins_mem->addr_dmem = (line_cur->ppage_num << (int)log2(page_size)) | (ins_mem->addr_dmem % page_size);
    return delay;
}

// Home allocation uses low-order bits interleaving
int System::allocHomeId(int num_homes, uint64_t addr)
{
    int offset;
    int home_bits;
    int home_mask;
    //Home bits
    offset = (int) log2(xml_sys->directory_cache.block_size );
    home_mask = (int)ceil(log2(num_homes));
    home_bits = (int)(((addr >> offset)) % (1 << home_mask));
    if (home_bits < num_homes) {
        return home_bits;
    }   
    else {
        return home_bits % (1 << (home_mask-1));
    }
}

int System::getHomeId(InsMem *ins_mem)
{
    int home_id;
    home_id = allocHomeId(network.getNumNodes(), ins_mem->addr_dmem);
    if (home_id < 0) {
        cerr<<"Error: Wrong home id\n";
    }
    return home_id;
}



int System::getCoreCount()
{
    return num_cores;
}


void System::report(ofstream* result, ofstream* stat)
{
    int i, j;
    uint64_t ins_count = 0, miss_count = 0, evict_count = 0, wb_count = 0, persist_count=0, persist_delay=0, intra_tot =0, inter_tot=0;
    uint64_t intra_persist_tot=0, inter_persist_tot=0, intra_pcycles_tot=0, inter_pcycles_tot=0;
    double miss_rate = 0;
    double  ratio_inter_intra_conflicts = 0.0;
    double  ratio_inter_intra_persists = 0.0;
    double  ratio_inter_intra_persist_cycles = 0.0;
    //double conflict_rate=0.0;
    uint64_t clwb_tot=0, critical_clwb=0, noncritical_clwb=0, external_clwb=0, all_natural_clwb=0, natural_clwb=0, sync_clwb=0, critical_persist_wb=0; //last one id related to total persist count
    uint64_t delay_clwb_tot=0, delay_critical_clwb=0, delay_noncritical_clwb=0, delay_external_clwb=0, delay_all_natural_clwb=0, delay_natural_clwb=0, delay_sync_clwb=0, delay_critical_persist_wb=0;
   uint64_t delay_same_block_tot =0; uint64_t same_block_count_tot =0;
   uint64_t epoch_sum_tot = 0;
   uint64_t epoch_id_tot = 0, epoch_id2_tot=0;
   uint64_t intra_vis_conflicts_tot=0, intra_evi_conflicts_tot=0, intra_evi_M_conflicts_tot=0, intra_allconflicts_tot=0, intra_vis_allconflicts_tot=0, intra_evi_allconflicts_tot=0, intra_evi_M_allconflicts_tot=0, intra_M_allconflicts_tot=0, intra_vis_persists_tot=0, intra_evi_persists_tot=0, intra_evi_M_persists_tot=0, inter_M_conflicts_tot=0, inter_allconflicts_tot=0, inter_M_allconflicts_tot=0, inter_M_persists_tot=0;
   uint64_t nzero_conflicts_tot=0, lowest_epoch_size_tot = 0, largest_lowest_epoch_size_tot = 0, max_largest_lowest_epoch_size = 0, num_epochs_flushed_tot = 0, num_nzero_epochs_flushed_tot = 0, max_largest_epoch_size = 0, largest_epoch_size_tot = 0 ;
 
   uint64_t all_persists_tot =0, all_pf_persists_tot=0, vis_pf_persists_tot = 0, evi_pf_persists_tot=0, inter_pf_persists_tot=0; 
   uint64_t invals_tot = 0, invals_M_tot = 0, invals_E_tot = 0, shares_tot = 0, shares_M_tot = 0, shares_E_tot = 0;
   
   //RET
   uint64_t atom_clk_timestamp_tot=0, clk_timestamp_tot=0, ret_add_tot=0, ret_delete_tot=0, ret_delete_lower_tot=0, ret_lookup_tot=0, ret_intra_lookup_tot = 0, ret_inter_lookup_tot = 0;

    network.report(result); 
    dram.report(result); 

    //MCQ and PBuff Stats
    mem_ctrl_queue->report(result);

    uint64_t num_wr_tot = 0;
    uint64_t num_cls_tot = 0;
    uint64_t num_evi_tot = 0;
    uint64_t num_barr_tot = 0;
    uint64_t num_acq_tot = 0;    
    uint64_t num_dep_conflicts_tot = 0;
    uint64_t num_dep_cls_flushed_tot = 0;

    for (int i = 0; i < num_cores; i++) {
        //persist_buffer[i].report(result);
        num_wr_tot += persist_buffer[i].num_wr;
        num_cls_tot += persist_buffer[i].num_cl_insert;
        num_evi_tot += persist_buffer[i].num_cl_evict;
        num_barr_tot += persist_buffer[i].num_barriers;
        num_acq_tot += persist_buffer[i].num_acq_barr;

        num_dep_conflicts_tot += persist_buffer[i].num_dep_conflicts;
        num_dep_cls_flushed_tot += persist_buffer[i].num_dep_cls_flushed;
    }

    *result << num_wr_tot << ", " << num_cls_tot << ", " << num_evi_tot << ", " << num_barr_tot << ", " << num_acq_tot <<  ", " << num_dep_conflicts_tot << ", " << num_dep_cls_flushed_tot << endl;

    *result << endl <<  "Simulation result for cache system: \n\n";
    
    if (verbose_report) {
        *result << "Home Occupation:\n";
        if (network.getNetType() == MESH_3D) {
            *result << "Allocated home locations in 3D coordinates:" << endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (home_stat[i]) {
                    Coord loc = network.getLoc(i); 
                    *result << "("<<loc.x<<", "<<loc.y<<", "<<loc.z<<")\n";
                }
            }
        }
        else {
            *result << "Allocated home locations in 2D coordinates:" << endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (home_stat[i]) {
                    Coord loc = network.getLoc(i); 
                    *result << "("<<loc.x<<", "<<loc.y<<")\n";
                }
             }
        }
        *result << endl;
    }
    *result << endl;

    if (tlb_enable) {
        ins_count =0;   
        miss_count = 0;
        evict_count = 0;
        miss_rate = 0;
        for (int i = 0; i < num_cores; i++) {
            ins_count += tlb_cache[i].getInsCount();
            miss_count += tlb_cache[i].getMissCount();
            wb_count += tlb_cache[i].getWbCount();
        }
        miss_rate = (double)miss_count / (double)ins_count; 
           
        *result << "TLB Cache"<<"===========================================================\n";
        *result << "Simulation results for "<< xml_sys->tlb_cache.size << " Bytes " << xml_sys->tlb_cache.num_ways
                   << "-way set associative cache model:\n";
        *result << "The total # of TLB access instructions: " << ins_count << endl;
        *result << "The # of cache-missed instructions: " << miss_count << endl;
        *result << "The # of replaced instructions: " << evict_count << endl;
        *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;
        *result << "=================================================================\n\n";
        
        if (verbose_report) {
            page_table.report(result);
        }
    } 

    *result << endl;
    *result << "Total delay caused by bus contention: " << total_bus_contention <<" cycles\n";
    *result << "Total # of broadcast: " << total_num_broadcast <<"\n\n";

    for (i=0; i<num_levels; i++) {
        ins_count =0;   
        miss_count = 0;
        evict_count = 0;
        wb_count = 0;
        persist_count = 0;
        persist_delay =0;
        miss_rate = 0;

        delay_critical_persist_wb = 0;
        inter_tot = 0;
        intra_tot = 0;
        ratio_inter_intra_conflicts = 0;
        inter_persist_tot = 0;
        intra_persist_tot = 0;
        ratio_inter_intra_persists = 0;
        inter_pcycles_tot = 0;
        intra_pcycles_tot = 0;
        ratio_inter_intra_persist_cycles = 0;
        clwb_tot = 0;
        critical_clwb = 0;
        noncritical_clwb = 0;
        external_clwb = 0;
        natural_clwb = 0;
        all_natural_clwb = 0;
        sync_clwb = 0;
        critical_persist_wb = 0;
        delay_clwb_tot = 0;
        delay_critical_clwb = 0;
        delay_noncritical_clwb = 0;
        delay_external_clwb = 0;
        delay_natural_clwb = 0;
        delay_all_natural_clwb = 0;
        delay_sync_clwb = 0;

        epoch_sum_tot = 0;
        epoch_id_tot = 0;
        epoch_id2_tot = 0;

        same_block_count_tot = 0;
        delay_same_block_tot =0;

        intra_vis_conflicts_tot=0; 
        intra_evi_conflicts_tot=0; 
        intra_evi_M_conflicts_tot=0; 
        intra_allconflicts_tot=0; 
        intra_vis_allconflicts_tot=0; 
        intra_evi_allconflicts_tot=0; 
        intra_evi_M_allconflicts_tot=0; 

        intra_M_allconflicts_tot=0; 
        intra_vis_persists_tot=0; 
        intra_evi_persists_tot=0; 
        intra_evi_M_persists_tot=0; 
        intra_evi_persists_tot=0; 
        inter_M_conflicts_tot=0; 
        inter_allconflicts_tot=0; 
        inter_M_allconflicts_tot=0; 
        inter_M_persists_tot=0;


        lowest_epoch_size_tot = 0; 
        largest_lowest_epoch_size_tot = 0; 
        max_largest_lowest_epoch_size = 0; 
        num_epochs_flushed_tot = 0; 
        num_nzero_epochs_flushed_tot = 0;
        max_largest_epoch_size = 0; 
        largest_epoch_size_tot = 0;

        all_persists_tot = 0;
        all_pf_persists_tot = 0;
        vis_pf_persists_tot = 0;
        evi_pf_persists_tot = 0;
        inter_pf_persists_tot = 0;

        invals_tot = 0;
        invals_M_tot = 0;
        invals_E_tot = 0;
        shares_tot = 0;
        shares_M_tot = 0;
        shares_E_tot = 0;

        clk_timestamp_tot = 0;
        atom_clk_timestamp_tot = 0;
        ret_add_tot = 0;
        ret_delete_tot = 0;
        ret_delete_lower_tot = 0; 
        ret_lookup_tot = 0;
        ret_intra_lookup_tot = 0;
        ret_inter_lookup_tot = 0;

        for (j=0; j<cache_level[i].num_caches; j++) {
            if (cache[i][j] != NULL) {
                ins_count += cache[i][j]->getInsCount();
                miss_count += cache[i][j]->getMissCount();
                evict_count += cache[i][j]->getEvictCount();
                wb_count += cache[i][j]->getWbCount();
                persist_count += cache[i][j]->getPersistCount();
                persist_delay += cache[i][j]->getPersistDelay();
                inter_tot += cache[i][j]->inter_conflicts;
                intra_tot += cache[i][j]->intra_conflicts;
                ratio_inter_intra_conflicts += (double)inter_tot/(inter_tot+intra_tot);
                inter_persist_tot += cache[i][j]->inter_persists;
                intra_persist_tot += cache[i][j]->intra_persists;
                ratio_inter_intra_persists += (double)inter_persist_tot/(inter_persist_tot+intra_persist_tot);
                inter_pcycles_tot += cache[i][j]->inter_persist_cycles;
                intra_pcycles_tot += cache[i][j]->intra_persist_cycles;
                ratio_inter_intra_persist_cycles += (double)inter_pcycles_tot/(inter_pcycles_tot+intra_pcycles_tot);

                same_block_count_tot += cache[i][j]->same_block_count; 
                clwb_tot            +=    cache[i][j]->write_back_count; 
                critical_clwb       +=   cache[i][j]->critical_write_back_count; 
                noncritical_clwb    +=    cache[i][j]->noncritical_write_back_count; 
                external_clwb       +=   cache[i][j]->external_critical_wb_count; 
                natural_clwb        +=    cache[i][j]->natural_eviction_count; 
                all_natural_clwb    +=    cache[i][j]->all_natural_eviction_count;
                sync_clwb           +=   cache[i][j]->sync_conflict_persists; 
                critical_persist_wb +=     cache[i][j]->critical_conflict_persists;


                delay_same_block_tot += cache[i][j]->same_block_delay; 
                delay_clwb_tot              +=      cache[i][j]->write_back_delay;
                delay_critical_clwb         +=      cache[i][j]->critical_write_back_delay;
                delay_noncritical_clwb      +=      cache[i][j]->noncritical_write_back_delay;
                delay_external_clwb         +=      cache[i][j]->external_critical_wb_delay;
                delay_natural_clwb          +=      cache[i][j]->natural_eviction_delay;
                delay_all_natural_clwb      +=      cache[i][j]->all_natural_eviction_delay;
                delay_sync_clwb             +=      cache[i][j]->sync_conflict_persist_cycles;
                delay_critical_persist_wb   +=      cache[i][j]->critical_conflict_persist_cycles;

                intra_vis_conflicts_tot += cache[i][j]-> intra_vis_conflicts;
                intra_evi_conflicts_tot += cache[i][j]-> intra_evi_conflicts ;
                intra_evi_M_conflicts_tot += cache[i][j]->intra_evi_M_conflicts ;
                intra_vis_allconflicts_tot += cache[i][j]->  intra_vis_allconflicts;
                intra_evi_allconflicts_tot += cache[i][j]->  intra_evi_allconflicts;
                intra_evi_M_allconflicts_tot += cache[i][j]->  intra_evi_M_allconflicts;
                intra_allconflicts_tot += cache[i][j]->  intra_allconflicts;
                intra_M_allconflicts_tot += cache[i][j]->intra_M_allconflicts;
                intra_vis_persists_tot += cache[i][j]-> intra_vis_persists;
                intra_evi_persists_tot += cache[i][j]-> intra_evi_persists;
                intra_evi_M_persists_tot += cache[i][j]-> intra_evi_M_persists;
                //intra_evi_persist_tot += cache[i][j]-> intra_evi_persist;
                inter_M_conflicts_tot += cache[i][j]-> inter_M_conflicts;
                inter_allconflicts_tot += cache[i][j]-> inter_allconflicts;
                inter_M_allconflicts_tot += cache[i][j]-> inter_M_allconflicts;
                inter_M_persists_tot += cache[i][j]-> inter_M_persists;

                //epochs stats
                nzero_conflicts_tot += cache[i][j]->nzero_conflicts ;
                lowest_epoch_size_tot += cache[i][j]->lowest_epoch_size ; 
                largest_lowest_epoch_size_tot += cache[i][j]->largest_lowest_epoch_size ; 
                if(max_largest_lowest_epoch_size < cache[i][j]-> largest_lowest_epoch_size) 
                    max_largest_lowest_epoch_size = cache[i][j]-> largest_lowest_epoch_size; 
                num_epochs_flushed_tot += cache[i][j]-> num_epochs_flushed; 
                num_nzero_epochs_flushed_tot += cache[i][j]-> num_nzero_epochs_flushed; 
                if(max_largest_epoch_size < cache[i][j]-> largest_epoch_size)
                    max_largest_epoch_size = cache[i][j]-> largest_epoch_size; 
                largest_epoch_size_tot +=cache[i][j]-> largest_epoch_size;

                epoch_sum_tot += cache[i][j]->epoch_sum;
                epoch_id_tot += cache[i][j]->epoch_updated_id;
                epoch_id2_tot += cache[i][j]->epoch_counted_id;

                all_persists_tot += cache[i][j]-> all_persists;
                all_pf_persists_tot += cache[i][j]->all_pf_persists;
                vis_pf_persists_tot += cache[i][j]->vis_pf_persists;
                evi_pf_persists_tot += cache[i][j]->evi_pf_persists;
                inter_pf_persists_tot += cache[i][j]->inter_pf_persists;

                invals_tot += cache[i][j]->invals;
                invals_M_tot += cache[i][j]->invals_M;
                invals_E_tot += cache[i][j]->invals_E;
                shares_tot += cache[i][j]->shares;
                shares_M_tot += cache[i][j]->shares_M;
                shares_E_tot += cache[i][j]->shares_E;

                atom_clk_timestamp_tot += cache[i][j]->atom_clk_timestamp;
                clk_timestamp_tot += cache[i][j]->clk_timestamp;
                ret_add_tot += cache[i][j]->cnt_sm_add;
                ret_delete_tot += cache[i][j]->cnt_sm_delete;
                ret_delete_lower_tot += cache[i][j]->cnt_sm_delete_lower;
                ret_lookup_tot += cache[i][j]->cnt_sm_lookups;
                ret_intra_lookup_tot += cache[i][j]->cnt_sm_rel_intra_lookups;
                ret_inter_lookup_tot += cache[i][j]->cnt_sm_rel_inter_lookups;


            }
        }
        miss_rate = (double)miss_count / (double)ins_count; 

        *result << "LEVEL"<<i<<"===========================================================\n";
        *result << "Simulation results for "<< cache_level[i].size << " Bytes " << cache_level[i].num_ways
                   << "-way set associative cache model:\n";
        *result << "The total # of memory instructions: " << ins_count << endl;
        *result << "The # of cache-missed instructions: " << miss_count << endl;
        *result << "The # of evicted instructions: " << evict_count << endl;
        *result << "The # of writeback instructions: " << wb_count << endl;
        *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;        
        *result << "-----------------------------------------------------------------------\n";
        *result << "Average Inter-thread conflicting Persists: " <<  inter_persist_tot << endl;
        *result << "Average Intra-thread conflicting Persists: " <<  intra_persist_tot << endl;
        *result << "Average Inter-Intra P ratio (SUM): " << (double)inter_persist_tot/(inter_persist_tot+intra_persist_tot) << endl;
        *result << "Average Inter-Intra P ratio (AVG): " << ratio_inter_intra_persists/cache_level[i].num_caches << endl;
        *result << "=----------------------------------------------------------------------\n";
        *result << "Average Inter-thread conflicts: " <<  inter_tot << endl;
        *result << "Average Intra-thread conflicts: " <<  intra_tot << endl;
        *result << "Average Inter-Intra ratio (SUM): " << (double)inter_tot/(inter_tot+intra_tot) << endl;
        *result << "Average Inter-Intra ratio (AVG): " << ratio_inter_intra_conflicts/cache_level[i].num_caches << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Average Inter-thread Persist Cycles: " <<  inter_pcycles_tot << endl;
        *result << "Average Intra-thread Persist Cycles: " <<  intra_pcycles_tot << endl;
        *result << "Average Inter-Intra PC ratio (SUM): " << (double)inter_pcycles_tot/(inter_pcycles_tot+intra_pcycles_tot) << endl;
        *result << "Average Inter-Intra PC ratio (AVG): " << ratio_inter_intra_persist_cycles/cache_level[i].num_caches << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Write-Backs count and delay " <<  clwb_tot << " \t" << delay_clwb_tot << endl;
        *result << "Critical Write-Backs count and delay " <<  critical_clwb << " \t" << delay_critical_clwb << endl;
        *result << "NonCritical Write-Backs count and delay " <<  noncritical_clwb << " \t" << delay_noncritical_clwb << endl;
        *result << "External Write-Backs count and delay " <<  external_clwb << " \t" << delay_external_clwb << endl;
        *result << "Eviction count and delay " <<  natural_clwb << " \t" << delay_natural_clwb << endl;
        *result << "Write-Backs count and delay (all) " <<  all_natural_clwb << " \t" << delay_all_natural_clwb << endl;
        //Not Used fields-only for initial testing.
        uint64_t conflicting_nat_wb = (pmodel==0)? 0: ((pmodel==3 || pmodel==8)? intra_evi_M_allconflicts_tot: intra_evi_M_allconflicts_tot);
        uint64_t conflicting_nat_wb_cycle = 0;
        //Not Used fields-only for initial testing.
        uint64_t conflicting_nat_wb2 = (pmodel==0)? 0: ((pmodel==3 || pmodel==8)? intra_evi_M_allconflicts_tot: intra_evi_M_allconflicts_tot);
        uint64_t conflicting_nat_wb_cycle2 = 0;

        *result << "Conflicting natural Evictions/Write-Backs count and delay " << conflicting_nat_wb << " \t" << conflicting_nat_wb_cycle << endl;
        //Not Used fields-only for initial testing.
        uint64_t tot_writeback_conflicts = (pmodel==0)? 0: ((pmodel==3 || pmodel==8)?intra_evi_persists_tot: intra_evi_M_allconflicts_tot); //same as nat_wb
        uint64_t tot_evi_conflicts = 0;

        *result << "Sync Write-Backs count and delay " <<  sync_clwb << " \t" << delay_sync_clwb << endl;
        *result << "Critical Persist count and delay " <<  critical_persist_wb << " \t" << critical_persist_wb << endl;
        *result << "All perisist (Persistence)" <<  persist_count << " \t" << persist_delay << endl;
        *result << "Sync WB/Critical WB " <<  (double)sync_clwb/critical_clwb << endl;
        *result << "Critical WB/All WB " <<  (double)critical_clwb/clwb_tot << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Total Epochs Sum " <<  epoch_sum_tot << endl;
        *result << "Total IDs Sum " <<  epoch_id_tot << endl;
        *result << "Total Counted IDs Sum " <<  epoch_id2_tot << endl;
        *result << "Average Writes/Epoch" <<  (double)epoch_sum_tot/epoch_id_tot << endl;

        *result << "-----------------------------------------------------------------------\n\n\n";
        
        *result << "Intra-conflict " <<  intra_tot << endl;
        *result << "Intra-conflict: Visibility " <<  intra_vis_conflicts_tot << endl;
        *result << "Intra-conflict: Evictions " <<  intra_evi_conflicts_tot << endl;
        *result << "Intra-conflict: Evictions-M" <<  intra_evi_M_conflicts_tot << endl;

        *result << "Intra-allconflict " <<  intra_allconflicts_tot << endl;
        *result << "Intra-allconflict:Visibility " <<  intra_vis_allconflicts_tot << endl;
        *result << "Intra-allconflict:Evictions " <<  intra_evi_allconflicts_tot << endl;
        *result << "Intra-allconflict:Evictions_M " <<  intra_evi_M_allconflicts_tot << endl;
        *result << "Intra-allconflict: Evictions-M" <<  intra_M_allconflicts_tot << endl;

        *result << "Intra-persists: All " <<  intra_persist_tot << endl;
        *result << "Intra-persists: Visibility " <<  intra_vis_persists_tot << endl;
        *result << "Intra-persists: Evictions " <<  intra_evi_persists_tot << endl;
        *result << "Intra-persists: Evictions-M" <<  intra_evi_M_persists_tot << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Inter-conflicts " <<  inter_tot << endl;
        *result << "Inter-conflicts-M " <<  inter_M_conflicts_tot << endl;
        *result << "Inter-allconflicts "  <<  inter_allconflicts_tot << endl;
        *result << "Inter-allconflicts-M " <<  inter_M_allconflicts_tot << endl;
        *result << "Inter-persists " <<  inter_persist_tot << endl;
        *result << "Inter-persists-M " <<  inter_M_persists_tot << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Invals - "  <<  invals_tot << endl;
        *result << "InvalsM - " <<  invals_M_tot << endl;
        *result << "InvalsE - " <<  invals_E_tot << endl;
        *result << "Shares - "  <<  shares_tot << endl;
        *result << "SharesM - " <<  shares_M_tot << endl;
        *result << "SharesE - " <<  shares_E_tot << endl;      
        *result << "-----------------------------------------------------------------------\n\n\n";

        *result << "Total Lowest Epoch Size " <<  lowest_epoch_size_tot << endl;
        *result << "Total ZN Conflicts from caches " <<  nzero_conflicts_tot << endl;
        *result << "Total NZ conflicts " <<  (intra_tot+inter_tot) << endl;
        *result << "Average Lowest Epoch Size " <<  (double)lowest_epoch_size_tot/(intra_tot+inter_tot+1) << endl;
        *result << "Average Largest Lowest Epoch Size " <<  largest_lowest_epoch_size_tot<< endl;
        *result << "Max Largest Lowest Epoch Size " <<  max_largest_lowest_epoch_size<< endl;

        *result << "Total Number of Epochs Flushed " <<  num_epochs_flushed_tot << endl;
        *result << "Total Number of (NZ) Epochs Flushed " <<  num_nzero_epochs_flushed_tot << endl;
        *result << "Average Number of (NZ) Epochs Flushed per conflict" <<  (double)num_nzero_epochs_flushed_tot/(nzero_conflicts_tot+1) << endl;

        *result << "Largest Epoch flushed (across all threads) " <<  max_largest_epoch_size << endl;
        *result << "Average largest epoch flushed (/num-threads) " <<  largest_epoch_size_tot << endl;
        
        *result << "-----------------------------------------------------------------------\n\n\n";      

        *result << "Total Persists " <<  all_persists_tot << endl;
        *result << "all_pf_persists " <<  all_pf_persists_tot << endl;
        *result << "vis_pf_persists " <<  vis_pf_persists_tot << endl;
        *result << "evi_pf_persists " <<  evi_pf_persists_tot << endl;
        *result << "inter_pf_persists " << inter_pf_persists_tot << endl; 

        *result << "=================================================================\n\n";

        *result << "RET: -----------------------------------------------------------------\n";
        *result << "Total Cycles " << clk_timestamp_tot  << endl;
        *result << "Total Rel Cycles " << atom_clk_timestamp_tot  << endl;
        *result << "Writes per Cycles " << (double)ret_add_tot/ clk_timestamp_tot  << endl;
        *result << "Lookups per Cycles " << (double)ret_lookup_tot/ clk_timestamp_tot  << endl;
        *result << "Writes per Rel ycles " << (double)ret_add_tot/ atom_clk_timestamp_tot  << endl;
        *result << "Lookups per Rel Cycles " << (double)ret_lookup_tot/ atom_clk_timestamp_tot  << endl;
        *result << "Adds " << (double)ret_add_tot  << endl;
        *result << "Lookups " << (double)ret_lookup_tot  << endl;
        *result << "Lookups Intra " << (double)ret_intra_lookup_tot  << endl;
        *result << "Lookups Inter " << (double)ret_inter_lookup_tot  << endl;
        *result << "Delete " << (double)ret_delete_tot  << endl;
        *result << "Delete Lower" << (double)ret_delete_lower_tot  << endl;        

        *result << "RET: -----------------------------------------------------------------\n\n";

        //Something to add.
        //Conflicting WB.
        //Statistic FIle.
        double conf_rate =(double)inter_persist_tot/(inter_persist_tot+intra_persist_tot);
        double wbdelay_rate = (double)delay_critical_clwb/(delay_critical_clwb+delay_noncritical_clwb);
        double wb_rate = (double)critical_clwb/(critical_clwb+noncritical_clwb);
        double p_rate = (double) critical_persist_wb/clwb_tot;
        double pdelay_rate = (double) delay_critical_persist_wb/delay_clwb_tot;
        //double clwb = clwb_tot;

        //ASPLOS Updates WB
        //*stat << inter_persist_tot << "," << intra_persist_tot << "," << persist_count << "," << clwb_tot << "," << critical_clwb << "," << noncritical_clwb << "," << natural_clwb << "," << external_clwb << "," << inter_tot << "," << intra_tot <<"," << same_block_count_tot << "," << delay_same_block_tot << ","  << epoch_sum_tot << "," << epoch_id_tot << "," << epoch_id2_tot << endl;
        
        *stat   << clwb_tot << ","
                << critical_clwb << ","
                << critical_clwb << ","
                << noncritical_clwb << ","
                << natural_clwb << ","
                << all_natural_clwb << ","

                << intra_tot << ","
                << intra_vis_conflicts_tot << ","
                << intra_evi_conflicts_tot << ","
                << intra_evi_M_conflicts_tot << ","

                << intra_allconflicts_tot << ","
                << intra_vis_allconflicts_tot << ","
                << intra_evi_allconflicts_tot << ","
                << intra_evi_M_allconflicts_tot << ","
                << intra_M_allconflicts_tot << ","

                << intra_persist_tot << ","
                << intra_vis_persists_tot << ","
                << intra_evi_persists_tot << ","
                << intra_evi_M_persists_tot << ","

                << inter_tot << ","
                << inter_M_conflicts_tot << ","
                << inter_allconflicts_tot << ","
                << inter_M_allconflicts_tot << ","
                << inter_persist_tot << ","
                << inter_M_persists_tot << ","

                << all_persists_tot << ","
                << all_pf_persists_tot << ","
                << vis_pf_persists_tot << ","
                << evi_pf_persists_tot << ","
                << inter_pf_persists_tot << ","

                << invals_tot << ","
                << invals_M_tot << ","
                << invals_E_tot << ","
                << shares_tot << ","
                << shares_M_tot << ","
                << shares_E_tot << ","   

                //later addition-after data2
                <<  conflicting_nat_wb << ","
                <<  conflicting_nat_wb_cycle << ","
                <<  conflicting_nat_wb2 << ","
                <<  conflicting_nat_wb_cycle2 << ","
                <<  tot_writeback_conflicts << ","
                <<  tot_evi_conflicts << endl ;

                //Need to add propoer conlficting writebacks for each one.
        //Adding new division of visibility/evictions/ and PF to stat file

        *result << conf_rate << "," << wb_rate << "," << wbdelay_rate << "," << p_rate << "," << pdelay_rate << "," << clwb_tot << "," << epoch_sum_tot << "," << epoch_id_tot << "," << epoch_id2_tot << endl;
    }
    
    ins_count =0;   
    miss_count = 0;
    evict_count = 0;
    wb_count = 0;
    miss_rate = 0;
    for (int i = 0; i < network.getNumNodes(); i++) {
        if (directory_cache[i] != NULL) {
            ins_count += directory_cache[i]->getInsCount();
            miss_count += directory_cache[i]->getMissCount();
            evict_count += directory_cache[i]->getEvictCount();
            wb_count += directory_cache[i]->getWbCount();
        }
    }
    miss_rate = (double)miss_count / (double)ins_count; 
       
    *result << "Directory Cache"<<"===========================================================\n";
    *result << "Simulation results for "<< xml_sys->directory_cache.size << " Bytes " << xml_sys->directory_cache.num_ways
               << "-way set associative cache model:\n";
    *result << "The total # of memory instructions: " << ins_count << endl;
    *result << "The # of cache-missed instructions: " << miss_count << endl;
    *result << "The # of replaced instructions: " << evict_count << endl;
    *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;
    *result << "=================================================================\n\n";


    if (verbose_report) {
        *result << "Statistics for each cache with non-zero accesses: \n\n";
        for (i=0; i<num_levels; i++) {
            *result << "LEVEL"<<i<<"*****************************************************\n\n";
            for (j=0; j<cache_level[i].num_caches; j++) {
                if (cache[i][j] != NULL) {
                    if (cache[i][j]->getInsCount() > 0) {
                        *result << "The " <<j<<"th cache:\n";
                        cache[i][j]->report(result);
                    }
                }
             }
            *result << "************************************************************\n\n";
        }

        if (directory_cache != NULL) {
            *result << "****************************************************" <<endl;
            *result << "Statistics for each directory caches with non-zero accesses" <<endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (directory_cache[i] != NULL) {
                    if (directory_cache[i]->getInsCount() > 0) {
                        *result << "Report for directory cache "<<i<<endl;
                        directory_cache[i]->report(result);
                    }
                }
            }
        }

        if (tlb_cache != NULL) {
            *result << "****************************************************" <<endl;
            *result << "Statistics for each TLB cache with non-zero accesses" <<endl;
            for (int i = 0; i < num_cores; i++) {
                if (tlb_cache[i].getInsCount() > 0) {
                    *result << "Report for tlb cache "<<i<<endl;
                    tlb_cache[i].report(result);
                }
            }
        }
    }
}

System::~System()
{
        int i, j;
        for (i=0; i<num_levels; i++) {
            for (j=0; j<cache_level[i].num_caches; j++) {
                if (cache[i][j] != NULL) {
                    if (i != 0) {
                        delete [] cache[i][j]->child;
                    }
                    delete cache[i][j];
                }
            }
        }
        for (i=0; i<num_levels; i++) {
            delete [] cache[i];
            delete [] cache_lock[i];
            delete [] cache_init_done[i];
        }
        for (i = 0; i < network.getNumNodes(); i++) {
            if (directory_cache[i] != NULL) {
                delete directory_cache[i];
            }
        }
        delete [] hit_flag;
        delete [] delay;
        delete [] home_stat;
        delete [] cache;
        delete [] cache_level;
        delete [] directory_cache;
        delete [] tlb_cache;
        delete [] cache_lock;
        delete [] directory_cache_lock;
        delete [] cache_init_done;
        delete [] directory_cache_init_done;
}


/* Old version before the DRAM Output stream.

//Access distributed shared LLC with embedded directory cache
int System::accessSharedCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state, int core_id)
{
    int level;
    level = num_levels;
    level = level;
    if (!directory_cache_init_done[home_id]) {
        init_directories(home_id); 
    }
    int delay = 0, delay_temp =0, delay_pipe =0, delay_max = 0;
    InsMem ins_mem_old;
    IntSet::iterator pos;
    Line* line_cur;
    home_stat[home_id] = 1;
    directory_cache[home_id]->lockUp(ins_mem);
    line_cur = directory_cache[home_id]->accessLine(ins_mem);
    directory_cache[home_id]->incInsCount();
    delay += directory_cache[home_id]->getAccessTime();
    //Shard llc miss
    if ((line_cur == NULL) && (ins_mem->mem_type != WB)) {
        line_cur = directory_cache[home_id]->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            directory_cache[home_id]->incEvictCount();
            if (line_cur->state == M) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id , 0, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], &ins_mem_old, core_id);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }
            //Broadcast
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], &ins_mem_old, core_id);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 
        }
        if(ins_mem->mem_type == WR) {
            line_cur->state = M;
        }
        else {
            line_cur->state = E;
        }

        directory_cache[home_id]->incMissCount();
        line_cur->sharer_set.clear();
        line_cur->sharer_set.insert(cache_id);
        delay += dram.access(ins_mem); //Read data from DRAM
    }   
    //Shard llc hit
    else {
        if (ins_mem->mem_type == WR) {

            //asplos-after: This does not writeback data. need to change. 

            if ((line_cur->state == M) || (line_cur->state == E)) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                
                //asplos-after: 
                if(pmodel != NONB){
                    //Do something: (1) this is important when non-invalidating write is used.

                }else{
                    //NOP-No need to do anything.
                    //delay += dram.access(ins_mem);
                }

            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem, core_id);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if(delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], ins_mem, core_id);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if(delay_temp > delay_max) 
                    {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 
            else if(line_cur->state == V) {
            }
            line_cur->state = M;
            line_cur->sharer_set.clear();
            line_cur->sharer_set.insert(cache_id);
        }
        else if (ins_mem->mem_type == RD) {
             if ((line_cur->state == M) || (line_cur->state == E)) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                line_cur->state = S;

                //asplos-after: if in M state, need to writebackdata  
                if(pmodel != NONB){
                    //Do something
                }else{
                    //NOP: M->S transition.
                    delay += dram.access(ins_mem);
                }            

            }
            else if (line_cur->state == S) {

                //This is the only change with direcotry access: delay += dram.access(ins_mem);
                if ((protocol_type == LIMITED_PTR) && ((int)line_cur->sharer_set.size() >= max_num_sharers)) {
                    line_cur->state = B;
                }
                else {
                    line_cur->state = S;
                }
            } 
            else if (line_cur->state == B) {
                line_cur->state = B;
            } 
            else if (line_cur->state == V) {
                line_cur->state = E;
            } 
            line_cur->sharer_set.insert(cache_id);
        }

        //asplos-after: WB also needs a variant for lazy and nlazy.
        else if((ins_mem->mem_type == WB)){
            //printf("writeback 0x%lx \n",ins_mem->addr_dmem);

            //asplos-after: nonp dont need to writeback data.
            if(pmodel != NONB){
                delay += dram.access(ins_mem);
                line_cur->state = I; //Remark:This need to be changed with. No need to invalidate.
            }                 
            else dram.access(ins_mem);

            //line_cur->state = I;
            line_cur->sharer_set.clear();            
        }//Writeback 

        //after-asplos: add new clwb operations. //next lazy and nlazy operations.
        else if(ins_mem->mem_type == CLWB){ //Write-back
                
                //Non-Invalidation writenbacks
            if(pmodel != NONB) 
                delay += dram.access(ins_mem);
            else dram.access(ins_mem); //delay is not added.
                //line_cur->state = I;
                //line_cur->sharer_set.clear(); //Not necessary
        }

        else {
            line_cur->state = V;
            line_cur->sharer_set.clear();
            dram.access(ins_mem);
        }
    }
    if (line_cur->state == B) {
        (*state) = S;
    }
    else {
        (*state) = line_cur->state;
    }
    line_cur->timestamp = timer;
    directory_cache[home_id]->unlockUp(ins_mem);
    return delay;
}
*/




/*
int System::epochPersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc){ //calling 
    
    //after-asplos
    //persist source: psrc: 0-same-block, 1-evctions/replacements, 2-inter-thread/writebacks

    //Buffered Epoch Persistency on caches
    int conflict_epoch_id = line_call->max_epoch_id; //Not sure! Check
    #ifdef DEBUG
    printf("Buffered Epoch Persistency Cache:%d Conflict Epoch:%d \n", req_core_id, conflict_epoch_id);
    #endif
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId(); int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = 0;

    //timing
    delay_tmp[core_id] = 0;
    int start_delay = 0;

    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    int level = cache_cur->getLevel(); 
    int persist_count =0;
    
    //Check thi: method-local intialization
    delay_tmp[core_id] += delay[core_id];
    start_delay = delay_tmp[core_id];
    //Proactive Flushing (PF): reduce the conflicts. allow lazy write-backs and coalecsing inside the cache-line.

    //Actual flush: min epoch id flushed = et_lowest_epoch_id
    for (uint64_t i = 0; i < cache_cur->getNumSets(); i++) {            
        for (uint64_t j = 0; j < cache_cur->getNumWays(); j++) {                
                //printf("A Epoch %d\n", rel_epoch_id);
                InsMem ins_mem;
                line_cur = cache_cur->directAccess(i,j,&ins_mem);    
                            
                if(line_cur != NULL && line_cur !=line_call && (line_cur->state==M)){ //|| line_cur->state==E  //Need to think about E here
                    //Only dirty data
                    //if(line_cur->max_epoch_id <= conflict_epoch_id){ //Epoch Id < conflictigng epoch id - similar to full flush
                    if(line_cur->max_epoch_id < conflict_epoch_id){    

                    #ifdef DEBUG
                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                    #endif
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        ins_mem.lazy_wb =false; 
                        //asplos-after: et_lowest_epoch_id: proactive_flushing
                        //if(proactive_flushing && (line_cur->max_epoch_id == et_lowest_epoch_id))
                        //    ins_mem.lazy_wb =true; //new lazy write-backs: no delay is added.                      

                        //dramsim2: add access delay.

                        //dramsim2: resolve llc conflicts. Go and check llc and push down cachelines. 

                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);

                        //ins_mem.lazy_wb =false;
                        
                        cache_cur->incPersistCount();
                        persist_count++;      
                        cache_cur->all_persists++;                 

                        //asplos-after: counting the average number of cachelines per epoch that is flushing
                        //bug fixed. this must come after the above.
                        line_cur->dirty = 0;
                        line_cur->state = I; //changed the 
                        line_cur->min_epoch_id = 0;
                        line_cur->max_epoch_id = 0;

                    }
                }
        }
    } 

    //Counts
    if(core_id == req_core_id){
        //asplos-after: psrc: 0-visibility conflics, 1-evictions         
        if(persist_count>0){
             cache_cur->intra_conflicts++;

             if(psrc==VIS){
                cache_cur->intra_vis_conflicts++;
             }else if(psrc == EVI){
                cache_cur->intra_evi_conflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_conflicts++;
                //else cache_cur->intra_evi_E_conflicts++;
             }
        }

        cache_cur->intra_allconflicts++;
        if(line_call->state == M) cache_cur->intra_M_allconflicts++;

        if(psrc==VIS){
               cache_cur->intra_vis_allconflicts++;
        }else if(psrc == EVI){
                cache_cur->intra_evi_allconflicts++;
                if(line_call->state==M) cache_cur->intra_evi_M_allconflicts++;
                //else cache_cur->intra_evi_E_allconflicts++;
        }

        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //psrc?
        if(psrc==VIS){
            cache_cur->intra_vis_persists += persist_count;
            cache_cur->intra_vis_persist_cycles += delay[core_id];
        }
        else if(psrc==EVI){            
            cache_cur->intra_evi_persists += persist_count;
            cache_cur->intra_evi_persist_cycles += delay[core_id];
            if(line_call->state == M) cache_cur->intra_evi_M_persists += persist_count;
            //else cache_cur->intra_evi_E_persists += persist_count;
        } 

        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;   //Write will be added later in the thread.
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{

        //asplos-after: psrc: 2-writebacks
        if(persist_count>0){
             cache_cur->inter_conflicts++;
             if(line_call->state==M)cache_cur->inter_M_conflicts++;
             //else cache_cur->inter_E_conflicts++;

        }  
        cache_cur->inter_allconflicts++;
            if(line_call->state==M) cache_cur->inter_M_allconflicts++;
            //else cache_cur->inter_E_allconflicts++;

        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        if(line_call->state == M) cache_cur->inter_M_persists += persist_count;
        //else cache_cur->inter_E_persists += persist_count;
        //New
        cache_cur->write_back_count += persist_count+1;
        cache_cur->noncritical_write_back_count += persist_count+1; //Is this true

        cache_cur->write_back_delay += delay[core_id]+1;
        cache_cur->noncritical_write_back_delay += delay[core_id]+1;
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];  
    }


    //Buffered Eopch Persistence
    #ifdef DEBUG
    printf("Number of persists (Buffered Epoch Persistency END): %d \n", persist_count); 
    #endif
    cache_cur->incPersistDelay(delay[core_id]);
    return delay[core_id];
}
*/