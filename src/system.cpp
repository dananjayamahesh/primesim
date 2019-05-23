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
    dram_access_time = xml_sys->dram_access_time;
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

    pmodel = RLSB; //FULL BARRIER SEMANTICS
    pmodel = (PersistModel) xml_sys->pmodel;
    printf("Persistent Model %d \n", pmodel);

    hit_flag = new bool [num_cores];
    delay = new int [num_cores];
    for (i=0; i<num_cores; i++) {
        hit_flag[i] = false;
        delay[i] = 0;
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

    page_table.init(page_size, xml_sys->page_miss_delay);
    dram.init(dram_access_time);
}

// This function models an access to memory system and returns the delay.
int System::access(int core_id, InsMem* ins_mem, int64_t timer)
{    
    //uint64_t tmp_addr = ins_mem->addr_dmem;  
    #ifdef DEBUG
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
 
    if (sys_type == DIRECTORY) {
        mesi_directory(cache[0][core_id], 0, cache_id, core_id, ins_mem, timer + delay[core_id]);
    }
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
    printf("[Cache Initialization] Initiated Cache : %d of level %d \n", cache_id, level);
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
    printf("Directory is created : Home Id - %d of level %d \n\n", home_id, num_levels);
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
                               inval_children(cache[num_levels-1][i], ins_mem, core_id);
                           }
                       }
                   }
               }
               line_cur->state = M;
           }
           inval_children(cache_cur, ins_mem, core_id);
           cache_cur->unlockUp(ins_mem);
           return M;
        }
        //Read
        else {
            if (line_cur->state != S) {
                share_children(cache_cur, ins_mem, core_id);
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
            inval_children(cache_cur, &ins_mem_old, core_id);
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
                            inval_children(cache[num_levels-1][i], ins_mem, core_id);
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
                             share_children(cache[num_levels-1][i], ins_mem, core_id);
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
        printf("\n[Cache] -----------------------Initiating Cache %d of %d --------------------\n", cache_id, level);
        cache_cur = init_caches(level, cache_id);       
        printf("[Cache] -----------------------Initiated  Cache %d of %d --------------------\n\n\n", cache_id, level);  
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

    if(level==0){
        //Epoch meters
        epoch_meters(core_id, ins_mem);
    }

    //Full barrier flush in reelase persistency - Special case   
    //Full flush - Full barrier Release Persistency (on the critical path)
    if(pmodel == FBRP && level==0){
        //Nedd to flush all cachelines with epoch id > min epoch id
        //Need to free the syncmap
        if(ins_mem->mem_type==WR && ins_mem->atom_type==FULL){
            fullBarrierFlush(cache_cur, ins_mem->epoch_id, core_id);
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
                              persist(cache_cur, ins_mem, line_cur, core_id);
                            //also need to write back this cache line                            
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
                    delay[core_id] += inval_children(cache_cur, ins_mem, core_id);
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
                       delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
                    
                    }    
                    else {
                       delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp, core_id);
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
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //Read
        else {
            //ERROR COHERENCE - MAHESH //Can be handled with Exclusive as well //New Error: New Error when level=0           
                if (line_cur->state != S) {
                    delay[core_id] += share_children(cache_cur, ins_mem, core_id); //Add core_id             
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
                        //Dont do anything!.
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
        //Invariant: If something is in the cacheline, highler levels should also ahve it. Inclusive caches.
        if(ins_mem->mem_type == WB){
            return I;
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
            if(line_cur->state == M || line_cur->state == E) {                
                cache_cur->incWbCount();
                if (level == num_levels-1) {
                    delay[core_id] += inval_children(cache_cur, &ins_mem_old, core_id); //Previous One
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
                        
                        printf("Release Persistency in Replacement \n");
                        //delay[core_id] += releasePersist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline  
                        if(pmodel == BEPB){
                            //delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //BEP-on repacement
                            //Update 2019/05/16 - Delay 
                            persist(cache_cur, &ins_mem_old, line_cur, core_id); //BEP-on repacement
                        }
                        else if(pmodel == RLSB){
                            if(ins_mem_old.atom_type !=NON){
                                //delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                                persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                            }
                        }
                        else if(pmodel == FBRP){
                            if(ins_mem_old.atom_type !=NON){
                                delay[core_id] += persist(cache_cur, &ins_mem_old, line_cur, core_id); //Whichline
                                //hasReleasePersisted = true;
                            }
                        }
                        else{
                            //NOP do nothing.
                        }

                        printf("Release Persistency in Replacement \n");

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
                    ins_mem_old.mem_type = WB;
                    //printf("F\n");
                         int before_delay = delay[core_id];
                    //printf("G\n"); 
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem_old, timer + delay[core_id]); 
                        
                        int after_delay = delay[core_id];
                        int delay_wb = after_delay - before_delay;
                        cache_cur->natural_eviction_count++; //Naturally evicted. not in the critical path of RP.
                        cache_cur->natural_eviction_delay += delay_wb;

                        if(level ==0){
                            if(pmodel == RLSB){ //Becuase write-backs happenes in the criticial path
                                delay[core_id] = before_delay; //No write-back cost. happenes outside the critical path of execution
                            }else if(pmodel !=NONB){
                                if(core_id ==0){
                                    delay[core_id] = before_delay;  //Check this correctness
                                }
                            }
                        }

                        if(level == 0){
                            cache_cur->write_back_count++;
                            cache_cur->write_back_delay+=delay_wb;

                            if(pmodel == RLSB || pmodel ==NONB){
                                cache_cur->noncritical_write_back_count++;     
                                cache_cur->noncritical_write_back_delay += delay_wb;
                            }
                            //This is also not in the critical path. For BEP evictions are lower.
                            else if(pmodel == BEPB || pmodel ==FBRP){ //This should also belongs to noncritical since conflicting blokc
                                cache_cur->critical_write_back_count++;     
                                cache_cur->critical_write_back_delay += delay_wb;
                            }

                        }


                    line_cur->state = I;
                    line_cur->dirty = 0;
                    line_cur->atom_type = NON;                                       
                    //printf("B\n");
                    /////////////////////////////////////////////////////////////////////////////////////////// 
                }
                //Something is missing here. Intermediate modifided data ust be written-back to memory when the cacheline is replaced?.
                //Can a cacheline be replaced randomly in the intermediate stage. //FINAL ERROR : [MESI-INVAL] Release -> Invalidating address [0x19]/[0xca0e8] of cache [1] in level [1]                
            }
        }
                
        line_cur->timestamp = timer + delay[core_id];
        
        if (level != num_levels-1) {
            line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                   cache_id*cache_level[level].share/cache_level[level+1].share, core_id, 
                                   ins_mem, timer);

            
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


int System::fullBarrierFlush(Cache *cache_cur, int epoch_id, int req_core_id){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }
    
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
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 

    //same cores.
    
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

//Buffered Epoch Persistence
int System::epochPersist(Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id){ //calling 
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
    //delay[core_id] = 0;   //Check this. Need to chaneg.  
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

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


                    printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                    i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 
    //Check. Need to flush the calling line as well
    printf("Here \n");

            //Counts
    if(core_id == req_core_id){
        cache_cur->intra_conflicts++;
        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{
        cache_cur->inter_conflicts++;
        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        //New
        cache_cur->write_back_count += persist_count;
        cache_cur->noncritical_write_back_count += persist_count; //Is this true

        cache_cur->write_back_delay += delay[core_id];
        cache_cur->noncritical_write_back_delay += delay[core_id];
        
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


int System::fullFlush(Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }
    
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
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount();
                        persist_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 

    //Counts
    if(core_id == req_core_id){
        cache_cur->intra_conflicts++;
        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //New
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{ // Inter conflicts are not in the sritical path of the receiving core.
        cache_cur->inter_conflicts++;
        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 


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


int System::releaseFlush(Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id){
    //Need to send write-back instruction to all upper level. from M or E state
    //Write-back
    int delay_flush = 0;
    delay_flush++;
    int core_id = cache_cur->getId();
    int cache_id = cache_cur->getId();
    Line *line_cur;
    int timer = 0;
    #ifdef DEBUG
    printf("Delay on flush %d of core %d by core %d \n", delay[core_id], core_id, req_core_id);
    #endif
    //delay[core_id] = 0;
    if(core_id != req_core_id){
        delay[core_id]  = 0;
    }

    int level = cache_cur->getLevel(); 
    int persist_count =0;   

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

                        printf("[Release Persist] [%lu][%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                        i,j, line_cur->min_epoch_id, line_cur->max_epoch_id, line_cur->dirty, line_cur->tag, line_cur->state, line_cur->atom_type);                                        
                     
                        //uint64_t address_tag = line_cur->tag;
                        ins_mem.mem_type = WB;   
                        ins_mem.atom_type = NON;                                             
                        line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, &ins_mem, timer + delay[core_id]);
                        
                        cache_cur->incPersistCount(); //all
                        persist_count++;
                        //cache_cur->critical_conflict_persists++; //But not in the critical path of this core
                        cache_cur->write_back_count++;
                        cache_cur->noncritical_write_back_count++;
                        
                        line_cur->dirty = 0;
                        line_cur->state = I;
                    }
                }
        }
    } 

    printf("Here\n"); 

     //Counts
    if(core_id == req_core_id){
        cache_cur->intra_conflicts++;
        cache_cur->intra_persists += persist_count;
        cache_cur->intra_persist_cycles += delay[core_id]; //Not correct, but not effects the core

        //New
        cache_cur->critical_conflict_persists += persist_count; //Should be equal to both intra-thread and inter-thread
        cache_cur->critical_conflict_persist_cycles += delay[core_id]; //Delay

        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->critical_write_back_count += persist_count;       
        cache_cur->critical_write_back_delay += delay[core_id];
        
    }else{ // Inter conflicts are not in the sritical path of the receiving core.
        cache_cur->inter_conflicts++;
        cache_cur->inter_persists += persist_count;
        cache_cur->inter_persist_cycles += delay[core_id]; 

        //New
        cache_cur->write_back_count += persist_count;
        cache_cur->write_back_delay += delay[core_id];

        cache_cur->noncritical_write_back_count += persist_count; //Is this true        
        cache_cur->noncritical_write_back_delay += delay[core_id];
        
        cache[0][req_core_id]->external_critical_wb_count += persist_count;
        cache[0][req_core_id]->external_critical_wb_delay += delay[core_id];        
    }

    #ifdef DEBUG
    printf("Number of persists on release : %d \n", persist_count);    
    #endif
    
    return delay[core_id];
}

//Mist return delay

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


int System::releasePersist(Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id){

    int delay_tmp = 0;
    
    #ifdef DEBUG
    cache_cur->printSyncMap();
    #endif

    SyncLine *syncline = cache_cur->searchSyncMap(ins_mem->addr_dmem); //Caclculate constant delay here.
    
    //assert(syncline != NULL); - Mahesh 2019/04/15 Becuase of exclusive state
    if(syncline !=NULL){
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
        printf("Release Persistence \n");
        delay_tmp = releaseFlush(cache_cur, syncline, line_cur, rel_epoch_id, req_core_id); 
        cache_cur->incPersistDelay(delay_tmp); //Counters
        //delay_tmp =  delay_tmp/2;
    }
    if(pmodel == FBRP){ //Epoch PErsistency
        printf("Full barrier Release Persistence \n");
        delay_tmp = releaseFlush(cache_cur, syncline, line_cur, rel_epoch_id, req_core_id); 
        cache_cur->incPersistDelay(delay_tmp); //Counters
        //delay_tmp =  delay_tmp/2;
    }
    else if(pmodel == FLLB){
        //printf("Epoch Persistence \n");
        delay_tmp = fullFlush(cache_cur, syncline, line_cur, rel_epoch_id, req_core_id); //Full barrier
        cache_cur->incPersistDelay(delay_tmp); //Counters
        //delay_tmp =  delay_tmp/2;
    }
    else
    {
         //printf("No Persistence \n");
        //NO need to check syncmap
        delay_tmp = 0;
    }

    printf("HERE Persistence \n");
    
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
int System::share(Cache* cache_cur, InsMem* ins_mem, int core_id)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line*  line_cur;
    
    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();

        if (line_cur != NULL && (line_cur->state == M || line_cur->state == E)) {
            
             for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = share(cache_cur->child[i], ins_mem, core_id);
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
                //Eviction implement: line_cur->atom_type == RELEASE -> DO not have this information
                //Before Write-back.
                printf("[Share] Release Persist Starting.... and effected core \n");
                #ifdef DEBUG
                              
                    printf("[MESI-SHARE] Atom- %d, State- %d -> Downgrading address [0x%lx]/[0x%lx] of cache [%d] in level [%d], Requesting core:%d\n", 
                            line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel(), core_id);
               #endif

                //delay += releasePersist(cache_cur, ins_mem, line_cur,core_id); // need both Acquire and Releas
                if(pmodel == BEPB) delay += persist(cache_cur, ins_mem, line_cur,core_id); // need both Acquire and Release // Persistence Model
                else if(line_cur->atom_type !=NON) delay += persist(cache_cur, ins_mem, line_cur,core_id);
                printf("[Share-Finish] Release Persist Starting.... \n");

            }

            //Need more care here to write-back the release andacquires.

            line_cur->state = S; //Check this
            //Update atomic type
            line_cur->atom_type = NON;     
            line_cur->dirty = 0;   
            
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

int System::share_children(Cache* cache_cur, InsMem* ins_mem, int core_id)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = share(cache_cur->child[i], ins_mem, core_id);
            if (delay_tmp > delay_max) {
                delay_max = delay_tmp;
            }
        }
        delay += delay_max;
    }
    return delay;
}



// This function propagates down invalidate state
int System::inval(Cache* cache_cur, InsMem* ins_mem, int core_id)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line* line_cur;

    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();
        if (line_cur != NULL) {
            
            for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = inval(cache_cur->child[i], ins_mem, core_id);
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
                    printf("[Inval] Release Persist Starting.... \n");
                    #ifdef DEBUG                    
                    printf("\n[MESI-INVAL] Atom- %d, State- %d, Invalidating address [0x%lx]/[0x%lx] of cache [%d] in level [%d]\n", 
                        line_cur->atom_type, line_cur->state ,line_cur->tag, ins_mem->addr_dmem, cache_cur->getId(), cache_cur->getLevel());
                    
                    #endif
                    //delay += releasePersist(cache_cur, ins_mem, line_cur, core_id); // need both Acquire and Releas
                    if (pmodel == BEPB) delay += persist(cache_cur, ins_mem, line_cur, core_id); // need both Acquire and Release
                    else if (line_cur->atom_type != NON) delay += persist(cache_cur, ins_mem, line_cur, core_id); 
                    printf("[Inval-Finish] Release Persist Starting.... \n");
                }//Delay of invalidation and shar is added to the cycles by coherence itdels.
                //No need to write-back 
                
            }
                  
            line_cur->state = I;
            //update_atomic
            line_cur->atom_type = NON; // If removed Invalidation messages will be printed
            line_cur->dirty = 0;
            
            
            delay += delay_max;
        }
        cache_cur->unlockDown(ins_mem);
    }
    return delay;
}

// This function propagates down invalidate state starting from children nodes
int System::inval_children(Cache* cache_cur, InsMem* ins_mem, int core_id)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = inval(cache_cur->child[i], ins_mem, core_id);
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
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
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
                for (int i = 0; i < num_cores; i++) { 
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
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem, core_id);
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
                    delay_temp += inval(cache[num_levels-1][i], ins_mem, core_id);
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
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id); //This handles Hit-Read problem of LLC 
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
                printf("writeback 0x%lx \n",ins_mem->addr_dmem);
                delay += dram.access(ins_mem);
                line_cur->state = I;
                line_cur->sharer_set.clear();
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
        delay += dram.access(ins_mem);
    }   
    //Shard llc hit
    else {
        if (ins_mem->mem_type == WR) {
            if ((line_cur->state == M) || (line_cur->state == E)) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem, core_id);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
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
            }
            else if (line_cur->state == S) {
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
        else if((ins_mem->mem_type == WB)){
            printf("writeback 0x%lx \n",ins_mem->addr_dmem);
            delay += dram.access(ins_mem);
            line_cur->state = I;
            line_cur->sharer_set.clear();            
        }
        //Writeback 
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


void System::report(ofstream* result)
{
    int i, j;
    uint64_t ins_count = 0, miss_count = 0, evict_count = 0, wb_count = 0, persist_count=0, persist_delay=0, intra_tot =0, inter_tot=0;
    uint64_t intra_persist_tot=0, inter_persist_tot=0, intra_pcycles_tot=0, inter_pcycles_tot=0;
    double miss_rate = 0;
    double  ratio_inter_intra_conflicts = 0.0;
    double  ratio_inter_intra_persists = 0.0;
    double  ratio_inter_intra_persist_cycles = 0.0;
    //double conflict_rate=0.0;
    uint64_t clwb_tot=0, critical_clwb=0, noncritical_clwb=0, external_clwb=0, natural_clwb=0, sync_clwb=0, critical_persist_wb=0; //last one id related to total persist count
    uint64_t delay_clwb_tot=0, delay_critical_clwb=0, delay_noncritical_clwb=0, delay_external_clwb=0, delay_natural_clwb=0, delay_sync_clwb=0, delay_critical_persist_wb=0;
   
   uint64_t epoch_sum_tot = 0;
   uint64_t epoch_id_tot = 0, epoch_id2_tot=0;
   
    network.report(result); 
    dram.report(result); 
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
        sync_clwb = 0;
        critical_persist_wb = 0;
        delay_clwb_tot = 0;
        delay_critical_clwb = 0;
        delay_noncritical_clwb = 0;
        delay_external_clwb = 0;
        delay_natural_clwb = 0;
        delay_sync_clwb = 0;

        epoch_sum_tot = 0;
        epoch_id_tot = 0;
        epoch_id2_tot = 0;

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

                clwb_tot            +=    cache[i][j]->write_back_count; 
                critical_clwb       +=   cache[i][j]->critical_write_back_count; 
                noncritical_clwb    +=    cache[i][j]->noncritical_write_back_count; 
                external_clwb       +=   cache[i][j]->external_critical_wb_count; 
                natural_clwb        +=    cache[i][j]->natural_eviction_count; 
                sync_clwb           +=   cache[i][j]->sync_conflict_persists; 
                critical_persist_wb +=     cache[i][j]->critical_conflict_persists;


                delay_clwb_tot              +=      cache[i][j]->write_back_delay;
                delay_critical_clwb         +=      cache[i][j]->critical_write_back_delay;
                delay_noncritical_clwb      +=      cache[i][j]->noncritical_write_back_delay;
                delay_external_clwb         +=      cache[i][j]->external_critical_wb_delay;
                delay_natural_clwb          +=      cache[i][j]->natural_eviction_delay;
                delay_sync_clwb             +=      cache[i][j]->sync_conflict_persist_cycles;
                delay_critical_persist_wb   +=      cache[i][j]->critical_conflict_persist_cycles;

                epoch_sum_tot += cache[i][j]->epoch_sum;
                epoch_id_tot += cache[i][j]->epoch_updated_id;
                epoch_id2_tot += cache[i][j]->epoch_counted_id;


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
        *result << "=----------------------------------------------------------------------\n";
        *result << "Inter-thread conflicts: " <<  inter_tot << endl;
        *result << "Intra-thread conflicts: " <<  intra_tot << endl;
        *result << "Inter-Intra ratio (SUM): " << (double)inter_tot/(inter_tot+intra_tot) << endl;
        *result << "Inter-Intra ratio (AVG): " << ratio_inter_intra_conflicts/cache_level[i].num_caches << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Inter-thread Persists: " <<  inter_persist_tot << endl;
        *result << "Intra-thread Persists: " <<  intra_persist_tot << endl;
        *result << "Inter-Intra P ratio (SUM): " << (double)inter_persist_tot/(inter_persist_tot+intra_persist_tot) << endl;
        *result << "Inter-Intra P ratio (AVG): " << ratio_inter_intra_persists/cache_level[i].num_caches << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Inter-thread Persist Cycles: " <<  inter_pcycles_tot << endl;
        *result << "Intra-thread Persist Cycles: " <<  intra_pcycles_tot << endl;
        *result << "Inter-Intra PC ratio (SUM): " << (double)inter_pcycles_tot/(inter_pcycles_tot+intra_pcycles_tot) << endl;
        *result << "Inter-Intra PC ratio (AVG): " << ratio_inter_intra_persist_cycles/cache_level[i].num_caches << endl;
        *result << "-----------------------------------------------------------------------\n";
        *result << "Write-Backs count and delay " <<  clwb_tot << " \t" << delay_clwb_tot << endl;
        *result << "Critical Write-Backs count and delay " <<  critical_clwb << " \t" << delay_critical_clwb << endl;
        *result << "NonCritical Write-Backs count and delay " <<  noncritical_clwb << " \t" << delay_noncritical_clwb << endl;
        *result << "External Write-Backs count and delay " <<  external_clwb << " \t" << delay_external_clwb << endl;
        *result << "Natural Write-Backs count and delay " <<  natural_clwb << " \t" << delay_natural_clwb << endl;
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
        *result << "=================================================================\n\n";

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
