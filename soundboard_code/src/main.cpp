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

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include "Configuration.h"
#include "Vs1053Esp32.h"
#include "StatusLed.h"
#include "HttpServer.h"




StatusLed::ledConfig  LED_SPEED_NORMAL = {2, 300, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_CONNECTED = {100, 2000, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_AP_MODE = {2, 10, 100};
StatusLed::ledConfig  LED_SPEED_WIFI_CONNECTING = {100, 500, 100};


// wifi settings
bool WIFI_AP_MODE = false;


int wifiConnectionCount = 0;
int wifiConnectionMaxCount = 30;




// global vars
File             mp3file;                               // File containing mp3 on SPIFFS

// Data mode the soundboard can currently have
enum datamode_t {DATA = 1,        // State for datastream
                 STOPREQD = 2,  // Request for stopping current song
                 STOPPED = 4    // State for stopped
                };

datamode_t       datamode;                                // State of datastream


// new queue handling of data taken from edzelf esp32 radio
#define QSIZ 400
QueueHandle_t     dataqueue ;                            // Queue for mp3 datastream
enum qdata_type { QDATA, QSTARTSONG, QSTOPSONG } ;    // datatyp in qdata_struct
struct qdata_struct
{
  int datatyp ;                                       // Identifier
  __attribute__((aligned(4))) uint8_t buf[32] ;       // Buffer for chunk
};
qdata_struct      outchunk;                             // Data to queue
qdata_struct      inchunk;                              // Data from queue
uint32_t          mp3filelength ;                        // File length (size)
uint8_t           tmpbuff[6000] ;                        // Input buffer for mp3 or data stream 
uint8_t*           outqp = outchunk.buf ;                 // Pointer to buffer in outchunk




bool             filereq = false;                         // Request for new file to play TODO: can filereq and filetoplay be one ?
String           fileToPlay;                              // the file to play

uint8_t          volume = 100;                             // the volume of the vs1053

bool             wifiTurnedOn = false;
bool             turnWifiOn = false;
bool             wifiTurningOn = false;
unsigned long    lastWifiCheck = 0;

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
  {4, false, "6"},  // Pig
  {0, false, "3"}, // Cat
  {2, false, "4"}, // Horse

  {13, false, "7"}, // Cow
  {12, false, "5"}, // Chicken
  {14, false, "8"}, // Duck


  {32, false, "10"}, // Blue Square
  {33, false, "11"}, // Purple Square
  {25, false, "9"}, // Bell
  {26, false, "12"}, // Red Square
  {3, false, "2"}, // Dog
  {17, false, "1"} // Sheep
};


// what time is the debounce delay ?
unsigned long lastButtonCheck = 0;
// what is the debounce delay
unsigned int debounceDelay = 50;

// the soundboard
Vs1053Esp32 vs1053player(VS1053_CS, VS1053_DCS, VS1053_DREQ);


// the status led handler
StatusLed statusLed(STATUS_LED_PIN);

// ### Task stuff ###
TaskHandle_t Sound_Task;

HttpServer *httpServer;


//**************************************************************************************************
//                                   H A N D L E B Y T E                                           *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// This byte will be send to the VS1053 most of the time.                                          *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.           *
// Set force to true if chunkbuffer must be flushed.                                               *
//**************************************************************************************************
void handlebyte(uint8_t b, bool force) {  

  // Handle next byte of MP3/Ogg data
  if (datamode == DATA)  {

    *outqp++ = b;
     // Buffer full?
    if ( outqp == ( outchunk.buf + sizeof(outchunk.buf))) {
      // Send data to playtask queue.  If the buffer cannot be placed within 200 ticks,
      // the queue is full, while the sender tries to send more.  The chunk will be dis-
      // carded it that case.
      
      // Send to queue
      xQueueSend(dataqueue, &outchunk, 200);
      // Item empty now
      outqp = outchunk.buf;                           
    }    
    }
    
  }


//**************************************************************************************************
//                                   H A N D L E B Y T E _ C H                                     *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// Chunked transfer encoding aware. Chunk extensions are not supported.                            *
//**************************************************************************************************
void handlebyte_ch(uint8_t b, bool force) {  
  handlebyte(b, force);                         // Normal handling of this byte
}






//**************************************************************************************************
//                                           SOUND TASK                                            *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void soundTaskCode(void * parameter ) {
 for(;;) {

  if ( xQueueReceive ( dataqueue, &inchunk, 5 ) )
    {
      while ( !vs1053player.data_request() )                       // If FIFO is full..
      {
        vTaskDelay ( 1 ) ;                                          // Yes, take a break
      }
      switch ( inchunk.datatyp )                                    // What kind of chunk?
      {
        case QDATA:
          vs1053player.playChunk( inchunk.buf,                    // DATA, send to player
                                    sizeof(inchunk.buf));          
          break ;
        case QSTARTSONG:
          vs1053player.startSong() ;                               // START, start player
          break ;
        case QSTOPSONG:
          //vs1053player.setVolume ( 0 ) ;                           // Mute
          vs1053player.stopSong() ;                                // STOP, stop player                    
          break ;
        default:
          break ;
      }
    }
 }
}



//**************************************************************************************************
//                              INIT THE SOUND BUTTONS                                             *
//**************************************************************************************************
void initSoundButtons() {
  // init sound button pins
  ESP_LOGI("Button", "Initializing: Buttons");
  for (int i = 0 ; i < buttonNr; i++ ) {
    int8_t  buttonPin = soundPins[i].gpio;
    ESP_LOGI("Button", "Initializing Button at pin: %d", buttonPin);
    pinMode(buttonPin, INPUT_PULLUP);
    soundPins[i].curr = digitalRead(buttonPin);
    ESP_LOGD("Button", "Button at pin: %d is in state %d", buttonPin, soundPins[i].curr);
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
//                                          START WIFI                                             *
//**************************************************************************************************
// Starts the esp32 ap mode                                                                        *
//**************************************************************************************************
void startWifi() {

  if (!turnWifiOn && wifiTurnedOn) {
    ESP_LOGI("Wifi", "Turning off wifi");
    wifiTurnedOn = false;
    WiFi.enableAP(false);
    WiFi.enableSTA(false);

    // reset wifi connecting
    wifiTurningOn = false;
    lastWifiCheck = 0;

    statusLed.setNewCfg(LED_SPEED_NORMAL);
    return;
  }

  if (wifiTurningOn == true) {
    // do nothing when the lastWifiCheck was not long enough ago
    if (millis() - lastWifiCheck < 500) {
      return;
    }

    lastWifiCheck = millis();

    ESP_LOGI("Wifi", "Waiting for wifi connection: %d of %d", wifiConnectionCount, wifiConnectionMaxCount);

    if (WiFi.status() == WL_CONNECTED) {
      statusLed.setNewCfg(LED_SPEED_WIFI_CONNECTED);
      wifiTurnedOn = true;
      ESP_LOGI("Wifi", "Ip address of esp is %s", WiFi.localIP().toString().c_str());
      // start http server
      httpServer->initHttpServer();
      wifiTurningOn = false;
      lastWifiCheck = 0;
    } else {
      wifiConnectionCount++;
      if (wifiConnectionMaxCount == wifiConnectionCount) {
        ESP_LOGI("Wifi", "Could not connect to wifi turning ap mode on");
        WIFI_AP_MODE = true;
        wifiTurningOn = false;
        lastWifiCheck = 0;
      }
    }
    return;
  }

  if (!wifiTurnedOn && turnWifiOn) {

    ESP_LOGI("Wifi", "Turning on wifi");

    if (WIFI_AP_MODE) {
      ESP_LOGI("Wifi", "Trying to setup AP with name: %s and password: %s.", WIFI_AP_SSID, WIFI_AP_PASS);
      WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);                        // This ESP will be an AP
      ESP_LOGI("Wifi", "AP IP = 192.168.4.1");             // Address for AP
      delay(1000);
      wifiTurnedOn = true;
      statusLed.setNewCfg(LED_SPEED_WIFI_AP_MODE);
      // start http server
      httpServer->initHttpServer();
    } else {
      ESP_LOGI("Wifi", "Trying to setup wifi with ssid: %s and password: %s.", WIFI_SSID, WIFI_PASS);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      statusLed.setNewCfg(LED_SPEED_WIFI_CONNECTING);
      wifiTurningOn = true;
    }
  }
}





/**
    Opens a local file from the given fs
*/
bool openLocalFile(const char * path) {

  ESP_LOGD("MP3", "Opening file %s", path);

  if (SPIFFS.exists(path) == false) {
    ESP_LOGE("MP3", "Error opening file %s", path);
    return false;
  }

  // Open the file
  mp3file = SPIFFS.open(path, FILE_READ);                           

  // Read the length of the file
  mp3filelength = mp3file.available() ;                         


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

  bool oneButtonPressed = false;
  String soundToPlay = "";


  int8_t buttonPin;
  for (int i = 0 ; i < buttonNr ; i++ ) {

    buttonPin = soundPins[i].gpio;
    // get the current state of the button
    bool buttonIsHigh = (digitalRead(buttonPin) == HIGH);

    // Change seen?
    if (buttonIsHigh != soundPins[i].curr) {

      // And the new level
      soundPins[i].curr = buttonIsHigh;

      // Button is low well than it is pushed
      if (buttonIsHigh == false) {

        if(oneButtonPressed) {
          turnWifiOn = !turnWifiOn;
          ESP_LOGD("Button", "GPIO_%02d is now LOW switching wifi to: %d", buttonPin, turnWifiOn);
          return;
        }

        ESP_LOGD("Button", "GPIO_%02d is now LOW playing sound: %s", buttonPin, soundPins[i].sound.c_str());
        soundToPlay = soundPins[i].sound;
        oneButtonPressed = true;
      }      
    } // level of the button changed

    if (soundToPlay != "") {
      initStartSound(soundToPlay);
    }
  }
}

//**************************************************************************************************
//                                      Q U E U E F U N C                                          *
//**************************************************************************************************
// Queue a special function for the play task.                                                     *
//**************************************************************************************************
void queuefunc (int func) {
  // Special function to queue
  qdata_struct     specchunk ;                          

  // Put function in datatyp
  specchunk.datatyp = func;                            

  // Send to queue
  xQueueSend (dataqueue,&specchunk, 200) ;           
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

  uint32_t        av = 0;                               // Available in stream
  uint32_t        maxchunk;                             // Max number of bytes to read
  uint32_t        qspace;                               // Free space in data queue
  int             res = 0;                              // Result reading from mp3 stream

  // Try to keep the ringbuffer filled up by adding as much bytes as possible
  // Test op playing
  if (datamode & (DATA)) {
    
    // Reduce byte count for this mp3loop()
    maxchunk = sizeof(tmpbuff) ;                         

    // Compute free space in data queue
    qspace = uxQueueSpacesAvailable( dataqueue ) * sizeof(qdata_struct);

    // Bytes left in file 
    av = mp3filelength ; 
    // Reduce byte count for this mp3loop()                              
    if (av < maxchunk) {
      maxchunk = av ;
    }
    // Enough space in queue?
    if(maxchunk > qspace ) {
      // No, limit to free queue space
      maxchunk = qspace;                              
    }

    // Anything to read?
    if ( maxchunk ) {
      // Read a block of data
      res = mp3file.read ( tmpbuff, maxchunk ) ;       
      // Number of bytes left
      mp3filelength -= res ;                           
    }
     
    for ( int i = 0 ; i < res ; i++ ) {
      // Handle one byte 
      handlebyte(tmpbuff[i], false) ;                     
    }
  }

  // STOP requested?
  if (datamode == STOPREQD) {
    ESP_LOGD("Sound", "STOP requested");

    mp3file.close();

    // Reset datacount 
    //datacount = 0 ;                                      
    // and pointer
    outqp = outchunk.buf;              
    // Queue a request to stop the song
    queuefunc(QSTOPSONG);                            
    // Yes, state becomes STOPPED
    datamode = STOPPED;                               
  }

  // Test op playing
  if (datamode & (DATA))  {
    if (av == 0) {        // End of mp3 data?
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
//                                           S E T U P                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.println();

  
  ESP_LOGI("Main", "Starting ESP32-soundboard Version %s...  Free memory %d", VERSION, ESP.getFreeHeap());

  // Init VSPI bus with default or modified pins
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  // start spiffs
  if (!SPIFFS.begin(true)) {
    ESP_LOGE("Main", "SPIFFS Mount Failed");
  }

  statusLed.setNewCfg(LED_SPEED_NORMAL);
  initSoundButtons();

  httpServer = new HttpServer();


  // Initialize VS1053 player
  vs1053player.begin();

  delay(10);

  wifiTurnedOn = false;
  turnWifiOn = false;

  


  // init the data queue
  dataqueue = xQueueCreate (QSIZ, sizeof(qdata_struct));
  

  // pin sound task to cpu 0
  xTaskCreatePinnedToCore(
    &soundTaskCode,
    "soundTask",
    1600,
    NULL,
    2,
    &Sound_Task,
    0);
}

//**************************************************************************************************
//                                           MAIN LOOP                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void loop() {
  
  vs1053player.setVolume(volume);
  buttonLoop();
  mp3loop();
  statusLed.callInloop();
  startWifi();
  httpServer->httpServerLoop();
}
