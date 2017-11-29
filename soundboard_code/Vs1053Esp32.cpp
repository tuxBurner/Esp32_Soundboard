#include "Arduino.h"

#include "Vs1053Esp32.h"

/**
   Constuctor
*/
Vs1053Esp32::Vs1053Esp32(uint8_t cs_pin, uint8_t dcs_pin, uint8_t dreq_pin) {
  _cs_pin = cs_pin;
  _dcs_pin = dcs_pin;
  _dreq_pin = dreq_pin;
}


/**
   Must be called before doing anything with the vs1053 chip
*/
void Vs1053Esp32::begin() {
  pinMode    (_dreq_pin,  INPUT);                   // DREQ is an input
  pinMode    (_cs_pin,    OUTPUT);                  // The SCI and SDI signals
  pinMode    (_dcs_pin,   OUTPUT);
  digitalWrite(_dcs_pin,   HIGH);                    // Start HIGH for SCI en SDI
  digitalWrite(_cs_pin,    HIGH);
  delay(100);

  // Init SPI in slow mode(0.2 MHz)
  _VS1053_SPI = SPISettings(200000, MSBFIRST, SPI_MODE0);
  delay(20);

  testComm("Slow SPI, Testing VS1053 read/write registers...");

  // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
  // when playing MP3.  You can modify the board, but there is a more elegant way:
  wram_write(0xC017, 3);                            // GPIO DDR = 3
  wram_write(0xC019, 0);                            // GPIO ODATA = 0

  delay(100);

  // Do a soft reset
  softReset();

  // Switch on the analog parts

  // 44.1kHz + stereo
  write_register(_SCI_AUDATA, 44100 + 1);

  // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
  write_register(_SCI_CLOCKF, 6 << 12);              // Normal clock settings multiplyer 3.0 = 12.2 MHz

  //SPI Clock to 4 MHz. Now you can set high speed SPI clock.
  _VS1053_SPI = SPISettings(4000000, MSBFIRST, SPI_MODE0);

  write_register(_SCI_MODE, _BV(_SM_SDINEW) | _BV(_SM_LINE1));
  testComm("Fast SPI, Testing VS1053 read/write registers again...");

  delay(10);
  await_data_request();
  _endFillByte = wram_read(0x1E06) & 0xFF;

  _dbg.printd("endFillByte is %X", _endFillByte);

  delay(100);
}

/**
   Set volume.  Both left and right.
   Input value is 0..100.  100 is the loudest.
   Clicking reduced by using 0xf8 to 0x00 as limits.
*/
void Vs1053Esp32::setVolume(uint8_t vol) {
  uint16_t value;                                      // Value to send to SCI_VOL

  if (vol != _curvol) {
    _curvol = vol;                                      // Save for later use
    value = map(vol, 0, 100, 0xF8, 0x00);           // 0..100% to one channel
    value = (value << 8) | value;
    write_register(_SCI_VOL, value);                 // Volume left and right
  }
}

void Vs1053Esp32::startSong() {
  sdi_send_fillers(10);
}

void Vs1053Esp32::playChunk(uint8_t* data, size_t len) {
  sdi_send_buffer(data, len);
}

void Vs1053Esp32::stopSong() {
  uint16_t modereg;                     // Read from mode register
  int      i;                           // Loop control

  sdi_send_fillers(2052);
  delay(10);
  write_register(_SCI_MODE, _BV(_SM_SDINEW) | _BV(_SM_CANCEL));
  for (i = 0; i < 200; i++)
  {
    sdi_send_fillers(32);
    modereg = read_register(_SCI_MODE);  // Read status
    if ((modereg & _BV(_SM_CANCEL)) == 0)
    {
      sdi_send_fillers(2052);
      _dbg.printd("Song stopped correctly after %d msec", i * 10);
      return;
    }
    delay(10);
  }
  printDetails("Song stopped incorrectly!");
}

/**
   Set bass/treble(4 nibbles)
*/
void Vs1053Esp32::setTone(uint8_t *rtone) {
  // Set tone characteristics.  See documentation for the 4 nibbles.
  uint16_t value = 0;                                  // Value to send to SCI_BASS
  int      i;                                          // Loop control

  for (i = 0; i < 4; i++)
  {
    value = (value << 4) | rtone[i];               // Shift next nibble in
  }
  write_register(_SCI_BASS, value);                  // Volume left and right
}

/**
  Get the currenet volume setting.
*/
uint8_t Vs1053Esp32::getVolume() {
  return _curvol;
}

void Vs1053Esp32::printDetails(const char *header) {
  uint16_t     regbuf[16];
  uint8_t      i;

  _dbg.printd(header);
  _dbg.printd("REG   Contents");
  _dbg.printd("---   -----");
  for (i = 0; i <= _SCI_num_registers; i++)
  {
    regbuf[i] = read_register(i);
  }
  for (i = 0; i <= _SCI_num_registers; i++)
  {
    delay(5);
    _dbg.printd("%3X - %5X", i, regbuf[i]);
  }
}


/**
   Test the communication with the VS1053 module.  The result wille be returned.
   If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
   in order to prevent an endless loop waiting for this signal.  The rest of the
   software will still work, but readbacks from VS1053 will fail.
*/
bool Vs1053Esp32::testComm(const char *header) {

  int       i;                                         // Loop control
  uint16_t  r1, r2, cnt = 0;
  uint16_t  delta = 300;                               // 3 for fast SPI

  if (!digitalRead(_dreq_pin))
  {
    _dbg.printd("VS1053 not properly installed!");
    // Allow testing without the VS1053 module
    pinMode(_dreq_pin,  INPUT_PULLUP);               // DREQ is now input with pull-up
    return false;                                      // Return bad result
  }
  // Further TESTING.  Check if SCI bus can write and read without errors.
  // We will use the volume setting for this.
  // Will give warnings on serial output if DEBUG is active.
  // A maximum of 20 errors will be reported.
  if (strstr(header, "Fast"))
  {
    delta = 3;                                         // Fast SPI, more loops
  }
  _dbg.printd(header);                                 // Show a header
  for (i = 0; (i < 0xFFFF) && (cnt < 20); i += delta)
  {
    write_register(_SCI_VOL, i);                     // Write data to SCI_VOL
    r1 = read_register(_SCI_VOL);                    // Read back for the first time
    r2 = read_register(_SCI_VOL);                    // Read back a second time
    if (r1 != r2 || i != r1 || i != r2)             // Check for 2 equal reads
    {
      _dbg.printd("VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2);
      cnt++;
      delay(10);
    }
  }
  return (cnt == 0);                               // Return the result
}

void Vs1053Esp32::wram_write(uint16_t address, uint16_t data) {
  write_register(_SCI_WRAMADDR, address);
  write_register(_SCI_WRAM, data);
}

uint16_t Vs1053Esp32::wram_read(uint16_t address) {
  write_register(_SCI_WRAMADDR, address);            // Start reading from WRAM
  return read_register(_SCI_WRAM);                   // Read back result
}

void Vs1053Esp32::write_register(uint8_t _reg, uint16_t _value) const {
  control_mode_on();
  SPI.write(2);                                // Write operation
  SPI.write(_reg);                             // Register to write(0..0xF)
  SPI.write16(_value);                         // Send 16 bits data
  await_data_request();
  control_mode_off();
}


uint16_t Vs1053Esp32::read_register(uint8_t _reg) const {
  uint16_t result;

  control_mode_on();
  SPI.write(3);                                // Read operation
  SPI.write(_reg);                             // Register to write(0..0xF)
  // Note: transfer16 does not seem to work
  result = (SPI.transfer(0xFF) << 8) |      // Read 16 bits data
           (SPI.transfer(0xFF));
  await_data_request();                           // Wait for DREQ to be HIGH again
  control_mode_off();
  return result;
}

void Vs1053Esp32::sdi_send_fillers(size_t len) {
  size_t chunk_length;                            // Length of chunk 32 byte or shorter

  data_mode_on();
  while (len)                                  // More to do?
  {
    await_data_request();                         // Wait for space available
    chunk_length = len;
    if (len > _vs1053_chunk_size)
    {
      chunk_length = _vs1053_chunk_size;
    }
    len -= chunk_length;
    while (chunk_length--)
    {
      SPI.write(_endFillByte);
    }
  }
  data_mode_off();
}

void Vs1053Esp32::sdi_send_buffer(uint8_t* data, size_t len) {
  size_t chunk_length;                            // Length of chunk 32 byte or shorter

  data_mode_on();
  while (len)                                  // More to do?
  {
    await_data_request();                         // Wait for space available
    chunk_length = len;
    if (len > _vs1053_chunk_size)
    {
      chunk_length = _vs1053_chunk_size;
    }
    len -= chunk_length;
    SPI.writeBytes(data, chunk_length);
    data += chunk_length;
  }
  data_mode_off();
}

void Vs1053Esp32::softReset() {
  write_register(_SCI_MODE, _BV(_SM_SDINEW) | _BV(_SM_RESET));
  delay(10);
  await_data_request();
}
