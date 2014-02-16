
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <iostream>

#include "pin.H"

/* ================================================================== */
// Global variables 
/* ================================================================== */

/* ===================================================================== */
// Command line switches
/* ===================================================================== */

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID ObjAllocate(void* obj) {
}

VOID ObjDeallocate(void* obj) {
}

VOID ObjCallEntry() {
}

VOID ObjCallExit() {
}

VOID ObjRefChange(void* dec, void* inc) {
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID ImageLoad( IMG img, VOID* v) {

  if ( IMG_IsMainExecutable(img) )
  {
    RTN obj_alloc_rtn   = RTN_FindByName( img, "LIVE_ObjAllocateHook" );
    RTN obj_dealloc_rtn = RTN_FindByName( img, "LIVE_ObjDeallocateHook" );
    RTN call_entry_rtn  = RTN_FindByName( img, "LIVE_ObjCallEntryHook" );
    RTN call_exit_rtn   = RTN_FindByName( img, "LIVE_ObjCallExitHook" );
    RTN ref_change_rtn  = RTN_FindByName( img, "LIVE_ObjRefChangeHook" );

    if (RTN_Valid(obj_dealloc_rtn) && RTN_Valid(obj_alloc_rtn)) {
      std::cout << "valid hook for obj (de)allocate\n";
      RTN_Replace(obj_alloc_rtn,   AFUNPTR(ObjAllocate));
      RTN_Replace(obj_dealloc_rtn, AFUNPTR(ObjDeallocate));
    }
    else {
      std::cout << "ObjAllocate and ObjDeallocate not suitable for hook\n";
    }

    if (RTN_Valid(call_entry_rtn) && RTN_Valid(call_exit_rtn)) {
      std::cout << "valid hook for obj call entry/exit\n";
      RTN_Replace(call_entry_rtn, AFUNPTR(ObjCallEntry));
      RTN_Replace(call_exit_rtn,  AFUNPTR(ObjCallExit));
    }
    else {
      std::cout << "ObjCallEntry and ObjCallExit not suitable for hook\n";
    }
      
    if (RTN_Valid(ref_change_rtn)) {
      std::cout << "valid hook for obj ref change\n";
      RTN_Replace(ref_change_rtn, AFUNPTR(ObjRefChange));
    }
    else {
      std::cout << "ObjRefChange not suitable for hook\n";
    }
  }
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
        return -1;
    }

    // Initialize symbol information
    PIN_InitSymbols();
    
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
