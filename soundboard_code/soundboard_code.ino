//*************************************************************************************************
//* ESP32 SOUND Board with a VS1053 mp3 module                                                    *
//* Code taken from https://github.com/Edzelf/ESP32-Radio and https://github.com/Edzelf/ESP-Radio *
//* ./esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset erase_flash *
//*************************************************************************************************


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
void        handlebyte_ch(uint8_t b, bool force);

// Release Notes
// 26-11-2017 Initial Code started
// 16-12-2017 Got the Stuff Working

#include <SPI.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include "DebugPrint.h"
#include "Vs1053Esp32.h"

// defines and includes
#define VERSION "Sa, 16 Dec 2017"

// vs1053 pins
#define VS1053_DCS    16
#define VS1053_CS     5
#define VS1053_DREQ   4
#define SPI_SCK_PIN   18
#define SPI_MISO_PIN  19
#define SPI_MOSI_PIN  23

#define NAME "SeppelsSB"

// wifi settings
bool WIFI_AP_MODE = false;
#define WIFI_SSID "suckOnMe"
#define WIFI_PASS "leatomhannes"
#define WIFI_AP_SSID "soundboard"
#define WIFI_AP_PASS "pass"

int wifiConnectionCount = 0;
int wifiConnectionMaxCount = 30;


// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
// Use a multiple of 1024 for optimal handling of bufferspace.  See definition of tmpbuff.
//#define RINGBFSIZ 40960
#define RINGBFSIZ 256

// global vars
File             mp3file;                               // File containing mp3 on SPIFFS

// Data mode the soundboard can currently have
enum datamode_t {DATA = 1,        // State for datastream
                 STOPREQD = 2,  // Request for stopping current song
                 STOPPED = 4    // State for stopped
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
  String sound;                                 // which sound nr to play or wifi when to handle wifi stuff
} ;

/**
   The actual button mapping
*/
soundPin_struct soundPins[] = {
  {15, false, "wifi"}, // blue square
  {12, false, "1"}, // sheep
  {13, false, "2"}, // dog
  {14, false, "3"}, // cat
  {27, false, "4"}, // horse
  {26, false, "5"}, // chicken
  {25, false, "6"}, // pig
  {33, false, "7"}, // cow
  {32, false, "8"}, // duck
  {22, false, "9"}, // bell
  {3, false, "11"},  // purple square
  {21, false, "12"} // red square
};


// what time is the debounce delay ?
unsigned long lastButtonCheck = 0;
// what is the debounce delay
unsigned long debounceDelay = 100;

// we need debug :)
DebugPrint dbg;

// the soundboard
Vs1053Esp32 vs1053player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// the http server
WiFiServer httpServer(80);


//**************************************************************************************************
//                                           S E T U P                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.println();

  dbg.print("Main", "Starting ESP32-soundboard Version %s...  Free memory %d", VERSION, ESP.getFreeHeap());

  // Init VSPI bus with default or modified pins
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  // start spiffs
  if (!SPIFFS.begin(true)) {
    dbg.print("Main", "SPIFFS Mount Failed");
  }

  dbg.print("Led", "Setting status led on pin: %d", statusLedPin);
  pinMode(statusLedPin, OUTPUT);

  initSoundButtons();

  // Create ring buffer
  ringbuf = (uint8_t*) malloc ( RINGBFSIZ ) ;
  // Initialize VS1053 player
  vs1053player.begin();

  delay(10);

  wifiTurnedOn = false;
  turnWifiOn = true;
}

/**
   Main loop
*/
void loop() {
  vs1053player.setVolume(volume);
  buttonLoop();
  mp3loop();
  startWifi();
  httpServerLoop();
}

//**************************************************************************************************
//                              INIT THE HTTP SERVER                                               *
//**************************************************************************************************
void initHttpServer() {
  httpServer.begin();
}


//**************************************************************************************************
//                              INIT THE SOUND BUTTONS                                             *
//**************************************************************************************************
void initSoundButtons() {
  // init sound button pins
  dbg.print("Button", "Initializing: Buttons");
  int8_t buttonPin;
  for (int i = 0 ; (buttonPin = soundPins[i].gpio) > 0 ; i++ ) {
    dbg.print("Button", "Initializing Button at pin: %d", buttonPin);
    pinMode(buttonPin, INPUT_PULLUP);
    soundPins[i].curr = digitalRead(buttonPin);
    dbg.print("Button", "Button at pin: %d is in state %d", buttonPin, soundPins[i].curr);
  }
}

//**************************************************************************************************
//                                          START WIFI                                             *
//**************************************************************************************************
// Starts the esp32 ap mode                                                                        *
//**************************************************************************************************
void startWifi() {

  if (!turnWifiOn && wifiTurnedOn) {
    dbg.print("Wifi", "Turning off wifi");
    wifiTurnedOn = false;
    WiFi.enableAP(false);
    WiFi.enableSTA(false);
    return;
  }

  if (!wifiTurnedOn && turnWifiOn) {

    dbg.print("Wifi", "Turning on wifi");



    if (WIFI_AP_MODE) {
      dbg.print("Wifi", "Trying to setup AP with name: %s and password: %s.", WIFI_AP_SSID, WIFI_AP_PASS);
      WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);                        // This ESP will be an AP
      dbg.print("Wifi", "AP IP = 192.168.4.1");             // Address for AP
      delay(1000);
      wifiTurnedOn = true;
    } else {
      dbg.print("Wifi", "Trying to setup wifi with ssid: %s and password: %s.", WIFI_SSID, WIFI_PASS);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        digitalWrite(statusLedPin, true);
        wifiConnectionCount++;
        dbg.print("Wifi", "Waiting for wifi connection .");
        digitalWrite(statusLedPin, false);
        if (wifiConnectionMaxCount == wifiConnectionCount) {
          dbg.print("Wifi", "Could not connect to wifi turning ap mode on");
          WIFI_AP_MODE = true;
          break;
        }
      }

      wifiTurnedOn = true;
      dbg.print("Wifi", "Ip address of esp is %s", WiFi.localIP().toString().c_str());
    }

    // start http server
    initHttpServer();
  }

  digitalWrite(statusLedPin, wifiTurnedOn);
}

/**
   Is called in the main loop and checks if we have any http client calling the server
*/

// the action a http client wants to perform
enum httpClientAction_t {
  NONE = 1,
  FAILURE = 2,
  PLAY = 4,
  INFO = 8,
  UPLOAD_INIT = 16,
  UPLOAD_BOUNDARY_INIT = 32,
  UPLOAD_BOUNDARY_FOUND = 64,
  UPLOAD_FILE_NAME_FOUND = 128,
  UPLOAD_DATA_START = 256,
  UPLOAD_DATA_END = 512,
  DOWNLOAD = 1024
};

// current action of the http client
httpClientAction_t httpClientAction = NONE;
const String httpHeaderOk = "HTTP/1.1 200 Ok";
const String httpHeaderFailure = "HTTP/1.1 404 Not Found";

#define UPL_MAX_SIZE 65535
char uplBuf[UPL_MAX_SIZE];
int uplPos = 0;

void httpServerLoop() {
  // do we have a new client ?
  WiFiClient client = httpServer.available();

  // no client ?
  if (!client) {
    return;
  }

  dbg.print("Http", "new client connected %s", client.remoteIP().toString().c_str());

  String currentLine = "";                // make a String to hold incoming data from the client

  // the current action/state of the http client parser
  httpClientAction = NONE;

  // stores the upload boundary
  String uploadBoundary = "";

  // some data we can handle after parsinf the request for example what file to play
  String getDataToHandle = "";


  while (client.connected()) {            // loop while the client's connected
    if (client.available()) {             // if there's bytes to read from the client,
      char c = client.read();             // read a byte, then

      // read the upload data to the buffer
      if (httpClientAction == UPLOAD_DATA_START) {
        if (uplPos < UPL_MAX_SIZE) {
          uplBuf[uplPos] = c;
          uplPos++;
        } else {
          dbg.print("Http Upload", "File is bigger than the allowed: %d", UPL_MAX_SIZE);
          httpClientAction = FAILURE;
          getDataToHandle = "File to big.";
        }
      }

      if (c == '\n') {                    // if the byte is a newline character

        // client wants to play a sound on the sound board
        if (currentLine.startsWith("GET /play/") && httpClientAction == NONE) {

          dbg.print("Http", "Client wants to play a sound from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /play/", "");
          httpClientAction = PLAY;
        }

        if (currentLine.startsWith("GET /mp3/") && httpClientAction == NONE) {
          dbg.print("Http", "Client wants to download a sound from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /mp3/", "");

          httpClientAction = DOWNLOAD;
        }



        // client wants to upload a file
        if (currentLine.startsWith("POST /upload") && httpClientAction == NONE) { // upload initialized
          httpClientAction = UPLOAD_INIT;
          uplPos = 0;
        }

        // client wants to upload a file and we found a boundary
        if (currentLine.startsWith("content-type: multipart/form-data; boundary=") && httpClientAction == UPLOAD_INIT) {
          uploadBoundary = "--" + currentLine.substring(44);
          dbg.print("Http Upload", "Found boundary: %s", uploadBoundary.c_str());
          httpClientAction = UPLOAD_BOUNDARY_INIT;

        }

        // the upload boundary actualy exists in the request
        if (currentLine.startsWith(uploadBoundary) && httpClientAction == UPLOAD_BOUNDARY_INIT) {
          dbg.print("Http Upload", "Found boundary in request: %s", uploadBoundary.c_str());
          httpClientAction = UPLOAD_BOUNDARY_FOUND;
        }

        // the upload file  name has to be parsed
        if (currentLine.startsWith("Content-Disposition: form-data; name=\"file\"; filename=") && httpClientAction == UPLOAD_BOUNDARY_FOUND) {
          getDataToHandle = currentLine.substring(55, currentLine.length() - 1);
          dbg.print("Http Upload", "Filename is: %s", getDataToHandle.c_str());
          httpClientAction = UPLOAD_FILE_NAME_FOUND;
        }

        // after parsing the name and finding the first empty line we can start reading the data
        if (currentLine == "" && httpClientAction == UPLOAD_FILE_NAME_FOUND) {
          dbg.print("Http Upload", "Starting reading the data");
          httpClientAction = UPLOAD_DATA_START;
        }


        // no more data to read
        if (currentLine.startsWith(uploadBoundary) && httpClientAction == UPLOAD_DATA_START) {
          dbg.print("Http Upload", "Found boundary end in request: %s", uploadBoundary.c_str());
          // remove the boundary from the content by resetting the bufPos
          uplPos -= currentLine.length() + 4;
          httpClientAction = UPLOAD_DATA_END;
        }



        // client wants some info about this board
        if (currentLine.startsWith("GET /info") && httpClientAction == NONE) {
          httpClientAction = INFO;
        }

        // not a valid request with get or post
        if ((currentLine.startsWith("GET") || currentLine.startsWith("POST")) && httpClientAction == NONE) {
          // none of the action matches
          getDataToHandle = "Not Found";
          httpClientAction = FAILURE;
        }

        // debug request
        if (httpClientAction != UPLOAD_DATA_START && httpClientAction != FAILURE) {
          dbg.print("Http", "Client send line: %s", currentLine.c_str());
        }

        currentLine = ""; // empty the current line
      } else if (c != '\r') {  // if you got anything else but a carriage return character,
        currentLine += c;      // add it to the end of the currentLine
      }

    } else { // no more data from the client
      if (httpClientAction == PLAY) {
        httpPlaySound(client, getDataToHandle);
      }

      if (httpClientAction == INFO) {
        httpGetInfo(client);
      }

      if (httpClientAction == DOWNLOAD) {
        httpDownloadMp3(client, getDataToHandle);
      }

      if (httpClientAction == UPLOAD_DATA_END) {
        httpUPloadFinished(client, getDataToHandle);
      }

      if (httpClientAction == FAILURE) {
        httpNotFound(client, getDataToHandle);
      }

      break; // exit the main client loop
    }
  }

  // close the connection:
  client.stop();
  dbg.print("Http", "Client Disconnected.");
}

/**
   Handles the download of the given mp3
*/
void httpDownloadMp3(WiFiClient client, String fileToDownload) {

  String path = "/" + fileToDownload + ".mp3";
  dbg.print("Http download", "Streaming file: %s to client", path.c_str());

  File file = SPIFFS.open(path);

  client.println(httpHeaderOk);
  client.println("Content-type: audio/mp3");
  client.println();
  //client.println("Playing sound: " + fileToPlay);
  while (file.available()) {
    client.write(file.read());
  }
  /*while (file.available()) {
    Serial.write(file.read());
  }*/

  client.println();

  file.close();
}

/**
   Handles the request to play a sound
*/
void httpPlaySound(WiFiClient client, String fileToPlay) {

  // let the sound board play the requested file
  initStartSound(fileToPlay);

  client.println(httpHeaderOk);
  client.println("Content-type:text/html");
  client.println();
  client.println("Playing sound: " + fileToPlay);
  client.println();
}

/**
   When the upload was a success
*/
void httpUPloadFinished(WiFiClient client, String uploadedFile) {

  // write the file
  String path = "/" + uploadedFile;

  // Remove old file
  dbg.print("File", "Removing old file: %s", path.c_str());
  SPIFFS.remove(path);

  dbg.print("File", "Open file to write: %s (%d)", path.c_str(), uplPos);
  static File file = SPIFFS.open(path, "w");

  for (int i = 0; i < uplPos; i++) {
    file.print(uplBuf[i]);
  }


  file.close();
  dbg.print("File", "Done writing: %s", path.c_str());


  client.println(httpHeaderOk);
  client.println("Content-type:text/html");
  client.println();
  client.println("Uploaded file: " + uploadedFile);
  client.println();
}


/**
   Displays the info to the client
*/
void httpGetInfo(WiFiClient client) {

  // let the sound board play the requested file
  initStartSound(fileToPlay);

  client.println(httpHeaderOk);
  client.println("Content-type:text/html");
  client.println();
  client.println("TODO: INFO HERE");
  client.println();
}


/**
   Handles a not found request
*/
void httpNotFound(WiFiClient client, String reason) {
  client.println(httpHeaderFailure);
  client.println("Content-type:text/html");
  client.println();
  client.println(reason);
  client.println();
}

/**
    Opens a local file from the given fs
*/
bool openLocalFile(fs::FS &fs, const char * path) {

  dbg.print("MP3", "Opening file %s", path);

  mp3file = fs.open(path, "r");                           // Open the file
  if (!mp3file) {
    dbg.print("MP3", "Error opening file %s", path);
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
  for (int i = 0 ; i < 12 ; i++ ) {

    buttonPin = soundPins[i].gpio;
    // get the current state of the button
    bool level = (digitalRead(buttonPin) == HIGH);

    // Change seen?
    if (level != soundPins[i].curr) {
      // And the new level
      soundPins[i].curr = level;
      // HIGH to LOW change?
      if (!level) {
        if (soundPins[i].sound != "wifi") {
          dbg.print("Button", "GPIO_%02d is now LOW playing sound: %s", buttonPin, soundPins[i].sound.c_str());
          initStartSound(soundPins[i].sound);
        } else {
          turnWifiOn = !turnWifiOn;
          dbg.print("Button", "GPIO_%02d is now LOW switching wifi to: %d", buttonPin, turnWifiOn);
        }
      }
      return;
    } // level off the button changed
  }
}

//**************************************************************************************************
//                                      INIT SOUND TO PLAY                                         *
//**************************************************************************************************
void initStartSound(String soundToPlay) {
  if (datamode & ( DATA)) {
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
  if (datamode & (DATA)) {

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
    handlebyte_ch(getring(), false);
  }

  // STOP requested?
  if (datamode == STOPREQD) {
    dbg.print("Sound", "STOP requested");

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

  // Test op playing
  if (datamode & (DATA))  {
    av = mp3file.available();                           // Bytes left in file
    if ((av == 0) && (ringavail() == 0)) {        // End of mp3 data?
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
void handlebyte_ch(uint8_t b, bool force) {
  static int  chunksize = 0;                         // Chunkcount read from stream
  handlebyte(b, force);                         // Normal handling of this byte
}

//**************************************************************************************************
//                                   H A N D L E B Y T E                                           *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// This byte will be send to the VS1053 most of the time.                                          *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.           *
// Set force to true if chunkbuffer must be flushed.                                               *
//**************************************************************************************************
void handlebyte(uint8_t b, bool force) {
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
        dbg.print("Sound" , "First chunk:");                 // Header for printout of first chunk
        for (i = 0; i < 32; i += 8)              // Print 4 lines
        {
          dbg.print("Sound", "%02X %02X %02X %02X %02X %02X %02X %02X",
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
