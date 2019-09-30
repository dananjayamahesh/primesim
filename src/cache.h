//===========================================================================
// cache.h 
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

#ifndef  CACHE_H
#define  CACHE_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <pthread.h> 
#include <set>
#include <map>
#include "xml_parser.h"
#include "bus.h"
#include "common.h"
//#define SYNCMAP_SIZE 1000
#define SYNCMAP_SIZE 10000


typedef struct Addr
{
    uint64_t    index;
    uint64_t    tag;
    uint64_t    offset;
} Addr;


typedef enum State
{
    I     =   0, //invalid
    S     =   1, //shared
    E     =   2, //exclusive
    M     =   3, //modified
    V     =   4, //valid 
    B     =   5  //broadcast
} State;

typedef enum CacheType
{
    DATA_CACHE         = 0,
    DIRECTORY_CACHE    = 1,
    TLB_CACHE          = 2,
} CacheType;


typedef enum AtomicType
{
    NON                 = 0,
    ACQUIRE             = 1,
    RELEASE             = 2,
    FULL                = 3,
} AtomicType;


typedef set<int> IntSet;

typedef struct Line
{
    char        state;
    int         id;
    int         set_num;
    int         way_num;
    uint64_t    tag;
    int64_t     timestamp;
    uint64_t    ppage_num;  //only for tlb cache
    IntSet      sharer_set; //only for directory cache
    AtomicType  atom_type; //Atomic data (Acquire/Release)

    int         min_epoch_id;
    int         max_epoch_id;
    bool        dirty;
} Line;


typedef map<int, Line*> LineMap;

typedef struct InsMem
{
    char        mem_type; // 2 means writeback, 1 means write, 0 means read , 3 clwb, 4 clflush
    int         prog_id;
    int         thread_id;
    int         rec_thread_id;
    uint64_t    addr_dmem; 
    AtomicType  atom_type;
    int         epoch_id; //timestamp
    char        mem_op; //2-RMW, 1-Write, 0-Read 
    //int         min_epoch; //Should be < Byte (only for L1)
    //int         max_epoch; //Shouls be < Byte (only for L1)
} InsMem;


typedef struct SyncLine{
    uint64_t    address;
    AtomicType  atom_type;
    int         epoch_id;  
    int         prog_id; //Unwanted, but PRiME can run multiple applications.
    uint64_t    tag;
    char        mem_op; 
    char        mem_type;
    int    sync_id;

    SyncLine * next; // point to next
} SyncLine;

//Should be a linkedlist
typedef struct SyncMap{
    SyncLine sync_lines [SYNCMAP_SIZE]; //Not is use
    SyncLine * head;    
    int epoch_id;
    int size;
}SyncMap;

class Cache
{
    public:
        int         num_children;
        Cache*      parent;
        Cache**     child;
        Bus*        bus;
        void init(XmlCache* xml_cache, CacheType cache_type_in, int bus_latency, int page_size_in, int level_in, int cache_id_in);
        Line* accessLine(InsMem* ins_mem);
        Line* directAccess(int set, int way, InsMem* ins_mem);
        Line* replaceLine(InsMem* ins_mem_old, InsMem* ins_mem);
        void flushAll();
        Line* flushLine(int set, int way, InsMem* ins_mem_old);
        Line* flushAddr(InsMem* ins_mem);
        Line* findSet(int index);
        void incInsCount();
        void incMissCount();
        void incEvictCount();
        void incWbCount();
        void lockUp(InsMem* ins_mem);
        void unlockUp(InsMem* ins_mem);
        void lockDown(InsMem* ins_mem);
        void unlockDown(InsMem* ins_mem);
        int  getAccessTime();
        uint64_t getSize();
        uint64_t getNumSets();
        uint64_t getNumWays();
        uint64_t getBlockSize();
        int getOffsetBits();
        int getIndexBits();
        uint64_t getInsCount();
        uint64_t getMissCount();
        uint64_t getEvictCount();
        uint64_t getWbCount();
        void addrParse(uint64_t addr_in, Addr* addr_out);
        void addrCompose(Addr* addr_in, uint64_t* addr_out);
        int lru(uint64_t index);
        void report(ofstream* result);
        int getLevel();
        int getId();
        void printCache();

        uint64_t getPersistCount();
        uint64_t getPersistDelay();
        void incPersistCount();
        void incPersistDelay(int delay);

        //SyncMap - Synchrnoization/Atomic Map/Sync Registry
        int initSyncMap(int size);
        SyncLine * createSyncLine(InsMem *ins_mem);
        SyncLine * searchSyncMap(uint64_t addr);
        void deleteFromSyncMap(uint64_t addr);
        void deleteLowerFromSyncMap(int epoch_id);
        SyncLine* searchByEpochId(int epoch_id); //SHould returb a map
        SyncLine * matchSyncLine(Line * line_cur);
        //int addSyncLine(InsMem * ins_mem);
        SyncLine * addSyncLine(InsMem * ins_mem);
        void printSyncMap();
        int replaceSyncLine(SyncLine * syncline, InsMem * ins_mem);

        ~Cache();
        //Intra thread conflicts
        uint64_t          intra_conflicts;
        uint64_t          intra_persists;
        uint64_t          intra_persist_cycles;

        //asplos-after
        uint64_t intra_allconflicts;
        uint64_t intra_M_allconflicts;

        uint64_t intra_vis_conflicts;
        uint64_t intra_vis_allconflicts;
        uint64_t intra_evi_conflicts; 
        uint64_t intra_evi_M_conflicts; 

         uint64_t intra_evi_allconflicts; 
        uint64_t intra_evi_M_allconflicts; 
        // /uint64_t intra_M_allconflicts; 

        uint64_t intra_vis_persists;
        uint64_t intra_evi_persists; 
        uint64_t intra_evi_M_persists; 
        uint64_t intra_vis_persist_cycles;
        uint64_t intra_evi_persist_cycles;

        //Inter thread conflicts
        uint64_t          inter_conflicts;
        uint64_t          inter_M_conflicts;
        uint64_t          inter_persists;
        uint64_t          inter_M_persists;
        int64_t           inter_persist_cycles;

        //asplos-after
        uint64_t inter_allconflicts;
        uint64_t inter_M_allconflicts;

        uint64_t        rmw_acq_count; //Write acquire as well
        uint64_t        rmw_rel_count;
        uint64_t        rmw_full_count;
        uint64_t        rmw_count;

        uint64_t        wrt_rel_count;
        uint64_t        wrt_acq_count;
         uint64_t       wrt_full_count;
        uint64_t        wrt_relaxed_count;

        uint64_t        rd_count; //Reads

        uint64_t sync_conflict_count; //Sync Write backs.
        uint64_t sync_conflict_persists;
        uint64_t sync_conflict_persist_cycles;

        uint64_t critical_conflict_count; //Number of persist happens in the critical path.
        uint64_t critical_conflict_persists;
        uint64_t critical_conflict_persist_cycles;

        uint64_t same_block_count;
        uint64_t same_block_delay;

        uint64_t write_back_count;
        uint64_t write_back_delay;

        uint64_t critical_write_back_count;
        uint64_t critical_write_back_delay;

        uint64_t noncritical_write_back_count;
        uint64_t noncritical_write_back_delay;

        uint64_t external_critical_wb_count;
        uint64_t external_critical_wb_delay;

        uint64_t natural_eviction_count;
        uint64_t natural_eviction_delay;

        uint64_t epoch_sum;
        uint64_t epoch_max;
        uint64_t epoch_min;
        uint64_t epoch_size;
        uint64_t epoch_last;

        bool     epoch_min_set;
        uint64_t epoch_updated_id;
        uint64_t epoch_counted_id;

        //all counts are in cachelines
        uint64_t lowest_epoch_size;
        uint64_t largest_lowest_epoch_size;
        uint64_t num_epochs_flushed; //number of conficting epochs
        uint64_t num_nzero_epochs_flushed;
        uint64_t largest_epoch_size;
        uint64_t nzero_conflicts;

        //Epoch Size?

    private:
        int reverseBits(int num, int size);
        Line              **line;
        pthread_mutex_t   *lock_up;
        pthread_mutex_t   *lock_down;
        CacheType         cache_type;
        int               access_time;
        uint64_t          ins_count;
        uint64_t          miss_count;
        uint64_t          evict_count;
        uint64_t          wb_count;
        uint64_t*         set_access_count;
        uint64_t*         set_evict_count;
        uint64_t          size;
        uint64_t          num_ways;
        uint64_t          block_size;
        uint64_t          num_sets;
        int               offset_bits;
        int               index_bits;  
        int               page_size;
        int               level;
        int               cache_id;
        uint64_t          offset_mask;
        uint64_t          index_mask;
        PersistModel      pmodel;
        SyncMap           syncmap;

        uint64_t          persist_count;
        uint64_t          persist_delay;


};

#endif //CACHE_H
