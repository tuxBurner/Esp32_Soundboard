#ifndef DEBUGPRINT_h
#define DEBUGPRINT_h

#include "Arduino.h"

#define DEBUG_BUFFER_SIZE 130;

class DebugPrint {

  public:
    DebugPrint();
    char* print(const String domain, const char* format, ...);

  private:
    char sbuf[130];
    int _DEBUG = 1; // Debug on/off

};

#endif
