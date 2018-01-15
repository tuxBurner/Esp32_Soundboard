const {BaseClass} = require('./BaseClass');
const { URL } = require('url');

class LocalFileHandler extends BaseClass {

  constructor() {
    super();

    this.fs = require('fs');
    this.https = require('https');

    this.logInfo('Starting LocalFileHandler');
    this.soundBoardFolder = `${this.config.mp3FilesFolder}/soundboards`;

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
    const mp3FileName = splittedPath[splittedPath.length-1];

    const newFilePath = `${this.soundBoardFolder}/${boardName}/${espBtnNr}_${mp3FileName}`;

    this.logDebug(`New file: ${espBtnNr} in sound board: ${boardName} name is: ${newFilePath}`);

    const file = this.fs.createWriteStream(newFilePath);

    const instance = this;
    this.https.get(url, (response) => {
      response.pipe(file);
      file.on('finish', function() {
        instance.logDebug(`Done downloading: ${url} to file: ${newFilePath}`);
        file.close(callBack);  // close() is async, call cb after close completes.
      });

    }).on('error', function(err) {
      instance.logError(`An error happened while downloading url: ${url} to: ${newFilePath}`,err);
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
