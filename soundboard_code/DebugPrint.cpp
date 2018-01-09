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
char* DebugPrint::print(const String domain, const char* format, ...) {

  va_list varArgs;                                    // For variable number of params

//  const char test = domain + format;

  va_start(varArgs, format);                    // Prepare parameters
  vsnprintf(sbuf, sizeof(sbuf), format, varArgs); // Format the message
  va_end(varArgs);                                 // End of using parameters
  if (_DEBUG) {                                     // DEBUG on?
    Serial.print(domain);                           // Yes, print prefix
    Serial.print(": ");
    Serial.println(sbuf);                          // and the info
  }
  return sbuf;
}
