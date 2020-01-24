//===========================================================================
// system.h 
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

#ifndef  SYSTEM_H
#define  SYSTEM_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include "xml_parser.h"
#include "cache.h"
#include "network.h"
#include "page_table.h"
#include "dram.h"
#include "common.h"

#include "pbuff.h"

typedef enum SysType
{
    DIRECTORY = 0,
    BUS = 1
} SysType;

typedef enum ProtocolType
{
    FULL_MAP = 0,
    LIMITED_PTR = 1
} ProtocolType;



typedef struct CacheLevel
{
    int         level;
    int         share;
    int         num_caches;
    int         access_time;
    uint64_t    size;
    uint64_t    num_ways;
    uint64_t    block_size;
    uint64_t    ins_count;
    uint64_t    miss_count;
    uint64_t    ins_count_timeline;
    uint64_t    timeline;
    double      miss_rate;
    double      timeline_avg;
    double      lock_time;
} CacheLevel;


class System
{
    public:
        void init(XmlSys* xml_sys);
        void setDRAMOut(ofstream * dram_out); //NEW
        Cache* init_caches(int level, int cache_id);
        void init_directories(int home_id);
        int access(int core_id, InsMem* ins_mem, int64_t timer);
        char mesi_bus(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer);
        char mesi_directory(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer);
        int syncConflict(Cache * cache_cur, SyncLine * syncline, Line* line_call);
        int fullBarrierFlush(int64_t clk_timer, Cache *cache_cur, int epoch_id, int req_core_id, int psrc);
        int epoch_meters(int core_id, InsMem * ins_mem);
        int bepochPersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int epochPersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int epochPersistWithPF(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int epochPersistWithoutPF(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int epochPersistWithPFGen(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_call, int req_core_id, int psrc);
        int fullFlush(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc);
        int releaseFlush(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc);
        int releaseFlushWithPF(int64_t clk_timer, Cache *cache_cur, SyncLine * syncline, Line *line_call, int rel_epoch_id, int req_core_id, int psrc);
        int persist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id);
        int persist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int releasePersist(int64_t clk_timer, Cache *cache_cur, InsMem *ins_mem, Line *line_cur, int req_core_id, int psrc);
        int share(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer);
        int share_children(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer);
        int inval(Cache* cache_cur, InsMem* ins_mem, int core_id,int64_t clk_timer);
        int inval_children(Cache* cache_cur, InsMem* ins_mem, int core_id, int64_t clk_timer);
        int accessDirectoryCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state, int core_id);
        int accessSharedCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state, int core_id);
        int allocHomeId(int num_homes, uint64_t addr);
        int getHomeId(InsMem *ins_mem);
        int tlb_translate(InsMem *ins_mem, int core_id, int64_t timer);
        int getCoreCount();
        void report(ofstream* result, ofstream* stat);
        ~System();

        int accessPersistBuffer(uint64_t timer, int core_id, InsMem * ins_mem);
        int accessMCQBuffer(uint64_t timer, int core_id, PBuffLine * new_line);
        int flushPersistBuffer(uint64_t timer, int core_id, uint64_t addr); 

    private:
        int        sys_type;
        int        protocol_type;
        int        max_num_sharers;
        int        num_cores;
        int        dram_access_time;
        int        num_levels;
        int        page_size;
        int        tlb_enable;
        int        shared_llc;
        int        verbose_report;
        bool       *hit_flag;
        int*       delay;
        int        total_num_broadcast;
        uint64_t   total_bus_contention;
        int*       home_stat;
        XmlSys*    xml_sys;
        CacheLevel* cache_level;
        Cache***   cache;
        Cache**    directory_cache;
        Cache*     tlb_cache;
        pthread_mutex_t** cache_lock;
        pthread_mutex_t*  directory_cache_lock;
        bool**     cache_init_done;
        bool*      directory_cache_init_done;
        PageTable  page_table;
        Network    network;
        Dram       dram;
        PersistModel pmodel;
        bool proactive_flushing;
        int*       delay_persist;

        bool pbuff_enabled;

        int*       delay_tmp; // For the flush operations.
        int*       delay_llc;
        int*       delay_mc;
        int*       return_delay;
        int*       return_timer;

        //Persist Buffer
        PBuff * persist_buffer; 
        MCQBuff * mem_ctrl_queue;

        //only for the persist buffer dependencies. 
        int*       unique_write_id;
        //Only for the DPO dependency tracking.

        bool*      is_last_access_dep;    
        int*       last_access_core; 
        int*       last_access_addr; //Cache_tag or address.
        int*       last_access_cache_tag;
        bool*      is_last_acc_barrier; // For Acquire
        int*       unique_last_write_id;
        //----------------------------------------

};

#endif // SYSTEM_H

/*
//Need some extra work to track dependencies.
is_last_access_dep[core_id] = false;
last_access_core[core_id] = -1;
last_access_addr[core_id] = -1; // Not necessary. check
last_access_cache_tag[core_id] = -1; //Not necessary. check.
is_last_acc_barrier [core_id] = false;
unique_last_write_id[core_id] = 0;
*/