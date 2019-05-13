//===========================================================================
// cache.cpp simulates a single cache with LRU replacement policy. 
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
#include <sys/time.h>
#include <time.h>

#include "cache.h"
#include "common.h"

using namespace std;




void Cache::init(XmlCache* xml_cache, CacheType cache_type_in, int bus_latency, int page_size_in, int level_in, int cache_id_in)
{
    ins_count = 0;;
    miss_count = 0;
    evict_count = 0;
    wb_count = 0;
    size = xml_cache->size;
    num_ways = xml_cache->num_ways;
    block_size = xml_cache->block_size;
    num_sets = size / (block_size*num_ways);
    access_time = xml_cache->access_time;
    cache_type = cache_type_in;
    page_size = page_size_in;
    level = level_in;
    cache_id = cache_id_in;
    pmodel = FLLB;  //Full barrier semantics by default
    persist_count = 0;

        intra_conflicts         = 0;
        intra_persists          = 0;
        intra_persist_cycles    = 0;
        inter_conflicts         = 0;
        inter_persists          = 0;
        inter_persist_cycles    = 0;
        rmw_acq_count           = 0;
        rmw_rel_count           = 0;
        wrt_rel_count           = 0;
        rmw_full_count          = 0 ;
        rmw_count               = 0;
        wrt_relaxed_count       = 0;

        sync_conflict_count     = 0;
        sync_conflict_persists  = 0;
        sync_conflict_persist_cycles = 0;

        critical_conflict_count = 0;
        critical_conflict_persists = 0;
        critical_conflict_persist_cycles = 0;

        write_back_count = 0;
        write_back_delay = 0; //Write-backs on the critical path

        critical_write_back_count = 0;
        critical_write_back_delay = 0;
        noncritical_write_back_count = 0;
        noncritical_write_back_delay = 0;

        external_critical_wb_count = 0;
        external_critical_wb_delay = 0;

        //To check number of evictions
        natural_eviction_count = 0; //conflicts are not included.

        //epoch info
        epoch_sum = 0;
        epoch_max = 0;
        epoch_min = 0;
        epoch_size = 0;
        epoch_last = 0;

    if (cache_type == TLB_CACHE) {
        offset_bits = (int) (log2(page_size));
        offset_mask = (uint64_t)(page_size - 1);
    }
    else {
        offset_bits = (int) (log2(block_size));
        offset_mask = (uint64_t)(block_size - 1);
    }
    index_bits  = (int) (log2(num_sets));
    index_mask  = (uint64_t)(num_sets - 1);

    if (cache_type == DATA_CACHE || cache_type == DIRECTORY_CACHE) {
        line = new Line* [num_sets];
        lock_up = new pthread_mutex_t [num_sets];
        lock_down = new pthread_mutex_t [num_sets];
        for (uint64_t i = 0; i < num_sets; i++ ) {
            pthread_mutex_init(&lock_up[i], NULL);
            pthread_mutex_init(&lock_down[i], NULL);
            line[i] = new Line [num_ways];
            for (uint64_t j = 0; j < num_ways; j++ ) {
                line[i][j].state = I;
                line[i][j].id = 0;
                line[i][j].set_num = i;
                line[i][j].way_num = j;
                line[i][j].tag = 0;
                line[i][j].ppage_num = 0;
                line[i][j].timestamp = 0;
                line[i][j].atom_type = NON;
                line[i][j].min_epoch_id = 0;
                line[i][j].max_epoch_id = 0;
            }
        }

        if(cache_type == DATA_CACHE && level==0){
            initSyncMap(SYNCMAP_SIZE); // Need the size
        }
        
    }
    else if (cache_type == TLB_CACHE) {
        line = new Line* [num_sets];
        for (uint64_t i = 0; i < num_sets; i++ ) {
            line[i] = new Line [num_ways];
            for (uint64_t j = 0; j < num_ways; j++ ) {
                line[i][j].state = I;
                line[i][j].id = 0;
                line[i][j].set_num = i;
                line[i][j].way_num = j;
                line[i][j].tag = 0;
                line[i][j].ppage_num = 0;
                line[i][j].timestamp = 0;
            }
        }
    }
    else {
        cerr << "Error: Undefined cache type!\n";
    }
    if (xml_cache->share > 1) {
        bus = new Bus;
        bus->init(bus_latency);
    }
    else {
        bus = NULL;
    }
}



Line* Cache::findSet(int index)
{
    assert(index >= 0 && index < (int)num_sets);
    return line[index];
}

int Cache::reverseBits(int num, int size)
{
    int reverse_num = 0;
    for (int i = 0; i < size; i++) {
        if((num & (1 << i))) {
           reverse_num |= 1 << ((size - 1) - i);  
        }
   }
   return reverse_num;
}

// This function parses each memory reference address into three separate parts
// according to the cache configuration: offset, index and tag.

void Cache::addrParse(uint64_t addr_in, Addr* addr_out)
{

    addr_out->offset =  addr_in & offset_mask;
    addr_out->tag    =  addr_in >> (offset_bits + index_bits);
    //hash
    addr_out->index  = (addr_in >> offset_bits) % num_sets;
}

// This function composes three separate parts: offset, index and tag into
// a memory address.

void Cache::addrCompose(Addr* addr_in, uint64_t* addr_out)
{
    //recover
    *addr_out = addr_in->offset 
              | (addr_in->index << offset_bits) 
              | (addr_in->tag << (offset_bits+index_bits));
}

// This function implements the LRU replacement policy in a cache set.

int Cache::lru(uint64_t index)
{
    uint64_t i; 
    int min_time = 0;
    Line* set_cur = findSet(index);
    assert(set_cur != NULL);
    for (i = 1; i < num_ways; i++) {
        if (set_cur[i].timestamp < set_cur[min_time].timestamp) {
            min_time = i;
        }
    }
    return min_time;
}


// This function returns a pointer to the cache line the instruction want to access,
// NULL is returned upon a cache miss.
Line* Cache::accessLine(InsMem* ins_mem)
{
    uint64_t i;
    Line* set_cur;
    Addr addr_temp;

    addrParse(ins_mem->addr_dmem, &addr_temp);
    set_cur = findSet(addr_temp.index);
    assert(set_cur != NULL);
    for (i = 0; i < num_ways; i++) {

        if( (set_cur[i].id == ins_mem->prog_id)
        &&  (set_cur[i].tag == addr_temp.tag)
        &&   set_cur[i].state) {
            return &set_cur[i];
        }
    }
    return NULL;
}
// This function replaces the cache line the instruction want to access and returns
// a pointer to this line, the replaced content is copied to ins_mem_old.
Line* Cache::replaceLine(InsMem* ins_mem_old, InsMem* ins_mem)
{
    Addr addr_old;
    Addr addr_temp;
    int way_rp;
    uint64_t i, addr_dmem_old;
    Line* set_cur;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    set_cur = findSet(addr_temp.index);
    
    for (i = 0; i < num_ways; i++) {
        if (set_cur[i].state == I) {
           set_cur[i].id = ins_mem->prog_id; 
           set_cur[i].tag = addr_temp.tag;
           
           //Add atomic operations support: Acquire and Release
           set_cur[i].atom_type =ins_mem->atom_type;
           if(set_cur[i].atom_type == RELEASE){
            //printf("\n[MESI-REPLACE] Atomic operations 0x%lx \n\n", ins_mem->addr_dmem);
           }
           
           
           return &set_cur[i]; 
        }
    }
    way_rp = lru(addr_temp.index);
    addr_old.index = addr_temp.index;
    addr_old.tag = set_cur[way_rp].tag;
    addr_old.offset = 0;
    addrCompose(&addr_old, &addr_dmem_old);
    
    ins_mem_old->prog_id = set_cur[way_rp].id;
    ins_mem_old->addr_dmem = addr_dmem_old;
    
    //Save old atomic type
    ins_mem_old->atom_type = set_cur[way_rp].atom_type;
    //
    set_cur[way_rp].id = ins_mem->prog_id; 
    set_cur[way_rp].tag = addr_temp.tag;

    //Set atomic type
    set_cur[way_rp].atom_type = ins_mem->atom_type; //Holy Shit!!
    //
    #ifdef DEBUG
    if(level==0 && cache_id>=0 && ins_mem_old->atom_type !=NON){
            printf("\n[MESI-REPLACE] replacements - Address 0x%lx by Address 0x%lx cache %d of level %d\n", ins_mem_old->addr_dmem, ins_mem->addr_dmem, cache_id, level);
    }
    #endif

    if(ins_mem_old->atom_type == RELEASE){
            //printf("\n[MESI-REPLACE] Replaced Atomic operations 0x%lx : Address 0x%lx by Address 0x%lx \n", ins_mem->addr_dmem, ins_mem_old->addr_dmem, ins_mem->addr_dmem);
    }

    return &set_cur[way_rp]; 
}

//Direct access to a specific line
Line* Cache::directAccess(int set, int way, InsMem* ins_mem)
{
    assert(set >= 0 && set < (int)num_sets);
    assert(way >= 0 && way < (int)num_ways);
    //printf("Direct Access %d %d\n",set,way );
    Line* set_cur;
    set_cur = findSet(set);
    if (set_cur == NULL) {
        return NULL;
    }
    else if (set_cur[way].state) {
        Addr addr;
        uint64_t addr_dmem;
        addr.index = set;
        addr.tag = set_cur[way].tag;
        addr.offset = 0;
        addrCompose(&addr, &addr_dmem);
        
        ins_mem->prog_id = set_cur[way].id;
        ins_mem->addr_dmem = addr_dmem;
        ins_mem->mem_type = 0;
        ins_mem->epoch_id = 0;
        ins_mem->atom_type = NON;
        
        return &set_cur[way];
    }
    else {
        return NULL;
    }
}

void Cache::flushAll()
{
    for (uint64_t i = 0; i < num_sets; i++) {
        for (uint64_t j = 0; j < num_sets; j++) {
            line[i][j].state = I;
            line[i][j].id = 0;
            line[i][j].tag = 0;
            line[i][j].sharer_set.clear();
        }
    }
}

Line* Cache::flushLine(int set, int way, InsMem* ins_mem_old)
{
    assert(set >= 0 && set < (int)num_sets);
    assert(way >= 0 && way < (int)num_ways);
    Line* set_cur;
    set_cur = findSet(set);
    if (set_cur == NULL) {
        return NULL;
    }
    else if(set_cur[way].state) {
        set_cur[way].state = I;
        set_cur[way].id = 0;
        set_cur[way].tag = 0;
        set_cur[way].sharer_set.clear();
        Addr addr_old;
        uint64_t addr_dmem_old;
        addr_old.index = set;
        addr_old.tag = set_cur[way].tag;
        addr_old.offset = 0;
        addrCompose(&addr_old, &addr_dmem_old);
        
        ins_mem_old->prog_id = set_cur[way].id;
        ins_mem_old->addr_dmem = addr_dmem_old;

        return &set_cur[way];
    }
    else {
        return NULL;
    }
}

Line* Cache::flushAddr(InsMem* ins_mem)
{
    Line* cur_line;
    cur_line = accessLine(ins_mem);
    if (cur_line != NULL) {
        cur_line->state = I;
        cur_line->id = 0;
        cur_line->tag = 0;
        cur_line->sharer_set.clear();
        return cur_line;
    }
    else {
        return NULL;
    }
}

void Cache::lockUp(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    assert(addr_temp.index >= 0 && addr_temp.index < num_sets);
    //pthread_mutex_lock(&lock_up[addr_temp.index]);
}

void Cache::unlockUp(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    assert(addr_temp.index >= 0 && addr_temp.index < num_sets);
    //pthread_mutex_unlock(&lock_up[addr_temp.index]);
}

void Cache::lockDown(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    pthread_mutex_lock(&lock_down[addr_temp.index]);
}

void Cache::unlockDown(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    pthread_mutex_unlock(&lock_down[addr_temp.index]);
}

int  Cache::getAccessTime()
{
    return access_time;
}

uint64_t Cache::getSize()
{
    return size;
}

uint64_t Cache::getNumSets()
{
    return num_sets;
}
        
uint64_t Cache::getNumWays()
{
    return num_ways;
}

uint64_t Cache::getBlockSize()
{
    return block_size;
}

int Cache::getOffsetBits()
{
    return offset_bits;
}
        
int Cache::getIndexBits()
{
    return index_bits;
}

void Cache::incInsCount()
{
    ins_count++;
}

void Cache::incMissCount()
{
    miss_count++;
}

void Cache::incWbCount()
{
    wb_count++;
}

void Cache::incEvictCount()
{
    evict_count++;
}

uint64_t Cache::getInsCount()
{
    return ins_count;
}

uint64_t Cache::getMissCount()
{
    return miss_count;
}

uint64_t Cache::getWbCount()
{
    return wb_count;
}

uint64_t Cache::getEvictCount()
{
    return evict_count;
}

int Cache::getLevel(){
    return level;
}

int Cache::getId(){
    return cache_id;
}

uint64_t Cache::getPersistCount(){
    return persist_count;
}

uint64_t Cache::getPersistDelay(){
    return persist_delay;
}

void Cache::incPersistCount(){
    persist_count++;
}

void Cache::incPersistDelay(int delay){
    persist_delay= persist_delay+delay;
}
////////////////////////////////////////////////////////////////////////////////////////////
//SyncMap Functions
int Cache::initSyncMap(int size){
    syncmap.size = 0;
    syncmap.head = NULL;    
    syncmap.epoch_id = 0;  
    printf("Initialized Sync Map with size %d", SYNCMAP_SIZE);  
    return 0;
}

void Cache::printSyncMap(){
    SyncLine *tmp_line = syncmap.head;
        //bool found = false;
    printf("------------------------------------------------------------[%d, %d]\n", cache_id, level);  
    printf("ID \t Epoch \t Address \t Atom \t Tag \t Op \t Type \n");  
        while (tmp_line !=NULL){            
               printf("%d \t %d \t 0x%lx \t %d \t 0x%lx \t %d \t %d \n", tmp_line->sync_id, tmp_line->epoch_id, tmp_line->address, tmp_line->atom_type, tmp_line->tag, tmp_line->mem_op, tmp_line->mem_type);  
                    //Need more logic here           
                tmp_line = tmp_line->next;
        }
    printf("------------------------------------------------------------[%d, %d]\n", cache_id, level);
}

void Cache::printCache(){
    printf("Cache \n");
    
    for (uint64_t i = 0; i < num_sets; i++) {      
        printf("------------------------------------------------------- %lu \n", i);      
            for (uint64_t j = 0; j < num_ways; j++) {
                
                if(line[i][j].state == M || line[i][j].state == E ){
                    printf("[%lu] Min-%d Max-%d Dirty-%d Addr-0x%lx State-%d Atom-%d \n", 
                        j, line[i][j].min_epoch_id, line[i][j].max_epoch_id, line[i][j].dirty, line[i][j].tag, line[i][j].state, line[i][j].atom_type);                                        
                }
        }
    }
    
}

SyncLine * Cache::createSyncLine(InsMem *ins_mem){
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    SyncLine * syncline = (SyncLine*) malloc(sizeof(SyncLine)); 
    syncline->address = ins_mem->addr_dmem;
    syncline->atom_type = ins_mem->atom_type;
    syncline->prog_id = ins_mem->prog_id;
    syncline->epoch_id = ins_mem->epoch_id;  
    syncline->tag       = addr_temp.tag;
    syncline->mem_op    = ins_mem->mem_op;
    syncline->mem_type  = ins_mem->mem_type;
    syncline->sync_id   = syncmap.size % SYNCMAP_SIZE ;
     
    return syncline;
}

int Cache::replaceSyncLine(SyncLine * syncline, InsMem * ins_mem){
    
    if(syncline !=NULL){
        //syncline->address = ins_mem->addr_dmem;
        syncline->atom_type = ins_mem->atom_type;
        //syncline->prog_id = ins_mem->prog_id;
        syncline->epoch_id = ins_mem->epoch_id;  
        //syncline->tag       = addr_temp.tag;
        syncline->mem_op    = ins_mem->mem_op;  //Check and .What happene when 3 releases conflicts? Acquire <-> Release
        syncline->mem_type  = ins_mem->mem_type; //Check and

    }
    return 0;
 }

SyncLine * Cache::addSyncLine(InsMem * ins_mem){    
    
    assert(ins_mem->atom_type != NON);
    //assert(syncmap.size < SYNCMAP_SIZE);
    SyncLine * new_line = NULL;
    //Before- syncmap.size < SYNCMAP_SIZE
    if(true){       
        //SyncLine * 
        new_line = createSyncLine(ins_mem);
        //check and overwrite syncs to same address from previous epochs
        SyncLine *tmp_line = syncmap.head;
        //Assmption only 1 sync atomic variable at a time
        while (tmp_line !=NULL){
            if(tmp_line->address == new_line->address){

                #ifdef DEBUG
                printf("[SyncMap] Overwrite Sync 0x%lx of atomic-type %d/%d in cache %d of Level %d \n",                
                    tmp_line->address, tmp_line->atom_type, new_line->atom_type, cache_id, level);   
                #endif
                    //Need more logic here
                    replaceSyncLine(tmp_line,ins_mem);
                    return tmp_line; // Existing line    
            }
                tmp_line = tmp_line->next;
        }

        if(syncmap.size < SYNCMAP_SIZE){  
        
            new_line->next = syncmap.head;
            syncmap.head = new_line;
            syncmap.size++;
            return new_line;
            #ifdef DEBUG
            printf("[SyncMap] Adding Sync Line 0x%lx to cache %d at level %d, atomic-type: %d ID:%d \n", ins_mem->addr_dmem, cache_id, level, ins_mem->atom_type, new_line->sync_id);  
            #endif
        }else{
            printf("[SyncMap] overflow %d %d", cache_id, level); 
            return new_line;     
        }
    }
    else{
        //Wait. Stall the until something get flushed.//No incident of races to same sync variable. Becuase of Data Race Free execution. 
        //FLUSH-with conflicts
        printf("[SyncMap] NULL MAP %d %d", cache_id, level); 
        return new_line;      
    }      
    //return 0;
}

void Cache::deleteFromSyncMap(uint64_t addr){
    
    SyncLine * tmp_line = syncmap.head;
    SyncLine *prev_line = syncmap.head;

    Addr addr_block;
    addrParse(addr, &addr_block);

        while (tmp_line !=NULL){
            
            Addr tmp_addr;
            addrParse(tmp_line->address, &tmp_addr);                    
            //if(tmp_line->address == addr){
            if(tmp_addr.tag == addr_block.tag && tmp_addr.index==addr_block.index){

                #ifdef DEBUG
                printf("[SyncMap] Deleting 0x%lx of atomic-type %d  0x%lx in cache %d of Level %d Tag: ox%lx SyncTag: ox%lx  \n", 
                    tmp_line->address, tmp_line->atom_type, addr, cache_id, level, addr_block.tag, tmp_line->tag);
                #endif
                    
                    if(tmp_line != syncmap.head) prev_line->next = tmp_line->next;
                    else syncmap.head = tmp_line->next;
                    syncmap.size--;
                    //break;          
            }    

            if(tmp_line != syncmap.head) prev_line = tmp_line;
            tmp_line = tmp_line->next;
        }   
        
}

void Cache::deleteLowerFromSyncMap(int epoch_id){
    
    SyncLine * tmp_line = syncmap.head;
    SyncLine *prev_line = syncmap.head;

        while (tmp_line !=NULL){
            
            if(tmp_line->epoch_id < epoch_id){

             
                    if(tmp_line != syncmap.head) prev_line->next = tmp_line->next;
                    else syncmap.head = tmp_line->next;
                    syncmap.size--;
                    //break;          
            }    

            if(tmp_line != syncmap.head) prev_line = tmp_line;
            tmp_line = tmp_line->next;
        }   
        
}

SyncLine * Cache::searchSyncMap(uint64_t addr){ //Accurate address is Cachline Tag
    
    SyncLine *tmp_line = syncmap.head;
    Addr addr_block;
    addrParse(addr, &addr_block);
    //Only need the cacheline block TAG
    SyncLine * sync_line = NULL;
       
        while (tmp_line !=NULL){

            Addr tmp_addr;
            addrParse(tmp_line->address, &tmp_addr);            
            //if(tmp_line->address == addr){
            if(tmp_addr.tag == addr_block.tag && tmp_addr.index==addr_block.index){
                
                #ifdef DEBUG
                printf("[SyncMap] Found Sync 0x%lx of atomic-type %d 0x%lx in cache %d of Level %d  Tag: 0x%lx Index:0x%lx Call-> Tag:0x%lx  Index: 0x%lx\n", 
                    tmp_line->address, tmp_line->atom_type, addr, cache_id, level, tmp_addr.tag, tmp_addr.index, addr_block.tag, addr_block.index);
                #endif
                //Not necessary for everything
                if(sync_line ==NULL){
                    sync_line = tmp_line;
                }else{
                    if(sync_line->epoch_id < tmp_line->epoch_id) sync_line = tmp_line;
                }
                //return tmp_line;     
            }
                tmp_line = tmp_line->next;
    }   
    return sync_line; //return NULL
}

SyncLine* Cache::searchByEpochId(int epoch_id){
    
    SyncLine *tmp_line = syncmap.head;    
        while (tmp_line !=NULL){
            if(tmp_line->epoch_id == epoch_id){ //No two sync share the same epoch ID.
                printf("[SyncMap] Found EPOCH 0x%lx of atomic-type %d epoch-%d in cache %d of Level %d \n", 
                    tmp_line->address, tmp_line->atom_type, epoch_id, cache_id, level);
                return tmp_line;     
            }
                tmp_line = tmp_line->next;
    }   
    return NULL;
}

SyncLine* Cache:: matchSyncLine(Line * line_cur){
    
    //Further think about peristent syncmap
    //SyncLine * syncline;
    int syncsize = syncmap.size;
    Addr addr_temp;

    SyncLine *temp_sline; //Largest epoch id //When multiple sync variables are located within  single cache line
    int finds = 0;
    
    assert (syncsize > SYNCMAP_SIZE);
    for(int i=0;i<syncsize;i++){
        uint64_t sync_addr = syncmap.sync_lines[i].address;        
        addrParse(sync_addr, &addr_temp);
        if(addr_temp.tag == line_cur->tag){
            //Find match
            finds++;
            temp_sline = &syncmap.sync_lines[i];
            printf("Find match ");
        }
    }   
     
    return temp_sline;
    //Send finds.
}



void Cache::report(ofstream* result)
{
    *result << "=================================================================\n";
    *result << "Simulation results for "<< size << " Bytes " << num_ways
            << "-way set associative cache model:\n";
    *result << "The total # of memory instructions: " << ins_count << endl;
    *result << "The # of cache-missed instructions: " << miss_count << endl;
    *result << "The # of evicted instructions: " << evict_count << endl;
    *result << "The # of writeback instructions: " << wb_count << endl;
    *result << "The cache miss rate: " << 100 * (double)miss_count/ (double)ins_count << "%" << endl;
    *result << "The # of persist counts: " << persist_count << endl;
    *result << "The # of persist delays: " << persist_delay << endl;
    *result << "---------------------------------------------------------------------- " << endl;
    *result << "Intra-thread Conflicts : " << intra_conflicts <<endl;   
    *result << "Inter-thread Conflicts : " << inter_conflicts <<endl;  
    *result << "Inter/Intra Conflict Ratio : " << (double)inter_conflicts/(intra_conflicts+inter_conflicts) <<endl;  
    *result << "---------------------------------------------------------------------- " << endl;
    *result << "Intra-thread Persists : " << intra_persists <<endl;
    *result << "Inter-thread Persists : " << inter_persists <<endl;  
    *result << "Inter/Intra Persists Ratio : " << (double)inter_persists/(intra_persists+inter_persists) <<endl;  
    *result << "---------------------------------------------------------------------- " << endl;
    *result << "Intra-thread Persists Cycles : " << intra_persist_cycles <<endl;
    *result << "Inter-thread Persists Cycles: " << inter_persist_cycles <<endl;  
    *result << "Inter/Intra Persists Cycles Ratio : " << (double)inter_persist_cycles/(intra_persist_cycles+inter_persist_cycles) <<endl; 
    *result << "---------------------------------------------------------------------- " << endl;
    *result << "Write-Backs count and delay " <<  write_back_count << " \t" << write_back_delay << endl;
    *result << "Critical Write-Backs count and delay " <<  critical_write_back_count << " \t" << critical_write_back_delay << endl;
    *result << "NonCritical Write-Backs count and delay " <<  noncritical_write_back_count << " \t" << noncritical_write_back_delay << endl;
    *result << "External Write-Backs count and delay " <<  external_critical_wb_count << " \t" << external_critical_wb_delay << endl;
    *result << "Natural Write-Backs count and delay " <<  natural_eviction_count << " \t" << natural_eviction_delay << endl;
    *result << "Sync Write-Backs count and delay " <<  sync_conflict_persists << " \t" << sync_conflict_persist_cycles << endl;
    *result << "Critical Persist count and delay " <<  critical_conflict_persists << " \t" << critical_conflict_persist_cycles << endl;
    *result << "All perisist (Persistence)" <<  getPersistCount() << " \t" << getPersistDelay() << endl;
    *result << "=================================================================\n\n";

    *result << "RMW: " << rmw_count <<endl;
    *result << "RMW full: " << rmw_full_count <<endl;
    *result << "RMW Acq: " << rmw_acq_count <<endl;
    *result << "RMW Rel: " << rmw_rel_count <<endl;
    *result << "WRT : " << wrt_relaxed_count <<endl;
    *result << "WRT full: " << wrt_full_count <<endl;
    *result << "WRT acq: " << wrt_acq_count <<endl;
    *result << "WRT rel: " << wrt_rel_count <<endl;
    *result << "RMW/WRT: " << (double)rmw_count /wrt_relaxed_count<<endl;
    *result << "=================================================================\n\n";

    if(level==0 and cache_type==DATA_CACHE) printf("Cache ID: %d of Level %d , The # of persist counts: %lu The # of persist delays: %lu, Inter: %lu Intra: %lu Ratio %f, P-Inter-P: %lu P-Intra-P: %lu P-Ratio-P %f, PC-Inter: %lu PC-Intra: %lu PC-Ratio %f  \n" 
            , cache_id, level, persist_count, persist_delay 
            , inter_conflicts, intra_conflicts, (double)inter_conflicts/(intra_conflicts+inter_conflicts)
            , inter_persists, intra_persists, (double)inter_persists/(intra_persists+inter_persists)
            , inter_persist_cycles, intra_persist_cycles, (double)inter_persist_cycles/(intra_persist_cycles+inter_persist_cycles)
        );
}

Cache::~Cache()
{
    if (cache_type == DATA_CACHE || cache_type == DIRECTORY_CACHE) {
        for (uint64_t i = 0; i < num_sets; i++ ) {
            pthread_mutex_destroy(&lock_up[i]);
            pthread_mutex_destroy(&lock_down[i]);
            delete [] line[i];
        }
        delete [] lock_up;
        delete [] lock_down;
        delete [] line;
    }
    else if (cache_type == TLB_CACHE) {
        for (uint64_t i = 0; i < num_sets; i++ ) {
            delete [] line[i];
        }
        delete [] line;
    }
    else {
        cerr << "Error: Undefined cache type!\n";
    }
    if (bus != NULL) {
        delete bus;
    }
}





