/**
   Handles the status led
*/
#ifndef STATUSLED_h
#define STATUSLED_h

#include "Arduino.h"
#include "DebugPrint.h"

#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000

class StatusLed {

  public:
    /**
       Constructor
    */
    StatusLed(uint8_t ledPin);

    /**
       struct for setting the configuration
    */
    struct ledConfig {
      uint8_t value;                                 // How much to change
      unsigned int msForNextIncrement;               // how long to wait for the next increment
      uint8_t maxBrightness;                         // maxBrightness
    };

    // Call this in the loop of your application    
    void callInloop();

    // sets the new config
    void setNewCfg(ledConfig newConfig);

    ledConfig NO_LIGHT = {0,0,0};

  private:
    ledConfig _LED_CURRENT_CFG;
    DebugPrint _dbg;
    uint8_t _ledPin;
    uint8_t _currentLedValue = 0;
    uint8_t _maxBrightness = 0;
    unsigned long _lastStatusLedCheck = 0;
};

#endif
