//===========================================================================
// page_table.h 
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

#ifndef  PBUFF_H
#define  PBUFF_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <set>
#include <map>
#include <vector>

#include "common.h"
#include "cache.h"
#include <pthread.h> 

//Persist-Buffer based Design
#define PBUFF_SIZE 4
#define MCQBUFF_SIZE 1

typedef struct PBuffLine
{
	uint64_t timestamp; //timestamp should be changed. 
    uint64_t cache_tag; // cachline address 
    uint64_t data;  // Not required
    bool is_barrier; //release or acquire

    bool * dp_vector;
    bool has_dp;

    uint64_t last_addr;
    bool is_empty;
    int core_id;
    //uint64_t unique_id; // for each core. may be for a write
    uint64_t unique_wr_id;
    uint64_t last_unique_wr_id;
    uint64_t num_wr_coal;

    //Only for Acquires- barrier. Need to track dependencies.
    bool  is_dependent; //persistent depends on another core.. PERSIST CONFLICTS
    uint64_t dep_core_id; // Released-core id
    uint64_t dep_cache_tag; //Cachline/ address.
    uint64_t dep_rel_addr;

    PBuffLine * next_line;

} PBuffLine;

//This is not necessayry with the class. Class must be initiated for every core.
typedef struct PBuffSet{
    PBuffEntry pbuff [PBUFF_SIZE]; //Lets keep initial size of 4.
    int pbuff_size;

    //ring buffer
    int head;
    int tail;
    bool overplay; // when the end < start 
} PBuffSet;


//This is the buffer- FIFO Buffer
class PBuff
{
    public:
        void init(int pbuff_size_in, int delay_in);           
        PBuffLine * access(uint64_t addr);

        int insert(uint64_t addr);
        int insert(PBuffLine * new_line); //This can push down some of the 
        //PBuffLine * remove(uint64_t addr); //When remove, first access mcq buffer and then dram. Remove the head.
        PBuffLine * remove(); // FIFO Out
        void report(ofstream* result); 
        int getBuffSize();
       	bool isFull();
       	bool isEmpty();
       	int getAccessDelay();
        ~PBuff(); 
        uint64_t num_persists;
        uint64_t num_pbarriers;

        bool * dp_vector; //Dependency Tracking Vector.
        bool has_dp; //At least one dependency.

    private:
    	int cur_size;
    	int max_size; //static size
        int access_delay;
        int offset; //for cache-lines
        pthread_mutex_t   *lock; //access control
        PBuffLine * pbuff; //This can be used as an array
        PBuffLine * head; //Individual Persist Buffer.
        PBuffLine * tail; //Individual Persist Buffer.
};

class MCQBuff{
	public:
		void init(int mcq_buff_size_in, int delay_in);
		int insert(PBuffLine * pbuff_line, uint64_t addr, int core_id);
		
		PBuffLine * remove(uint64_t addr);
		PBuffLine * remove(); //return type may be PBuffLine*
		int getAccessDelay();
		bool isFull();
		bool isEmpty();	
		~MCQBuff(); 

		// /uint64_t num_persists;
        uint64_t num_pbarriers;
        uint64_t num_access;
		uint64_t num_persists;	
	 
	private:
		pthread_mutex_t   *lock; //access control
		PBuffLine * head;
		PBuffLine * tail;
		int cur_size;
		int max_size;	
		int persists;
		int access_delay;		
};

#endif //PBUFF_H
                                                                                                                                                                                                                                                                                                                                                                                                                                      