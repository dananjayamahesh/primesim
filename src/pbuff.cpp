//===========================================================================
// page_table.cpp contains a simple page translation model to allocate 
// physical pages sequentially starting from 0
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


#include "pbuff.h"

using namespace std;

void PBuff::init(int pbuff_size_in, int delay_in)
{
    pbuff_size = pbuff_size_in;
    access_delay = delay_in;      
    lock = new pthread_mutex_t;
    pthread_mutex_init(lock, NULL);
    pbuff = NULL;
    head = NULL;
    tail = NULL;
}
         
PBuffLine * PBuff::access(uint64_t addr, bool barrier, bool release){
    
    //If the access successful then 
    cout << "PBuff Access" << endl;
    return NULL;
}  

int PBuff::insert(uint64_t addr){
    
    PBuffLine pline;
    pline.timestamp = 0;
    printf("%lx \n", pline.timestamp);
    return 1;
}

//Remove the head-only head can be removed.
PBuffLine * PBuff::remove(uint64_t addr){
    cout << "PBuff Remove" << endl;
    return NULL;
}

void PBuff::report(ofstream* result)
{
    *result << "Persist Buffers - Kolli:\n";  
}

PBuff::~PBuff()
{
    pthread_mutex_destroy(lock);
    delete lock;
}


//-------------------------------------------------------------------------------------------
void MCQBuff::init(int mcq_buff_size_in, int delay_in){
   
   cur_size = 0;
   max_size = mcq_buff_size_in;   
   access_delay = delay_in;
   head = NULL;
   tail = NULL;

}

int MCQBuff::insert(PBuffLine * pbuff_line, uint64_t addr, int core_id){
    cout << "MCQBuff Insert" << endl;
    return 1;
} 

PBuffLine * MCQBuff::remove(uint64_t addr){
    cout << "MCQBuff Remove" << endl;
    return  NULL;
} 

MCQBuff::~MCQBuff(){
    pthread_mutex_destroy(lock);
    delete lock;
}


