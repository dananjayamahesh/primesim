/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include "pin.H"
#include <iostream>

enum MemType
{
    RD    = 0,  //read
    WR    = 1,  //write
    WB    = 2   //writeback
};


FILE * trace;
int ins_count=0;

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: R %p\n", ip, addr);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: W %p\n", ip, addr);
}

// This function is called before every instruction is executed to calculate the instruction count
void PIN_FAST_ANALYSIS_CALL insCount(uint32_t ins_count, THREADID threadid)
{
    //core_manager->insCount(ins_count, threadid);
    //ins_count++;
}

// Handle non-memory instructions
void PIN_FAST_ANALYSIS_CALL execNonMem(uint32_t ins_count, THREADID threadid)
{
    //core_manager->execNonMem(ins_count, threadid);
     //if(threadid > 0) printf("NonMem Thread Id : %d \n", threadid);
}

// Handle a memory instruction
void execMem(void * addr, THREADID threadid, uint32_t size, bool mem_type)
{
   // core_manager->execMem(addr, threadid, size, mem_type);
    //if(threadid > 0) printf("Mem Thread Id : %d %d %s %d \n", threadid, *((int *)addr), (mem_type)?"RD":"WR", size);
}

void memIns(void * addr, THREADID threadid, uint32_t size, bool mem_type, void *ip, void *ins, uint32_t next_op, bool valid)
{
   // core_manager->execMem(addr, threadid, size, mem_type);
   if(valid) {
        if(next_op == 0x17a){ //Checkfor Mfence
            std::cout << next_op << " \n" << std::endl;  //MFENCE
        }
    
   }
    //std::cout << INS_Mnemonic(*in) << std::endl;    

    //if(threadid > 0) printf("Mem Thread Id : %d %d %s %d %s \n", threadid, *((int *)addr), (mem_type)?"RD":"WR", size, INS_Mnemonic(*in).c_str());
}

// Pin calls this function every time a new basic block is encountered
void Trace(TRACE trace, void *v)
{
     uint32_t nonmem_count;

    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        // Insert a call to insCount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)insCount, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, BBL_NumIns(bbl), IARG_THREAD_ID, IARG_END);

        nonmem_count = 0;
        INS ins = BBL_InsHead(bbl);
        //INS ins_next =BBL_InsHead(bbl);
        //BBL lib - https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__BBL__BASIC__API.html

        //for (; INS_Valid(ins); ins = INS_Next(ins)) {
        for (; INS_Valid(ins); ins = INS_Next(ins)) {           
            

            //OPCODE opcode = INS_Opcode(ins); //Unused
            //printf("OPCODE - %x \n", (int) opcode);
            if(INS_IsAtomicUpdate (ins)){
                std::cout <<  INS_Mnemonic(ins) << std::endl;
            }
            //std::cout <<  INS_Mnemonic(ins) << std::endl;        

            if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {

                uint32_t memOperands = INS_MemoryOperandCount(ins);
                //uint32_t next_addr = INS_NextAddress (ins); //Unused
                INS ins_next = INS_Next(ins);
                //OPCODE opcode_next = INS_Opcode(ins_next);
                bool isvalid = INS_Valid(ins_next);
                uint32_t opn = 0;
                if(isvalid) {
                     opn = (uint32_t)INS_Opcode(ins_next);
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
                            ins, IPOINT_BEFORE, (AFUNPTR)memIns,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, RD,
                            IARG_INST_PTR,
                            IARG_PTR, ins,
                            IARG_UINT32, opn, 
                            IARG_BOOL, isvalid,
                            IARG_END);

                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)memIns,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, WR,
                            IARG_INST_PTR,
                            IARG_PTR, ins,
                            IARG_UINT32, opn, 
                            IARG_BOOL, isvalid,
                            IARG_END);
                    }
                }
            
            }
            
            else {
                    nonmem_count++;
            }

                    //ins=ins_next;

        } //END INS LOOP
            
        
        // Insert a call to execNonMem before every bbl, passing the number of nonmem instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)execNonMem, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, nonmem_count, IARG_THREAD_ID, IARG_END);

    } // End Ins For

}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("acqrel.out", "w");

    //INS_AddInstrumentFunction(Instruction, 0);
    TRACE_AddInstrumentFunction(Trace, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

        //Intel BIB
        ////BBL lib - https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__BBL__BASIC__API.html

        //All INS parameters
        //Ins Object : https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__INS__BASIC__API.html
        // Memory Barriers http://www.modernescpp.com/index.php/fences-as-memory-barriers

        //BBL_NumIns (how arguments work)
        //https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__BBL__BASIC__API.html#ga76ceb2d9e0fb974d3c3769b2413ed634

        //BBL Cache line flush
        //https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__INS__BASIC__API.html
        

        //Ins_..()
        //INS API : https://software.intel.com/sites/landingpage/pintool/docs/81205/Pin/html/group__INS__BASIC__API__GEN__IA32.html

        //ALl arguments - parameters
        //https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/group__INST__ARGS.html#ga089c27ca15e9ff139dd3a3f8a6f8451d

        //Who ordered an MFENCE in x86
        //https://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/

              //printf("MemOp\n");
                //if(INS_Mnemonic(INS_Next(ins)) == "MFENCE") printf("MFENCE \n"); //https://www.felixcloutier.com/x86/mfence

                //if(INS_IsAtomicUpdate(ins)) std::cout << INS_Mnemonic(ins) << std::endl;
                //ALl INS Function
                //https://software.intel.com/sites/landingpage/pintool/docs/81205/Pin/html/group__INS__BASIC__API__GEN__IA32.html



/////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
/*
// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    //printf("Nothing");

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
    }
}
*/