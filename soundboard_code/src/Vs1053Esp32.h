#ifndef VS1053ESP32_h
#define VS1053ESP32_h

#include "Arduino.h"
#include <SPI.h>
#include "DebugPrint.h"

class Vs1053Esp32 {

  public:
    Vs1053Esp32(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin);
    bool testComm(const char *header);           // Test communication with module
    void softReset();                               // Do a soft reset
    void begin();    
    void startSong();                               // Prepare to start playing. Call this each
    void playChunk(uint8_t* data, size_t len);   // Play a chunk of data.  Copies the data to
    void stopSong();                                // Finish playing a song. Call this after
    void setVolume(uint8_t vol);                 // Set the player volume.Level from 0-100,
    void setTone(uint8_t* rtone);                // Set the player baas/treble, 4 nibbles for
    uint8_t getVolume();                               // Get the current volume setting.
    void printDetails(const char *header);       // Print configuration details to serial output.
    
    inline bool data_request() const {
      return (digitalRead(_dreq_pin) == HIGH);
    }

  private:
    uint8_t _dreq_pin;                      // Pin where DREQ line is connected
    uint8_t _cs_pin;                        // Pin where CS line is connected
    uint8_t _dcs_pin;                       // Pin where DCS line is connected

    uint8_t _curvol;                        // Current volume setting 0..100%
    uint8_t _endFillByte;                   // Byte to send when stopping song
    const uint8_t _vs1053_chunk_size = 32;

    DebugPrint _dbg;

    SPISettings  _VS1053_SPI;               // SPI settings for this slave
    // SCI Register
    const uint8_t _SCI_MODE = 0x0;
    const uint8_t _SCI_AUDATA = 0x5;
    const uint8_t _SCI_BASS = 0x2;
    const uint8_t _SCI_CLOCKF = 0x3;
    const uint8_t _SCI_VOL = 0xB;
    const uint8_t _SCI_WRAM = 0x6;
    const uint8_t _SCI_WRAMADDR = 0x7;
    const uint8_t _SCI_num_registers = 0xF;
    // SCI_MODE bits
    const uint8_t _SM_SDINEW = 11;        // Bitnumber in SCI_MODE always on
    const uint8_t _SM_LINE1 = 14;        // Bitnumber in SCI_MODE for Line input
    const uint8_t _SM_RESET = 2;         // Bitnumber in SCI_MODE soft reset
    const uint8_t _SM_CANCEL = 3;         // Bitnumber in SCI_MODE cancel song


  protected:
    void wram_write(uint16_t address, uint16_t data);
    uint16_t wram_read(uint16_t address);

    uint16_t read_register(uint8_t _reg) const;
    void write_register(uint8_t _reg, uint16_t _value) const;

    void sdi_send_buffer(uint8_t* data, size_t len);
    void sdi_send_fillers(size_t length);

    inline void await_data_request() const {
      while (!digitalRead(_dreq_pin)) {
        NOP();                                   // Very short delay
      }
    }

    inline void control_mode_on() const {
      SPI.beginTransaction(_VS1053_SPI);       // Prevent other SPI users
      digitalWrite(_dcs_pin, HIGH);            // Bring slave in control mode
      digitalWrite(_cs_pin, LOW);
    }

    inline void control_mode_off() const {
      digitalWrite(_cs_pin, HIGH);             // End control mode
      SPI.endTransaction();                      // Allow other SPI users
    }

    inline void data_mode_on() const {
      SPI.beginTransaction(_VS1053_SPI);       // Prevent other SPI users
      digitalWrite(_cs_pin, HIGH);             // Bring slave in data mode
      digitalWrite(_dcs_pin, LOW);
    }

    inline void data_mode_off() const {
      digitalWrite(_dcs_pin, HIGH);            // End data mode
      SPI.endTransaction();                      // Allow other SPI users
    }

};

#endif
