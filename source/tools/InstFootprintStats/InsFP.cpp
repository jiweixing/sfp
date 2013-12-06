
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include "instlib.H"

using namespace INSTLIB;
/* ================================================================== */
// Types
/* ================================================================== */
struct BlockStat
{
  UINT64 counter;
  string img_name;
  string rtn_name;
  //std::map<ADDRINT, UINT64> blocks;
  BlockStat() : counter(0),
               img_name(""),
               rtn_name("")
  {}
};

/* ================================================================== */
// Global variables 
/* ================================================================== */

//#define PAGESIZE       4096
#define CACHELINESIZE  64
//#define PAGEMASK       0xfff
#define CACHELINEMASK  0x3f

//#define PageBase(x)       ((x)&~PAGEMASK)
#define CachelineBase(x)  ((x)&~CACHELINEMASK)

//UINT64 insCount = 0;        //number of dynamically executed instructions
//UINT64 bblCount = 0;        //number of dynamically executed basic blocks
UINT64 threadCount = 0;     //total number of threads, including main thread
//std::map<ADDRINT, PageStat> gPageProfile;
std::map<ADDRINT, BlockStat> gProfile;

std::ostream * out = &cerr;


FOLLOW_CHILD follow;
/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "fp.out", "specify file name for InstFP output");


/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the statistics of instruction footprint at page level " << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every instruction when it is about to be executed.
 * @param[in]   InsPC    instruction PC
 * @param[in]   InsSize  instruction Size
 * @note use atomic operations for multi-threaded applications
 */
VOID RecordIns(const ADDRINT InsPC, const UINT32 InsSize)
{
    ADDRINT block = CachelineBase(InsPC);

    BlockStat bs = gProfile[block];

    // increase block's counter 
    bs.counter++;
 
    // store block's image info
    if ( bs.img_name == "" )
    {
      IMG img = IMG_FindByAddress(InsPC);
      if ( IMG_Valid(img) )
      {
        bs.img_name = IMG_Name(img).c_str();
      }
    }

    // store block's rtn info
    if ( bs.rtn_name == "" )
    {
      RTN rtn = RTN_FindByAddress(InsPC);
      if ( RTN_Valid(rtn) )
      {
        bs.rtn_name = RTN_Name(rtn);
      }
    } 

    gProfile[block] = bs;

}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID* v) {

   // head of trace
   ADDRINT pc = TRACE_Address(trace);

   for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
     
      // head of basic block
      const INS head = BBL_InsHead(bbl);

      if (!INS_Valid(head)) continue;
      
      for (INS ins = head; INS_Valid(ins); ins = INS_Next(ins)) {

          UINT32 instruction_size = INS_Size(ins);
          RecordIns(pc, instruction_size);

          pc += instruction_size;
      }
   }
}

/*!
 * Increase counter of threads in the application.
 * This function is called for every thread created by the application when it is
 * about to start running (including the root thread).
 * @param[in]   threadIndex     ID assigned by PIN to the new thread
 * @param[in]   ctxt            initial register state for the new thread
 * @param[in]   flags           thread creation flags (OS specific)
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddThreadStartFunction function call
 */
VOID ThreadStart(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    threadCount++;
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    stringstream ss;
    ofstream ResultFile;

    string fileName = KnobOutputFile.Value();
    ss << fileName << "." << PIN_GetPid();
    ResultFile.open(ss.str().c_str());

    ResultFile <<  "===============================================" << endl;
    ResultFile <<  "InstFootprint analysis results: " << endl;
    
    map<ADDRINT, BlockStat>::iterator profile_iter;
    for(profile_iter  = gProfile.begin(); 
        profile_iter != gProfile.end(); 
        profile_iter++) {

      ADDRINT base = profile_iter->first;
      BlockStat bs = profile_iter->second;
      
      ResultFile << hex << "0x" << base << "\t" << bs.img_name << "\t" << bs.rtn_name << "\t" << dec << bs.counter << endl;
    }
      
    ResultFile <<  "===============================================" << endl;

    ResultFile.close();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    // Register function to be called to instrument instructionss
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called for every thread before it starts running
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    follow.Activate();

    // Use the same prefix as our command line
    follow.SetPrefix(argv);

    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by InstFootprintStat" << endl;
    if (!KnobOutputFile.Value().empty()) 
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr <<  "===============================================" << endl;

    PIN_InitSymbols();
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
