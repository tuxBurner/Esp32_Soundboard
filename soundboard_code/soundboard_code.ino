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


// Release Notes
// 26-11-2017 Initial Code started
// 16-12-2017 Got the Stuff Working
// 10-01-2018 New Http Server Implementation

#include <SPI.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include "DebugPrint.h"
#include "Vs1053Esp32.h"
#include "StatusLed.h"

// defines and includes
#define VERSION "Mi, 10 Jan 2018"

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

StatusLed::ledConfig LED_SPEED_NORMAL = {2, 300, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_CONNECTED = {100, 2000, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_AP_MODE = {2, 10, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_CONNECTING = {100, 500, 100};

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

bool             wifiTurnedOn = false;
bool             turnWifiOn = false;

// pins for playing a mp3 via buttons
struct soundPin_struct {
  int8_t gpio;                                  // Pin number
  bool curr;                                    // Current state, true = HIGH, false = LOW
  String sound;                                 // which sound nr to play or wifi when to handle wifi stuff
};

/**
   The actual button mapping
*/
const int buttonNr = 12;
soundPin_struct soundPins[] = {
  {4, false, "1"},  // Sheep
  {0, false, "3"}, // Cat
  {2, false, "4"}, // Horse

  {13, false, "7"}, // Cow
  {12, false, "5"}, // Chicken
  {14, false, "8"}, // Duck


  {32, false, "wifi"}, // Blue Square
  {33, false, "11"}, // Purple Square
  {25, false, "9"}, // Bell
  {26, false, "12"}, // Red Square
  {3, false, "2"}, // Dog
  {17, false, "6"} // Pig
};


// what time is the debounce delay ?
unsigned long lastButtonCheck = 0;
// what is the debounce delay
unsigned int debounceDelay = 100;

// we need debug :)
DebugPrint dbg;

// the soundboard
Vs1053Esp32 vs1053player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// the http server
WiFiServer httpServer(80);

// the status led handler
StatusLed statusLed(STATUS_LED_PIN);


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

  statusLed.setNewCfg(LED_SPEED_NORMAL);
  initSoundButtons();

  // Create ring buffer
  ringbuf = (uint8_t*) malloc ( RINGBFSIZ ) ;
  // Initialize VS1053 player
  vs1053player.begin();

  delay(10);

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
  statusLed.callInloop();
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
  for (int i = 0 ; i < buttonNr; i++ ) {
    int8_t  buttonPin = soundPins[i].gpio;
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

    statusLed.setNewCfg(LED_SPEED_NORMAL);
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
        wifiConnectionCount++;
        dbg.print("Wifi", "Waiting for wifi connection .");
        if (wifiConnectionMaxCount == wifiConnectionCount) {
          dbg.print("Wifi", "Could not connect to wifi turning ap mode on");
          WIFI_AP_MODE = true;
          break;
        }
      }

      statusLed.setNewCfg(LED_SPEED_WIFI_CONNECTED);

      wifiTurnedOn = true;
      dbg.print("Wifi", "Ip address of esp is %s", WiFi.localIP().toString().c_str());
    }

    // start http server
    initHttpServer();
  }
}

/**
   Is called in the main loop and checks if we have any http client calling the server
*/

// the action a http client wants to perform
enum httpClientAction_t {
  NONE = 1,
  FAILURE = 2,
  PLAY = 3,
  INFO = 4,
  UPLOAD_INIT = 5,
  UPLOAD_BOUNDARY_INIT = 6,
  UPLOAD_BOUNDARY_FOUND = 7,
  UPLOAD_FILE_NAME_FOUND = 8,
  UPLOAD_DATA_START = 9,
  UPLOAD_DATA_END = 10,
  DOWNLOAD = 11,
  DELETE = 12,
  RESTART = 13
};

// current action of the http client
httpClientAction_t httpClientAction = NONE;
const String httpHeaderOk = "HTTP/1.1 200 Ok";
const String httpHeaderFailure = "HTTP/1.1 404 Not Found";

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

  File uplFile;

  while (client.connected()) {            // loop while the client's connected
    if (client.available()) {             // if there's bytes to read from the client,
      char c = client.read();             // read a byte, then


      // when we want to write the data write it to the file
      if (httpClientAction == UPLOAD_DATA_START) {
        uplFile.write(c);
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

        // client wants to download mp3
        if (currentLine.startsWith("GET /download/") && httpClientAction == NONE) {
          dbg.print("Http", "Client wants to download a sound from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /download/", "");

          httpClientAction = DOWNLOAD;
        }

        // client wants to delete a file
        if (currentLine.startsWith("GET /delete/") && httpClientAction == NONE) {
          dbg.print("Http", "Client wants to delete a file from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /delete/", "");

          httpClientAction = DELETE;
        }

        // client wants some info about this board
        if (currentLine.startsWith("GET /info") && httpClientAction == NONE) {
          httpClientAction = INFO;
        }

        // client wants to restart this board
        if (currentLine.startsWith("GET /restart") && httpClientAction == NONE) {
          httpClientAction = RESTART;
        }



        // client wants to upload a file
        if (currentLine.startsWith("POST /upload") && httpClientAction == NONE) { // upload initialized
          httpClientAction = UPLOAD_INIT;
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
          uplFile = httpStartUpload(getDataToHandle);
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
          //uplFile.flush();
          uplFile.close();
          httpClientAction = UPLOAD_DATA_END;
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

      if (httpClientAction == DELETE) {
        httpDeleteFile(client, getDataToHandle);
      }

      if (httpClientAction == FAILURE) {
        httpNotFound(client, getDataToHandle);
      }

      if (httpClientAction == RESTART) {
        httpRestart(client);
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

  File file = SPIFFS.open(path, FILE_READ);

  if (file.size() == 0) {
    httpNotFound(client, "File: " + path + " not found");
    return;
  }

  client.println(httpHeaderOk);
  client.println("Content-type: audio/mp3");
  client.println("Content-Length:" + file.size());
  client.println();

  while (file.available()) {
    client.write(file.read());
  }

  client.println();

  file.close();
}

/**
   Handles delete request
*/
void httpDeleteFile(WiFiClient client, String fileToDelete) {
  String path = "/" + fileToDelete;
  dbg.print("Http download", "Delete file: %s", path.c_str());


  if (SPIFFS.exists(path) == false) {
    httpNotFound(client, "File: " + path + " not found");
    return;
  }

  SPIFFS.remove(path);

  client.println(httpHeaderOk);
  client.println("Content-type: text/html");
  client.println();

  client.println("File: " + path + " deleted.");
  client.println();
}

/**
   Handles the request to play a sound
*/
void httpPlaySound(WiFiClient client, String fileToPlay) {

  String path = "/" + fileToPlay + ".mp3";
  if (SPIFFS.exists(path) == false) {
    httpNotFound(client, "File: " + path + " not found");
    return;
  }

  // let the sound board play the requested file
  initStartSound(fileToPlay);

  client.println(httpHeaderOk);
  client.println("Content-type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println("Playing sound: " + fileToPlay);
  client.println();
}

/**
   Client wants to restart the esp
*/
void httpRestart(WiFiClient client) {

  dbg.print("Main", "Client wants to restart the board");

  client.println(httpHeaderOk);
  client.println("Content-type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println("Restarting.");
  client.println();

  client.stop();

  delay(1000);

  ESP.restart();
}

/**
   Is called when the upload begins.
   Removes th old file and opens the new file for writing
*/
File httpStartUpload(String uploadedFile) {
  // write the file
  String path = "/" + uploadedFile;

  // Remove old file
  /*if (SPIFFS.exists(path) == true) {
    dbg.print("File", "Removing old file: %s", path.c_str());
    SPIFFS.remove(path);
    }*/

  SPIFFS.end();
  delay(1000);
  SPIFFS.begin(true);

  dbg.print("File", "Open file to write: %s", path.c_str());
  static File file = SPIFFS.open(path, FILE_WRITE);

  return file;
}


/**
   When the upload was a success
*/
void httpUPloadFinished(WiFiClient client, String uploadedFile) {
  dbg.print("File", "Done writing: %s", uploadedFile.c_str());

  client.println(httpHeaderOk);
  client.println("Content-type:text/html");
  client.println();

  client.print("{\"name\": \"" + uploadedFile + "\",");
  client.print("size: ");
  //client.println(uplPos);
  client.println();
}


/**
   Displays the info to the client
*/
void httpGetInfo(WiFiClient client) {
  client.println(httpHeaderOk);
  client.println("Content-type:application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println();

  client.println("{"); // main {}

  client.print("\"version\" : \"");
  client.print(VERSION);
  client.println("\",");

  client.print("\"name\" : \"");
  client.print(NAME);
  client.println("\",");

  client.print("\"freeMem\" : ");
  client.print(ESP.getFreeHeap());
  client.println(",");

  client.print("\"flashSize\" : ");
  client.print(ESP.getFlashChipSize());
  client.println(",");

  uint64_t chipid = ESP.getEfuseMac();
  client.print("\"chipId\" : \"");
  client.printf("%04X", (uint16_t)(chipid >> 32));
  client.printf("%08X", (uint32_t)chipid);
  client.println("\",");

  client.print("\"macAddress\" : \"");
  client.print(WiFi.macAddress());
  client.println("\",");


  client.println("\"files\" : ["); // files {}
  File root = SPIFFS.open("/", FILE_READ);
  File file = root.openNextFile();
  String sep = "";
  while (file) {
    client.print(sep);
    client.print("{\"name\" : \"");
    client.print(file.name());
    client.print("\",\"size\": ");
    client.print(file.size());
    client.println("}");
    file.close();

    file = root.openNextFile();
    sep = ",";
  }
  root.close();

  client.println("]"); // eo file {}

  client.println("}"); // eo main {}
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
bool openLocalFile(const char * path) {

  dbg.print("MP3", "Opening file %s", path);

  if (SPIFFS.exists(path) == false) {
    dbg.print("MP3", "Error opening file %s", path);
    return false;
  }

  mp3file = SPIFFS.open(path, FILE_READ);                           // Open the file


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
  for (int i = 0 ; i < buttonNr ; i++ ) {

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

    bool fileExists = openLocalFile(fileToPlay.c_str());
    if (fileExists == false) {
      return;
    }

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
  //static uint16_t  playlistcnt;                       // Counter to find right entry in playlist
  //static bool      firstmetabyte;                     // True if first metabyte(counter)
  //static int       LFcount;                           // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32]; // Buffer for chunk
  static int       bufcnt = 0;                        // Data in chunk
  static bool      firstchunk = true;                 // First chunk as input
  //String           lcml;                              // Lower case metaline
  //String           ct;                                // Contents type
  //static bool      ctseen = false;                    // First line of header seen or not
  //int              inx;                               // Pointer in metaline
  //int              i;                                 // Loop control

  // Handle next byte of MP3/Ogg data
  if (datamode == DATA)  {

    buf[bufcnt++] = b;                                // Save byte in chunkbuffer
    if (bufcnt == sizeof(buf) || force)            // Buffer full?
    {
      if (firstchunk)
      {
        firstchunk = false;
        dbg.print("Sound" , "First chunk:");                 // Header for printout of first chunk
        for (int i = 0; i < 32; i += 8)              // Print 4 lines
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
