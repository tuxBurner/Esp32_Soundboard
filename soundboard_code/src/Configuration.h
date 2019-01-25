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

  // Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
  // Use a multiple of 1024 for optimal handling of bufferspace.  See definition of tmpbuff.
  //#define RINGBFSIZ 40960
  #define RINGBFSIZ 256


#endif;