const {BaseClass} = require('./BaseClass');
const {URL} = require('url');

class LocalFileHandler extends BaseClass {

  constructor() {
    super();

    this.fs = require('fs');
    this.https = require('https');
    this.request = require('request');

    this.logInfo('Starting LocalFileHandler');
    this.soundBoardFolder = `${this.config.mp3FilesFolder}/soundboards`;

    this.espUploadUrl = `http://${this.config.esp32Ip}/upload`;

    if(this.fs.existsSync(this.config.mp3FilesFolder) === false) {
      this.logInfo(`Local main sound folder: ${this.config.mp3FilesFolder} does not exists creating it.`);
      this.fs.mkdirSync(this.config.mp3FilesFolder);
    }
  }

  /**
   * Reads the current local files
   */
  readSoundBoardFiles() {

    if(this.fs.existsSync(this.soundBoardFolder) === false) {
      this.logInfo(`Local folder: ${this.soundBoardFolder} does not exists creating it.`);
      this.fs.mkdirSync(this.soundBoardFolder);
    }

    let result = [];

    this.fs.readdirSync(this.soundBoardFolder).forEach(boardFolder => {

      const dirPath = `${this.soundBoardFolder}/${boardFolder}`;

      if(this.fs.statSync(dirPath).isDirectory()) {

        let boardInfo = {
          "name": boardFolder,
          "files": []
        };

        this.fs.readdirSync(dirPath)
          .filter(file => file.endsWith('.mp3'))
          .forEach(file => {

            // remove mp3
            file = file.replace('.mp3', '');

            const numberSepIdx = file.indexOf('_', 1);

            const id = Number(file.substring(0, numberSepIdx));
            const name = file.substring(numberSepIdx + 1);

            const fileInfo = {
              "id": id,
              "name": name
            };

            boardInfo.files.push(fileInfo);
          });

        result.push(boardInfo);
      }
    });

    return result;
  }

  /**
   * Gets a stream of the local sound board file.
   * @param boardName
   * @param fileName
   * @return {"fs".ReadStream}
   */
  getLocalFile(boardName, fileName) {
    const filePath = `${this.soundBoardFolder}/${boardName}/${fileName}`;

    if(this.fs.existsSync(filePath) === false) {
      throw new Error(`File: ${filePath} does not exist.`);
    }

    return this.fs.createReadStream(filePath);
  }

  /**
   * When a current file exists remove it and download the data from the url
   * @param boardName
   * @param espBtnNr
   * @param url
   * @param callBack
   */
  writeLocalFileFromUrl(boardName, espBtnNr, url, callBack) {

    this.logInfo(`Setting new file: ${espBtnNr} in sound board: ${boardName} with url: ${url}`);

    const currentFile = this.findBoardFileByBoardAndBtnNr(boardName, espBtnNr);
    if(currentFile !== undefined) {
      const pathToDelete = `${this.soundBoardFolder}/${boardName}/${currentFile}`;
      this.logInfo(`Removing old file: ${pathToDelete}`);
      this.fs.unlinkSync(pathToDelete);
    }

    const myURL = new URL(url);

    const splittedPath = myURL.pathname.split('/');
    const mp3FileName = splittedPath[splittedPath.length - 1];

    const newFilePath = `${this.soundBoardFolder}/${boardName}/${espBtnNr}_${mp3FileName}`;

    this.logDebug(`New file: ${espBtnNr} in sound board: ${boardName} name is: ${newFilePath}`);

    const file = this.fs.createWriteStream(newFilePath);

    const instance = this;
    this.https.get(url, (response) => {
      response.pipe(file);
      file.on('finish', () => {
        instance.logDebug(`Done downloading: ${url} to file: ${newFilePath}`);
        file.close(callBack);
      });

    }).on('error', (err) => {
      const errMsg = `An error happened while downloading url: ${url} to: ${newFilePath}`;
      instance.logError(errMsg, err);
      throw new Error(errMsg);
    });
  }

  /**
   * Uploads the file to the esp
   * @param boardName
   * @param espBtnNr
   * @param callBack
   */
  uploadFileToEsp(boardName, espBtnNr, callBack) {
    this.logInfo(`User wants to upload file: ${espBtnNr} from soundboard: ${boardName} to the esp.`);

    const localFile = this.findBoardFileByBoardAndBtnNr(boardName, espBtnNr);

    if(localFile === undefined) {
      throw new Error('No file found');
    }

    this.logInfo(`Uploading file: ${localFile} to: ${this.espUploadUrl}`);

    const instance = this;

    const req = this.request.post(this.espUploadUrl, (err, resp, body) => {
      if(err) {
        const errMsg = `An error happened while uploading file: ${localFile} to esp: ${this.espUploadUrl}`;
        instance.logError(errMsg, err);
        callBack(new Error(errMsg));
      } else {
        instance.logInfo(`Successfully uploaded file: ${localFile} to: ${this.espUploadUrl}`)
        callBack();
      }
    });

    const form = req.form();
    form.append('file', this.fs.createReadStream(`${this.soundBoardFolder}/${boardName}/${localFile}`), {
      filename: `${espBtnNr}.mp3`,
      contentType: 'audio/mp3'
    });


  }

  /**
   * Tries to locate the current file for the given board name and esp btn nr.
   * @param boardName
   * @param espBtnNr
   */
  findBoardFileByBoardAndBtnNr(boardName, espBtnNr) {

    this.logInfo(`Looking for file: ${espBtnNr} in sound board: ${boardName}`);

    const boardPath = `${this.soundBoardFolder}/${boardName}`;
    const currentFile = this.fs.readdirSync(boardPath)
      .find(file => file.startsWith(`${espBtnNr}_`) && file.endsWith('.mp3'));

    if(currentFile === undefined) {
      this.logInfo('File not found');
    } else {
      this.logInfo(`Found file: ${currentFile}`);
    }

    return currentFile;
  }

}

module.exports = new LocalFileHandler();
