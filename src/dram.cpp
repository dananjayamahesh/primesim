//===========================================================================
// dram.cpp implements a simple dram model with constant delay 
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


#include "dram.h"

using namespace std;


void Dram::init(int access_delay_in)
{
    access_delay = access_delay_in;
    num_accesses = 0;
}

void Dram::init(int access_delay_in, ofstream * dram_out)
{
    
    //dram output stream
    dram_o = dram_out;
    access_delay = access_delay_in;
    num_accesses = 0;
}

void Dram::setOut(ofstream * dram_out)
{
   dram_o = dram_out; //Set
   printf("DRAM Trace File Has Been Created");
}

int Dram::access(InsMem * ins_mem)
{
    num_accesses++;
    if(ins_mem->mem_type == RD) return int(access_delay*0.75);
    else return access_delay;
}

//New one
int Dram::access(int64_t timestamp, InsMem * ins_mem)
{
    
    #ifdef DRAM_OUT_ENABLE 
        num_accesses++;
        //Outpout Dump
        if(dram_o != NULL){ //check fot the output file
          *dram_o << timestamp << "," << ( (ins_mem->mem_type==RD)?0:1 ) << "," << ins_mem->addr_dmem << endl;
        }
        else{
          cerr << "Error: Not enough cores!\n";
          return -1;
        }
        if(ins_mem->mem_type == RD) return int(access_delay*0.75);
        else return access_delay;
    
    #else
        num_accesses++;
        if(ins_mem->mem_type == RD) return int(access_delay*0.75);
        else return access_delay;

    #endif
}


void Dram::report(ofstream* result)
{

    *result << "DRAM Statistics:\n";
    *result << "Total # of DRAM accesses: " << num_accesses <<endl;
}

Dram::~Dram()
{
}       

