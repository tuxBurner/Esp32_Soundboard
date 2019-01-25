#include "HttpServer.h"

HttpServer::HttpServer() {   
}

void HttpServer::initHttpServer() {
  wifiServer->begin();
}

void HttpServer::httpNotFound(WiFiClient client, String reason) {
  client.println(httpHeaderFailure);
  client.println("Content-type:text/html");
  client.println();
  client.println(reason);
  client.println();    
}

File HttpServer::httpStartUpload(String uploadedFile) {
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

  ESP_LOGD("File", "Open file to write: %s", path.c_str());
  static File file = SPIFFS.open(path, FILE_WRITE);

  return file;
}

void HttpServer::httpDownloadMp3(WiFiClient client, String fileToDownload) {

  String path = "/" + fileToDownload + ".mp3";
  ESP_LOGI("Http download", "Streaming file: %s to client", path.c_str());

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

void HttpServer::httpDeleteFile(WiFiClient client, String fileToDelete) {
  String path = "/" + fileToDelete;
  ESP_LOGI("Http download", "Delete file: %s", path.c_str());


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

void HttpServer::httpPlaySound(WiFiClient client, String fileToPlay) {

  String path = "/" + fileToPlay + ".mp3";
  if (SPIFFS.exists(path) == false) {
    httpNotFound(client, "File: " + path + " not found");
    return;
  }

  // let the sound board play the requested file
  // TODO: initStartSound(fileToPlay);

  client.println(httpHeaderOk);
  client.println("Content-type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println("Playing sound: " + fileToPlay);
  client.println();
}

void HttpServer::httpRestart(WiFiClient client) {

  ESP_LOGI("Main", "Client wants to restart the board");

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

void HttpServer::httpUPloadFinished(WiFiClient client, String uploadedFile) {
  ESP_LOGD("File", "Done writing: %s", uploadedFile.c_str());

  client.println(httpHeaderOk);
  client.println("Content-type:text/html");
  client.println();

  client.print("{\"name\": \"" + uploadedFile + "\",");
  client.print("size: ");
  //client.println(uplPos);
  client.println();
}

void HttpServer::httpGetInfo(WiFiClient client) {
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

void HttpServer::httpServerLoop() {
   // do we have a new client ?
  WiFiClient client = wifiServer->available();

  // no client ?
  if (!client) {
    return;
  }

  ESP_LOGD("Http", "new client connected %s", client.remoteIP().toString().c_str());

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

          ESP_LOGD("Http", "Client wants to play a sound from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /play/", "");
          httpClientAction = PLAY;
        }

        // client wants to download mp3
        if (currentLine.startsWith("GET /download/") && httpClientAction == NONE) {
          ESP_LOGD("Http", "Client wants to download a sound from the board");

          // get rid of the HTTP
          getDataToHandle = currentLine;
          getDataToHandle.replace(" HTTP/1.1", "");
          getDataToHandle.replace("GET /download/", "");

          httpClientAction = DOWNLOAD;
        }

        // client wants to delete a file
        if (currentLine.startsWith("GET /delete/") && httpClientAction == NONE) {
          ESP_LOGD("Http", "Client wants to delete a file from the board");

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
          ESP_LOGD("Http Upload", "Found boundary: %s", uploadBoundary.c_str());
          httpClientAction = UPLOAD_BOUNDARY_INIT;

        }

        // the upload boundary actualy exists in the request
        if (currentLine.startsWith(uploadBoundary) && httpClientAction == UPLOAD_BOUNDARY_INIT) {
          ESP_LOGD("Http Upload", "Found boundary in request: %s", uploadBoundary.c_str());
          httpClientAction = UPLOAD_BOUNDARY_FOUND;
        }

        // the upload file  name has to be parsed
        if (currentLine.startsWith("Content-Disposition: form-data; name=\"file\"; filename=") && httpClientAction == UPLOAD_BOUNDARY_FOUND) {
          getDataToHandle = currentLine.substring(55, currentLine.length() - 1);
          ESP_LOGD("Http Upload", "Filename is: %s", getDataToHandle.c_str());
          uplFile = httpStartUpload(getDataToHandle);
          httpClientAction = UPLOAD_FILE_NAME_FOUND;
        }

        // after parsing the name and finding the first empty line we can start reading the data
        if (currentLine == "" && httpClientAction == UPLOAD_FILE_NAME_FOUND) {
          ESP_LOGD("Http Upload", "Starting reading the data");
          httpClientAction = UPLOAD_DATA_START;
        }


        // no more data to read
        if (currentLine.startsWith(uploadBoundary) && httpClientAction == UPLOAD_DATA_START) {
          ESP_LOGD("Http Upload", "Found boundary end in request: %s", uploadBoundary.c_str());
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
          ESP_LOGD("Http", "Client send line: %s", currentLine.c_str());
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
  ESP_LOGD("Http", "Client Disconnected.");  
}