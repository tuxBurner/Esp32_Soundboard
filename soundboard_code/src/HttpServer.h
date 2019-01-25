#ifndef SNDHTTPSERVER_h
#define SNDHTTPSERVER_h

#include <WiFi.h>
#include <WiFiServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include "Configuration.h"



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


const String httpHeaderOk = "HTTP/1.1 200 Ok";
const String httpHeaderFailure = "HTTP/1.1 404 Not Found";



class HttpServer {
    public:

      HttpServer();

      void httpServerLoop();

      // the http server
      WiFiServer *wifiServer = new WiFiServer(80);

      void initHttpServer();


    private:       

      /**
       * Handles a not found request
      */
      void httpNotFound(WiFiClient client, String reason);

      /**
       * Is called when the upload begins.
       * Removes th old file and opens the new file for writing
      */
      File httpStartUpload(String uploadedFile);

      /**
      * Handles the download of the given mp3
      */
      void httpDownloadMp3(WiFiClient client, String fileToDownload);

      /**
       * Handles delete request
      */
      void httpDeleteFile(WiFiClient client, String fileToDelete);

      /**
        * Handles the request to play a sound
      */
      void httpPlaySound(WiFiClient client, String fileToPlay);

      /**
      * Client wants to restart the esp
      */
      void httpRestart(WiFiClient client);

      /**
      *  When the upload was a success
      */     
      void httpUPloadFinished(WiFiClient client, String uploadedFile);

      /**
       * Displays the info to the client
      */
      void httpGetInfo(WiFiClient client);

      httpClientAction_t httpClientAction = NONE;      

    
};

#endif