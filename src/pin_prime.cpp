//===========================================================================
// pin_prime.cpp utilizes PIN to feed instructions into the core model 
// at runtime
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

#include "pin_prime.h"

using namespace std;

int ins_count=0;
// This function is called before every instruction is executed to calculate the instruction count
void PIN_FAST_ANALYSIS_CALL insCount(uint32_t ins_count, THREADID threadid)
{
    core_manager->insCount(ins_count, threadid);
}

// Handle non-memory instructions
void PIN_FAST_ANALYSIS_CALL execNonMem(uint32_t ins_count, THREADID threadid)
{
    core_manager->execNonMem(ins_count, threadid);
}




#ifdef ACQREL

void execMem(void * addr, THREADID threadid, uint32_t size, bool mem_type, void *ip, void *ins, uint32_t next_op, bool valid, bool isrelease, bool isacquire, bool isrmw)
{
   // core_manager->execMem(addr, threadid, size, mem_type);
   //if(release) printf("RELEASE\n");
    if(isrelease || isacquire) printf("[AR]--- Instruction Trap --- %d ///// THREAD Id - %d /// Addr %p Size %d IP %p mem-type %s isRMW: %s isRelease: %s isAcquire: %s  \n", isrelease, (uint32_t)threadid, addr, size, ip, (!mem_type)?"READ":"WRITE", (isrmw)?"true":"false", (isrelease)?"true":"false", (isacquire)?"true":"false");
    //if(isrmw) printf("[AR]--- Instruction Trap RMW %p \n", addr);
}

#else
// Handle a memory instruction
void execMem(void * addr, THREADID threadid, uint32_t size, bool mem_type, bool is_release, bool is_acquire, bool is_rmw)
{
    
    //if(is_release || is_acquire) printf("--- Instruction Trap --- %d ///// THREAD Id - %d /// Addr %p Size %d mem-type %s isRMW: %s isRelease: %s isAcquire: %s  \n", is_release, (uint32_t)threadid, addr, size, (!mem_type)?"READ":"WRITE", (is_rmw)?"true":"false", (is_release)?"true":"false", (is_acquire)?"true":"false");

    //if(is_release || is_acquire)printf("[MEM-EXEC]] %d %d %d ///// THREAD Id - %d /// Addr %p Size %d mem-type %s\n", is_release, is_acquire, is_rmw, (uint32_t)threadid, addr, size, (!mem_type)?"READ":"WRITE" );
/*
    if((uint32_t)threadid >=0){
        if(is_release){
            //printf("////////////////////////////////// RELEASE %d ///// THREAD Id - %d /// Addr %p Size %d mem-type %s\n", is_release, (uint32_t)threadid, addr, size, (!mem_type)?"READ":"WRITE" );
        }    
        if(is_acquire){
            //printf("////////////////////////////////// ACQUIRE  %d ///// THREAD ID - %d /// Addr %p Size %d mem-type %s \n", is_acquire, (uint32_t)threadid, addr, size, (!mem_type)?"READ":"WRITE");
        }
    }

*/
    core_manager->execMem(addr, threadid, size, mem_type, is_acquire, is_release, is_rmw);
}

#endif
// This routine is executed every time a thread starts.
void ThreadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *v)
{
    core_manager->threadStart(threadid, ctxt, flags, v);
}

// This routine is executed every time a thread is destroyed.
void ThreadFini(THREADID threadid, const CONTEXT *ctxt, int32_t code, void *v)
{
    core_manager->threadFini(threadid, ctxt, code, v);
}


// Inserted before syscalls
void SysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, 
               ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, THREADID threadid)
{
    core_manager->sysBefore(ip, num, arg0, arg1, arg2, arg3, arg4, arg5, threadid);
}

// Inserted after syscalls
void SysAfter(ADDRINT ret, THREADID threadid)
{
    core_manager->sysAfter(ret, threadid);
}


// Enter a syscall
void SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    core_manager->syscallEntry(threadIndex, ctxt, std, v);
}

// Exit a syscall
void SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    core_manager->syscallExit(threadIndex, ctxt, std, v);
}


#ifdef ACQREL

void Trace(TRACE trace, void *v)
{
     
    std::cout << "//////////////////////////////////// New Trace [AR] /////////////////////////////////////" << std::endl ;
     uint32_t nonmem_count;
     
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        std::cout << "-------------------------new BBL -  Instructions " << BBL_NumIns(bbl) << "-------------------------" << std::endl ;
        // Insert a call to insCount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)insCount, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, BBL_NumIns(bbl), IARG_THREAD_ID, IARG_END);

        nonmem_count = 0;
        INS ins = BBL_InsHead(bbl);
        //INS ins_next =BBL_InsHead(bbl);
        //BBL lib - https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__BBL__BASIC__API.html

        //for (; INS_Valid(ins); ins = INS_Next(ins)) {
        for (; INS_Valid(ins); ins = INS_Next(ins)) {           
            
            std::cout << std::hex << INS_NextAddress(ins) << "\t"<< INS_Opcode(ins) << " \t " << INS_Disassemble(ins) << std::endl;
            //OPCODE opcode = INS_Opcode(ins); //Unused
            //printf("OPCODE - %x \n", (int) opcode);
            
            //std::cout <<  INS_Mnemonic(ins) << std::endl;        

            if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {

                uint32_t memOperands = INS_MemoryOperandCount(ins);
                //uint32_t next_addr = INS_NextAddress (ins); //Unused

                bool isRelease = false;
                bool isAcquire = false;

                bool isRMW = false;
                
                INS ins_next = INS_Next(ins);
                //OPCODE opcode_next = INS_Opcode(ins_next);
                bool isvalid = INS_Valid(ins_next);
                uint32_t opn = 0;
                
                if(isvalid) {
                     opn = (uint32_t)INS_Opcode(ins_next);
                        if(opn == 0x17a){ //Checkfor Mfence
                            std::cout << "MFENCE "<< opn << " \n" << std::endl;  //MFENCE
                            isRelease = true;
                        }                   
                }   
                
                if(INS_IsAtomicUpdate (ins)){
                    
                    //Check cmpxchg - 64
                     uint32_t rmwop= (uint32_t)INS_Opcode(ins);
                    if(rmwop==0x064){
                        isRMW = true;                    
                        //isAcquire = true;
                        if(foundSFence && foundLFence){
                            isAcquire = true; isRelease = true;
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                            std::cout << "FULL Barrier "<< opn << " \n" << std::endl;  //MFENCE
                        }
                        else if(foundSFence){
                            isRelease = true;
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                             std::cout << "Release Barrier "<< opn << " \n" << std::endl;  //MFENCE
                        }
                        else if(foundLFence){
                            isAcquire = true;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                             std::cout << "Acquire Barrier "<< opn << " \n" << std::endl;  //MFENCE
                        }else{
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                        }
                            
                        std::cout <<  INS_Mnemonic(ins) << " Operands "<< memOperands << " MemOpType "<< (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) <<std::endl;
                        //Lock prefic of x86 - LOCK the bus
                        //https://stackoverflow.com/questions/8891067/what-does-the-lock-instruction-mean-in-x86-assembly
    
                        if(INS_IsMemoryRead(ins)){
                            std::cout <<  "READ \n "<< std::endl;
                        }else{
                            std::cout <<  "WRITE \n "<< std::endl;
                        }
                    }
        
                }
        
               // Iterate over each memory operand of the instruction.
                for (uint32_t memOp = 0; memOp < memOperands; memOp++){

                    const uint32_t size = INS_MemoryOperandSize(ins, memOp);

                    // Note that in some architectures a single memory operand can be 
                    // both read and written (for instance incl (%eax) on IA-32)
                    // In that case we instrument it once for read and once for write.

                    if (INS_MemoryOperandIsRead(ins, memOp))
                    {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, RD,
                            IARG_INST_PTR,
                            IARG_PTR, ins,
                            IARG_UINT32, opn, 
                            IARG_BOOL, isvalid,
                            IARG_BOOL, isRelease,
                            IARG_BOOL, isAcquire,
                            IARG_BOOL, isRMW,
                            IARG_END);

                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, WR,
                            IARG_INST_PTR,
                            IARG_PTR, ins,
                            IARG_UINT32, opn, 
                            IARG_BOOL, isvalid,
                            IARG_BOOL, isRelease,
                            IARG_BOOL, isAcquire,
                            IARG_BOOL, isRMW,
                            IARG_END);
                    }
                }
            
            }
            
            else {
                    uint32_t op=0;
                    op = (uint32_t)INS_Opcode(ins);
                    if(op == 0x17a){ //MFENCE
                            std::cout << "MFENCE "<< op << " \n" << std::endl;  //MFENCE
                            foundMFence = true;
                            mfence_count = 1;
                            mfence_delay = 0;
                    } 
                    else if(op==0x2a1){
                            std::cout << "SFENCE "<< op << " \n" << std::endl;  //MFENCE
                            foundSFence = true;
                            sfence_count = 1;
                            sfence_delay = 0;
                    }   
                    else if(op==0x15f){
                            std::cout << "LFENCE "<< op << " \n" << std::endl;  //MFENCE
                            foundLFence = true;
                            lfence_count = 1;
                            lfence_delay = 0;
                    }else{
                            if(foundMFence) mfence_delay++;
                            if(foundSFence) sfence_delay++;
                            if(foundLFence) lfence_delay++;
                    }
                    nonmem_count++;
            }

                    //ins=ins_next;

        } //END INS LOOP
            
        
        // Insert a call to execNonMem before every bbl, passing the number of nonmem instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)execNonMem, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, nonmem_count, IARG_THREAD_ID, IARG_END);

    } // End Ins For

}

#else


// Pin calls this function every time a new basic block is encountered
void Trace(TRACE trace, void *v)
{
        //std::cout << "//////////////////////////////////// New Trace /////////////////////////////////////" << std::endl ;

     uint32_t nonmem_count;

    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        //std::cout << "-------------------------new BBL -  Instructions " << BBL_NumIns(bbl) << "-------------------------" << std::endl ;

        // Insert a call to insCount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)insCount, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, BBL_NumIns(bbl), IARG_THREAD_ID, IARG_END);

        nonmem_count = 0;
        INS ins = BBL_InsHead(bbl);
        for (; INS_Valid(ins); ins = INS_Next(ins)) {
            //std::cout << std::hex << INS_NextAddress(ins) << "\t"<< INS_Opcode(ins) << " \t " << INS_Disassemble(ins) << std::endl;

            ins_count++;
            // Instruments memory accesses using a predicated call, i.e.
            // the instrumentation is called iff the instruction will actually be executed.
            //
            // The IA-64 architecture has explicitly predicated instructions. 
            // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
            // prefixed instructions appear as predicated instructions in Pin.
            if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) { 
                uint32_t memOperands = INS_MemoryOperandCount(ins);

                bool isRelease = false;
                bool isAcquire = false;
                bool isRMW = false;
                INS ins_next = INS_Next(ins);

                if(INS_Valid(ins_next)){
                    uint32_t next_op = (uint32_t)INS_Opcode(ins_next);
                    if(next_op == 0x17a){
                        isRelease = true;
                        #ifdef PDEBUG
                        printf("Ins Id %d MFENCE is DETECTED \n", ins_count);
                        #endif
                    }else{
                        isRelease = false;
                    }
                }

                if(INS_IsAtomicUpdate (ins)){
                    //isAcquire = true;
                    //#ifdef SYNCBENCH
                    //Check cmpxchg - 64
                    uint32_t rmwop= (uint32_t)INS_Opcode(ins);
                    #ifdef SYNCBENCH                    
                    if(rmwop==0x064 || rmwop==0x3a){
                        isRMW = true;                    
                        //isAcquire = true;
                        if(foundSFence && foundLFence){
                            isAcquire = true; isRelease = true;
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                            #ifdef PDEBUG
                                std::cout<<" Ins Id: "<<ins_count;
                                std::cout << "FULL Barrier "<< rmwop << " \n" << std::endl;  //MFENCE
                            #endif
                        }
                        else if(foundSFence){
                            isRelease = true;
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                            #ifdef PDEBUG
                                std::cout<<" Ins Id: "<<ins_count;
                                std::cout << "Release Barrier "<< rmwop << " \n" << std::endl;  //MFENCE
                             #endif
                        }
                        else if(foundLFence){
                            isAcquire = true;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                            #ifdef PDEBUG
                                std::cout<<" Ins Id: "<<ins_count;
                                std::cout << "Acquire Barrier "<< rmwop << " \n" << std::endl;  //MFENCE
                             #endif
                        }else{
                            foundSFence = false; sfence_delay=0; sfence_count=0;
                            foundLFence = false; lfence_delay=0; lfence_count=0;
                        }
                        //isAcquire = isRMW;
                    }

                    #else
                        if(rmwop==0x064){ //think about bts as well
                            isAcquire = true;
                            isRMW = true;
                        }
                    #endif
                    //#else
                    //    isAcquire = true;
                    //#endif
                }
                // Iterate over each memory operand of the instruction.
                for (uint32_t memOp = 0; memOp < memOperands; memOp++) {
                    const uint32_t size = INS_MemoryOperandSize(ins, memOp);
                    // Note that in some architectures a single memory operand can be 
                    // both read and written (for instance incl (%eax) on IA-32)
                    // In that case we instrument it once for read and once for write.
                    if (INS_MemoryOperandIsRead(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, RD,
                            IARG_BOOL, isRelease,
                            IARG_BOOL, isAcquire,
                            IARG_BOOL, isRMW,
                            IARG_END);

                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, WR,
                            IARG_BOOL, isRelease,
                            IARG_BOOL, isAcquire,
                            IARG_BOOL, isRMW,
                            IARG_END);
                    }
                }
            }
            else {

                #ifdef SYNCBENCH

                uint32_t op=0;
                    op = (uint32_t)INS_Opcode(ins);
                    if(op == 0x17a){ //MFENCE
                            #ifdef PDEBUG
                                std::cout << "MFENCE "<< op << " \n" << std::endl;  //MFENCE
                            #endif
                            foundMFence = true;
                            mfence_count = 1;
                            mfence_delay = 0;
                    } 
                    else if(op==0x2a1){
                            #ifdef PDEBUG
                                std::cout << "SFENCE "<< op << " \n" << std::endl;  //MFENCE
                            #endif
                            foundSFence = true;
                            sfence_count = 1;
                            sfence_delay = 0;
                    }   
                    else if(op==0x15f){
                            #ifdef PDEBUG
                                std::cout << "LFENCE "<< op << " \n" << std::endl;  //MFENCE
                            #endif
                            foundLFence = true;
                            lfence_count = 1;
                            lfence_delay = 0;
                    }else{
                            if(foundMFence) mfence_delay++;
                            if(foundSFence) sfence_delay++;
                            if(foundLFence) lfence_delay++;
                    }

                #endif    

                nonmem_count++;
            }
        }
        // Insert a call to execNonMem before every bbl, passing the number of nonmem instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)execNonMem, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, nonmem_count, IARG_THREAD_ID, IARG_END);

	} // End Ins For

}

#endif

void Start(void *v)
{
}

void Fini(int32_t code, void *v)
{
    core_manager->getSimFinishTime();
    stringstream ss_rank;
    ss_rank << myrank;
    result.open((KnobOutputFile.Value() + "_" + ss_rank.str()).c_str());
    //Report results
    stat.open((KnobOutputFile.Value() + "_" + "stat").c_str());
    core_manager->report(&result, &stat);
    result.close();
    stat.close();
    core_manager->finishSim(code, v);
    delete core_manager;
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
int32_t Usage()
{
    PIN_ERROR( "This Pintool simulates a many-core cache system\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    //Link to the OpenMPI library
    dlopen(OPENMPI_PATH, RTLD_NOW | RTLD_GLOBAL);
    int prov, rc;
    rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &prov);
    if (rc != MPI_SUCCESS) {
        cerr << "Error starting MPI program. Terminating.\n";
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
    if(prov != MPI_THREAD_MULTIPLE) {
        cerr << "Provide level of thread supoort is not required: " << prov << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_Comm_size(MPI_COMM_WORLD,&num_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);

    int rank_excl[1] = {0};
    // Extract the original group handle *
    MPI_Comm_group(MPI_COMM_WORLD, &orig_group);

    MPI_Group_excl(orig_group, 1, rank_excl, &new_group);

    // Create new new communicator and then perform collective communications 
    MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm);
    MPI_Comm_rank(new_comm, &new_rank);
    
    if (PIN_Init(argc, argv)) return Usage();

    XmlParser xml_parser;
    XmlSim*   xml_sim;

    if(!xml_parser.parse(KnobConfigFile.Value().c_str())) {
		cerr<< "XML file parse error!\n";
        MPI_Abort(MPI_COMM_WORLD, -1);
		return -1;
    }
    xml_sim = xml_parser.getXmlSim();

    PIN_InitSymbols();

    core_manager = new CoreManager;
    //# of core processes equals num_tasks-1 because there is a uncore process
    core_manager->init(xml_sim, num_tasks-1, myrank);
    core_manager->getSimStartTime();

     foundMFence = false;
     mfence_count = 0; //Number of mfence found
     mfence_delay = 0; //Instructions since last mfence.
     foundSFence = false;
     sfence_count = 0; 
     sfence_delay = 0;
     foundLFence = false;
     lfence_count = 0;
     lfence_delay = 0;
   // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);	
    PIN_AddThreadFiniFunction(ThreadFini, 0);	
    PIN_AddApplicationStartFunction(Start, 0);
    PIN_AddFiniFunction(Fini, 0);

    core_manager->startSim(&new_comm);

    PIN_StartProgram();
    
    return 0;
}
