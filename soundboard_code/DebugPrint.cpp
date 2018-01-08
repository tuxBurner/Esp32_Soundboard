#include "Arduino.h"

#include "DebugPrint.h"

DebugPrint::DebugPrint() {
  
}

//**************************************************************************************************
//                                          D B G P R I N T                                        *
//**************************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the DEBUG flag.        *
// Print only if DEBUG flag is true.  Always returns the the formatted string.                     *
//**************************************************************************************************
char* DebugPrint::print(const char* format, ...) {
  va_list varArgs;                                    // For variable number of params

  va_start(varArgs, format);                      // Prepare parameters
  vsnprintf(sbuf, sizeof(sbuf), format, varArgs); // Format the message  
  if (_DEBUG) {                                     // DEBUG on?
    Serial.print("D: ");                           // Yes, print prefix
    Serial.println(sbuf);                          // and the info
  }
  va_end(varArgs);                                 // End of using parameters

  return sbuf;
}
