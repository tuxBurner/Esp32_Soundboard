#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

  // defines and includes
  #define VERSION "Mi, 25 Jan 2019"

  #define NAME "SeppelsSB"

  // vs1053 pins
  // XRST goes on EN or RST pin this must be high when esp is turned on
  #define VS1053_DCS    22
  #define VS1053_CS     5
  #define VS1053_DREQ   21
  #define SPI_SCK_PIN   18
  #define SPI_MISO_PIN  19
  #define SPI_MOSI_PIN  23

  // status led vars
  #define STATUS_LED_PIN 16

  // wifi settings
  #define WIFI_SSID "suckOnMe"
  #define WIFI_PASS "leatomhannes"
  #define WIFI_AP_SSID "soundboard"
  #define WIFI_AP_PASS "pass"

  #define BUFFER_SIZE 60 // was 6000
  #define QSIZ 400  // size of the data que

#endif;