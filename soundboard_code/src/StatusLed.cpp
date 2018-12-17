#include "Arduino.h"

#include "StatusLed.h"

StatusLed::StatusLed(uint8_t ledPin) {
  _dbg.print("Led", "Setting status led on pin: %d", ledPin);
  _ledPin = ledPin;
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(_ledPin, LEDC_CHANNEL_0);
}


void StatusLed::setNewCfg(ledConfig newConfig) {
  _currentLedValue = 0;
  _maxBrightness  = newConfig.maxBrightness;
  _lastStatusLedCheck = 0;
  _LED_CURRENT_CFG = newConfig;
}

void StatusLed::callInloop() {
  
  // dont change the value because we have to wait.
  if ((millis() - _lastStatusLedCheck) < _LED_CURRENT_CFG.msForNextIncrement) {
    return;
  }

  //_dbg.print("LED", "Update the LED %d", _currentLedValue);

  // debounce time over
  _lastStatusLedCheck = millis();

  uint32_t duty = (8191 / 255) * _min(_currentLedValue, 255);
  // write duty to LEDC
  ledcWrite(LEDC_CHANNEL_0, duty);

  _currentLedValue += _LED_CURRENT_CFG.value;

  // reverse the direction
  if (_currentLedValue <= 0 || _currentLedValue >= _LED_CURRENT_CFG.maxBrightness) {
    _LED_CURRENT_CFG.value = -_LED_CURRENT_CFG.value;
  }
}
