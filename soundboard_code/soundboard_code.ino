//*************************************************************************************************
//* ESP32 SOUND Board with a VS1053 mp3 module                                                    *
//* Code taken from https://github.com/Edzelf/ESP32-Radio and https://github.com/Edzelf/ESP-Radio *
//* ./esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset erase_flash *
//*************************************************************************************************

// Libraries used:

// * https://github.com/me-no-dev/ESPAsyncWebServer
// * https://github.com/me-no-dev/AsyncTCP

// Wiring. Note that this is just an example.  Pins(except 18,19 and 23 of the SPI interface)
// can be configured in the config page of the web interface.
// ESP32dev Signal  Wired to VS1053
// -------- ------  -------------------
// GPIO16           pin 1 XDCS
// GPIO5            pin 2 XCS
// GPIO4            pin 4 DREQ
// GPIO18   SCK     pin 5 SCK
// GPIO19   MISO    pin 7 MISO
// GPIO23   MOSI    pin 6 MOSI
// -------  ------  -------------------
// GND      -       pin 8 GND
// VCC 5 V  -       pin 9 5V
// EN       -       pin 3 XRST


//**************************************************************************************************
// Forward declaration of various functions.                                                       *
//**************************************************************************************************
void        handlebyte_ch(uint8_t b, bool force = false);

// Release Notes
// 26-11-2017 Initial Code started

#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include "DebugPrint.h"
#include "Vs1053Esp32.h"

// defines and includes
#define VERSION "So, 26 Nov 2017 21:49:00 GMT"

// vs1053 pins
#define VS1053_DCS    16
#define VS1053_CS     5
#define VS1053_DREQ   4
#define SPI_SCK_PIN   18
#define SPI_MISO_PIN  19
#define SPI_MOSI_PIN  23

#define NAME "SeppelsSB"

// wifi settings
#define WIFI_AP_MODE false
#define WIFI_SSID "suckOnMe"
#define WIFI_PASS "leatomhannes"

int wifiConnectionCount = 0;
int wifiConnectionMaxCount = 10;


// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
// Use a multiple of 1024 for optimal handling of bufferspace.  See definition of tmpbuff.
#define RINGBFSIZ 40960

// global vars
AsyncWebServer   cmdserver(80);                        // Instance of embedded webserver on port 80
File             mp3file;                               // File containing mp3 on SPIFFS

// TODO: check which are needed
enum datamode_t {INIT = 1, HEADER = 2, DATA = 4,        // State for datastream
                 METADATA = 8, PLAYLISTINIT = 16,
                 PLAYLISTHEADER = 32, PLAYLISTDATA = 64,
                 STOPREQD = 128, STOPPED = 256
                };
datamode_t       datamode;                                // State of datastream
uint16_t         rcount = 0;                              // Number of bytes in ringbuffer
uint16_t         rbwindex = 0;                            // Fill pointer in ringbuffer
uint8_t*         ringbuf;                                 // Ringbuffer for VS1053
uint16_t         rbrindex = RINGBFSIZ - 1;                // Emptypointer in ringbuffer

int              chunkcount = 0;                          // Counter for chunked transfer
bool             chunked = false;                         // Station provides chunked transfer TODO: Not needed
bool             filereq = false;                         // Request for new file to play TODO: can filereq and filetoplay be one ?
String           fileToPlay;                              // the file to play
uint8_t          volume = 100;                             // the volume of the vs1053

int8_t           statusLedPin = LED_BUILTIN;
bool             wifiTurnedOn = false;
bool             turnWifiOn = false;

// pins for playing a mp3 via buttons
struct soundPin_struct
{
  int8_t gpio;                                  // Pin number
  bool curr;                                    // Current state, true = HIGH, false = LOW
  String sound;                                 // which sound nr to play
  bool wifiBtn;                                 // when true this is the button handling the wifi mode
} ;

soundPin_struct soundPins[] = {
  {12, false, "1", false},
  {13, false, "2", false},
  {14, false, "3", true}
};


// what time is the debounce delay ?
unsigned long lastButtonCheck = 0;
unsigned long debounceDelay = 100;

// we need debug :)
DebugPrint dbg;

// the soundboard
Vs1053Esp32 vs1053player(VS1053_CS, VS1053_DCS, VS1053_DREQ);


//**************************************************************************************************
//                                           S E T U P                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.println();

  dbg.printd("Starting ESP32-soundboard Version %s...  Free memory %d", VERSION, ESP.getFreeHeap());

  // Init VSPI bus with default or modified pins
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  // start spiffs
  if (!SPIFFS.begin(true)) {
    dbg.printd("SPIFFS Mount Failed");
  }

  dbg.printd("Setting status led on pin: %d", statusLedPin);
  pinMode(statusLedPin, OUTPUT);


  initSoundButtons();

  // Create ring buffer
  ringbuf = (uint8_t*) malloc ( RINGBFSIZ ) ;
  // Initialize VS1053 player
  vs1053player.begin();

  delay(10);

  // Handle startpage
  cmdserver.on ( "/", handleCmd ) ;
  // Handle file from FS
  cmdserver.onNotFound(handleFS);
  // Handle file uploads
  cmdserver.onFileUpload(handleFileUpload);
  // start http server
  cmdserver.begin();

  wifiTurnedOn = false;
  turnWifiOn = false;
}

/**
   Main loop
*/
void loop() {
  vs1053player.setVolume(volume);
  buttonLoop();
  mp3loop();
  startWifi();
}


//**************************************************************************************************
//                              INIT THE SOUND BUTTONS                                             *
//**************************************************************************************************
void initSoundButtons() {
  // init sound button pins
  dbg.printd("Initializing: Buttons");
  int8_t buttonPin;
  for (int i = 0 ; (buttonPin = soundPins[i].gpio) > 0 ; i++ ) {
    dbg.printd("Initializing Button at pin: %d", buttonPin);
    pinMode(buttonPin, INPUT_PULLUP);
    soundPins[i].curr = digitalRead(buttonPin);
    dbg.printd("Button at pin: %d is in state %d", buttonPin, soundPins[i].curr);
  }
}

//**************************************************************************************************
//                                          START WIFI                                             *
//**************************************************************************************************
// Starts the esp32 ap mode                                                                        *
//**************************************************************************************************
void startWifi() {

  if (!turnWifiOn && wifiTurnedOn) {
    dbg.printd("Turning off wifi");
    wifiTurnedOn = false;    
    
    /*if (WIFI_AP_MODE) {
      WiFi.softAPdisconnect(true); // turn off wifi ?
    }*/
     WiFi.enableAP(false);
     WiFi.enableSTA(false);
  }

  if (!wifiTurnedOn && turnWifiOn) {

    dbg.printd("Turning on wifi");

    wifiTurnedOn = true;
    
    if (WIFI_AP_MODE) {
      //WiFi.disconnect();                                   // After restart the router could DISABLED lead to reboots with SPIFFS
      //WiFi.softAPdisconnect(true);                         // still keep the old connection
      dbg.printd("Trying to setup AP with name %s and password %s.", WIFI_SSID, WIFI_PASS);
      WiFi.softAP(WIFI_SSID, WIFI_PASS);                        // This ESP will be an AP
      dbg.printd("IP = 192.168.4.1");             // Address for AP
      delay(5000);
    } else {
      dbg.printd("Trying to setup wifi with ssid %s and password %s.", WIFI_SSID, WIFI_PASS);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      while (WiFi.status() != WL_CONNECTED) {        
        delay(500);
        digitalWrite(statusLedPin, true);
        wifiConnectionCount++;
        dbg.printd("Waiting for wifi connection .");
        digitalWrite(statusLedPin, false);
        if (wifiConnectionMaxCount == wifiConnectionCount) {
          dbg.printd("Could not connect to wifi");
          break;
        }
      }

      Serial.println(WiFi.localIP());
    }
  }

  digitalWrite(statusLedPin, wifiTurnedOn);
}


//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// Handling of the various commands from remote (case sensitive). All commands have the    *
// form "/?parameter[=value]".  Example: "/?volume=50".                                    *
// The startpage will be returned if no arguments are given.                               *
// Multiple parameters are ignored.  An extra parameter may be "version=<random number>"   *
// in order to prevent browsers like Edge and IE to use their cache.  This "version" is    *
// ignored.                                                                                *
// Example: "/?upvolume=5&version=0.9775479450590543"                                      *
// The save and the list commands are handled specially.                                   *
//******************************************************************************************
void handleCmd ( AsyncWebServerRequest* request ) {

  // Get number of arguments
  int params = request->params();

  // no params ? then do nothing here
  if (params == 0) {
    request->send(202, "text/plain", "No valid command.");
    return;
  }

  // read the argument and its value from the first parameter
  AsyncWebParameter* p = request->getParam(0);
  String argument = p->name();
  String value = p->value();

  // Station in the form address:port
  if ( argument == "play" )
  {
    dbg.printd("Play file: %s requested", fileToPlay.c_str());
    initStartSound(value);
    request->send ( 200, "text/plain", "Play file:" + fileToPlay);
    return;
  }

  // set the volume
  if (argument == "volume") {
    uint8_t newVol = value.toInt();
    if (newVol >= 0 && newVol <= 100) {
      volume = newVol;
      request->send ( 200, "text/plain", "Volume is now:" + value);
      return;
    }
  }

  request->send(404, "text/plain", "No valid command.");
}



//******************************************************************************************
//                         H A N D L E F I L E U P L O A D                                 *
//******************************************************************************************
// Handling of upload request.  Write file to SPIFFS.                                      *
//******************************************************************************************
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String          path;                              // Filename including "/"
  static File     f;                                 // File handle output file
  char*           reply;                             // Reply for webserver
  static uint32_t t;                                 // Timer for progress messages
  uint32_t        t1;                                // For compare
  static uint32_t totallength;                       // Total file length
  static size_t   lastindex;                         // To test same index

  if (index == 0) {
    path = String("/") + filename;                // Form SPIFFS filename
    SPIFFS.remove(path);                          // Remove old file
    f = SPIFFS.open(path, "w");                   // Create new file
    t = millis();                                    // Start time
    totallength = 0;                                 // Total file lengt still zero
    lastindex = 0;                                   // Prepare test
  }
  t1 = millis();                                     // Current timestamp
  // Yes, print progress
  dbg.printd("File upload %s, t = %d msec, len %d, index %d", filename.c_str(), t1 - t, len, index);
  if (len) {                                       // Something to write?
    if ((index != lastindex) || (index == 0)) { // New chunk?
      f.write(data, len);                         // Yes, transfer to SPIFFS
      totallength += len;                            // Update stored length
      lastindex = index;                             // Remenber this part
    }
  }
  if (final) {                                    // Was this last chunk?
    f.close();                                       // Yes, clode the file
    reply = dbg.printd("File upload %s, %d bytes finished", filename.c_str(), totallength);
    request->send(200, "", reply);
  }
}


//******************************************************************************************
//                                H A N D L E F S                                          *
//******************************************************************************************
// Handling of requesting files from the SPIFFS. Example: /favicon.ico                     *
//******************************************************************************************
void handleFS(AsyncWebServerRequest* request)
{
  handleFSf(request, request->url());               // Rest of handling
}

//******************************************************************************************
//                                H A N D L E F S F                                        *
//******************************************************************************************
// Handling of requesting files from the SPIFFS/PROGMEM. Example: /favicon.ico             *
//******************************************************************************************
void handleFSf(AsyncWebServerRequest* request, const String& filename)
{
  static String          ct;                           // Content type
  AsyncWebServerResponse *response;                    // For extra headers

  dbg.printd("FileRequest received %s", filename.c_str());
  ct = getContentType(filename);                    // Get content type
  if ((ct == "") || (filename == "") || (filename == "/favicon.ico"))         // Empty is illegal
  {
    request->send(404, "text/plain", "File not found");
  }
  else
  {

    response = request->beginResponse(SPIFFS, filename, ct);
    // Add extra headers
    response->addHeader("Server", NAME);
    response->addHeader("Cache-Control", "max-age=3600");
    response->addHeader("Last-Modified", VERSION);
    request->send(response);
  }
  dbg.printd("Response sent");
}


//******************************************************************************************
//                             G E T C O N T E N T T Y P E                                 *
//******************************************************************************************
// Returns the contenttype of a file to send.                                              *
//******************************************************************************************
String getContentType(String filename) {
  if (filename.endsWith(".mp3")) return "audio/mpeg";
  return "text/plain";
}


/**
    Opens a local file from the given fs
*/
bool openLocalFile(fs::FS &fs, const char * path) {

  dbg.printd("Opening file %s", path);

  mp3file = fs.open(path, "r");                           // Open the file
  if (!mp3file) {
    dbg.printd("Error opening file %s", path);
    return false;
  }

  return true;
}

//**************************************************************************************************
//                                     B U T T O N L O O P                                         *
//**************************************************************************************************
void buttonLoop() {

  // do nothing when any button was pushed in the debouncedelay time
  if ((millis() - lastButtonCheck) < debounceDelay) {
    return;
  }

  // debounce time over
  lastButtonCheck = millis();


  int8_t buttonPin;
  for (int i = 0 ; (buttonPin = soundPins[i].gpio) > 0 ; i++ ) {
    // get the current state of the button
    bool level = (digitalRead(buttonPin) == HIGH);

    // Change seen?
    if (level != soundPins[i].curr) {
      // And the new level
      soundPins[i].curr = level;
      // HIGH to LOW change?
      if (!level) {
        if (soundPins[i].wifiBtn == false) {
          dbg.printd("GPIO_%02d is now LOW playing sound: %s", buttonPin, soundPins[i].sound);
          initStartSound(soundPins[i].sound);
        } else {
          turnWifiOn = !turnWifiOn;
          dbg.printd("GPIO_%02d is now LOW switching wifi to: %d", buttonPin, turnWifiOn);
        }
      }
    }
  }
}

//**************************************************************************************************
//                                      INIT SOUND TO PLAY                                         *
//**************************************************************************************************
void initStartSound(String soundToPlay) {
  if ( datamode & ( HEADER | DATA | METADATA | PLAYLISTINIT |
                    PLAYLISTHEADER | PLAYLISTDATA ) )
  {
    datamode = STOPREQD ;                           // Request STOP
  }

  fileToPlay = "/" + soundToPlay + ".mp3";
  filereq = true;
}

//**************************************************************************************************
//                                           M P 3 L O O P                                         *
//**************************************************************************************************
// Called from the mail loop() for the mp3 functions.                                              *
// A connection to an MP3 server is active and we are ready to receive data.                       *
// Normally there is about 2 to 4 kB available in the data stream.  This depends on the            *
// sender.                                                                                         *
//**************************************************************************************************
void mp3loop() {

  static uint8_t  tmpbuff[RINGBFSIZ / 20];              // Input buffer for mp3 stream
  uint32_t        rs;                                   // Free space in ringbuffer
  uint32_t        av;                                   // Available in stream
  uint32_t        maxchunk;                             // Max number of bytes to read
  int             res = 0;                              // Result reading from mp3 stream

  // Try to keep the ringbuffer filled up by adding as much bytes as possible
  // Test op playing
  if (datamode & (INIT | HEADER | DATA |
                  METADATA | PLAYLISTINIT |
                  PLAYLISTHEADER |
                  PLAYLISTDATA)) {

    // Get free ringbuffer space
    rs = ringspace();

    // Need to fill the ringbuffer?
    if (rs >= sizeof(tmpbuff)) {

      // Reduce byte count for this mp3loop()
      maxchunk = sizeof(tmpbuff);

      // Bytes left in file
      av = mp3file.available();
      // Reduce byte count for this mp3loop()
      if (av < maxchunk) {
        maxchunk = av;
      }
      // Anything to read?
      if (maxchunk) {
        // Read a block of data
        res = mp3file.read(tmpbuff, maxchunk);
      }

      // Transfer to ringbuffer
      putring(tmpbuff, res);
    }
  }

  // Try to keep VS1053 filled
  while (vs1053player.data_request() && ringavail()) {
    // Yes, handle it
    handlebyte_ch(getring());
  }

  // STOP requested?
  if (datamode == STOPREQD) {
    dbg.printd("STOP requested");

    mp3file.close();

    handlebyte_ch(0, true);                          // Force flush of buffer
    vs1053player.setVolume(0);                       // Mute
    vs1053player.stopSong();                            // Stop playing
    emptyring();                                        // Empty the ringbuffer
    //TODO: check if nedded and if so enabel all outcommented lines
    //metaint = 0;                                        // No metaint known now
    datamode = STOPPED;                                 // Yes, state becomes STOPPED
    delay(500);
  }

  if (datamode & (INIT | HEADER | DATA |           // Test op playing
                  METADATA | PLAYLISTINIT |
                  PLAYLISTHEADER |
                  PLAYLISTDATA))
  {
    av = mp3file.available();                           // Bytes left in file
    if ((av == 0) && (ringavail() == 0))         // End of mp3 data?
    {
      datamode = STOPREQD;                              // End of local mp3-file detected
      filereq = false;
    }
  }

  // new file to play ?
  if (filereq) {
    filereq = false;

    openLocalFile(SPIFFS, fileToPlay.c_str());

    // set the mode to data
    datamode = DATA;

  }

}

//**************************************************************************************************
// Ringbuffer(fifo) routines.                                                                     *
//**************************************************************************************************
//**************************************************************************************************
//                                          R I N G S P A C E                                      *
//**************************************************************************************************
uint16_t ringspace() {
  // Free space available
  return (RINGBFSIZ - rcount);
}

//**************************************************************************************************
//                                        P U T R I N G                                            *
//**************************************************************************************************
// Version of putring to write a buffer to the ringbuffer.                                         *
// No check on available space.  See ringspace().                                                  *
//**************************************************************************************************
void putring(uint8_t* buf, uint16_t len) {
  uint16_t partl;                                                // Partial length to xfer

  // anything to do?
  if (len) {
    // First see if we must split the transfer.  We cannot write past the ringbuffer end.
    if ((rbwindex + len) >= RINGBFSIZ) {
      partl = RINGBFSIZ - rbwindex;                              // Part of length to xfer
      memcpy(ringbuf + rbwindex, buf, partl);                 // Copy next part
      rbwindex = 0;
      rcount += partl;                                           // Adjust number of bytes
      buf += partl;                                              // Point to next free byte
      len -= partl;                                              // Adjust rest length
    }
    // Rest to do?
    if (len) {
      memcpy(ringbuf + rbwindex, buf, len);                   // Copy full or last part
      rbwindex += len;                                           // Point to next free byte
      rcount += len;                                             // Adjust number of bytes
    }
  }
}

//**************************************************************************************************
//                                         R I N G A V A I L                                       *
//**************************************************************************************************
inline uint16_t ringavail() {
  return rcount;                     // Return number of bytes available for getring()
}

//**************************************************************************************************
//                                        G E T R I N G                                            *
//**************************************************************************************************
uint8_t getring() {
  // Assume there is always something in the bufferpace.  See ringavail()
  if (++rbrindex == RINGBFSIZ)    // Increment pointer and
  {
    rbrindex = 0;                    // wrap at end
  }
  rcount--;                          // Count is now one less
  return *(ringbuf + rbrindex);      // return the oldest byte
}

//**************************************************************************************************
//                                       E M P T Y R I N G                                         *
//**************************************************************************************************
void emptyring() {
  rbwindex = 0;                      // Reset ringbuffer administration
  rbrindex = RINGBFSIZ - 1;
  rcount = 0;
}

//**************************************************************************************************
//                                   H A N D L E B Y T E _ C H                                     *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// Chunked transfer encoding aware. Chunk extensions are not supported.                            *
//**************************************************************************************************
void handlebyte_ch(uint8_t b, bool force)
{
  static int  chunksize = 0;                         // Chunkcount read from stream

  /*if (chunked && !force &&
      (datamode & (DATA |                        // Test op DATA handling
                   METADATA |
                   PLAYLISTDATA)))
    {
    if (chunkcount == 0)                          // Expecting a new chunkcount?
    {
      if (b == '\r')                             // Skip CR
      {
        return;
      }
      else if (b == '\n')                        // LF ?
      {
        chunkcount = chunksize;                     // Yes, set new count
        chunksize = 0;                              // For next decode
        return;
      }
      // We have received a hexadecimal character.  Decode it and add to the result.
      b = toupper(b) - '0';                      // Be sure we have uppercase
      if (b > 9)
      {
        b = b - 7;                                  // Translate A..F to 10..15
      }
      chunksize = (chunksize << 4) + b;
    }
    else
    {
      handlebyte(b, force);                       // Normal data byte
      chunkcount--;                                  // Update count to next chunksize block
    }
    }
    else
    {*/
  handlebyte(b, force);                         // Normal handling of this byte
  //}
}

//**************************************************************************************************
//                                   H A N D L E B Y T E                                           *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// This byte will be send to the VS1053 most of the time.                                          *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.           *
// Set force to true if chunkbuffer must be flushed.                                               *
//**************************************************************************************************
void handlebyte(uint8_t b, bool force)
{
  static uint16_t  playlistcnt;                       // Counter to find right entry in playlist
  static bool      firstmetabyte;                     // True if first metabyte(counter)
  static int       LFcount;                           // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32]; // Buffer for chunk
  static int       bufcnt = 0;                        // Data in chunk
  static bool      firstchunk = true;                 // First chunk as input
  String           lcml;                              // Lower case metaline
  String           ct;                                // Contents type
  static bool      ctseen = false;                    // First line of header seen or not
  int              inx;                               // Pointer in metaline
  int              i;                                 // Loop control

  // Handle next byte of MP3/Ogg data
  if (datamode == DATA)  {

    buf[bufcnt++] = b;                                // Save byte in chunkbuffer
    if (bufcnt == sizeof(buf) || force)            // Buffer full?
    {
      if (firstchunk)
      {
        firstchunk = false;
        dbg.printd("First chunk:");                  // Header for printout of first chunk
        for (i = 0; i < 32; i += 8)              // Print 4 lines
        {
          dbg.printd("%02X %02X %02X %02X %02X %02X %02X %02X",
                     buf[i],   buf[i + 1], buf[i + 2], buf[i + 3],
                     buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
        }
      }
      vs1053player.playChunk(buf, bufcnt);         // Yes, send to player
      bufcnt = 0;                                     // Reset count
    }
    return;
  }
}
