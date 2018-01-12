const {BaseClass} = require('./BaseClass');

class LocalFileHandler extends BaseClass {

  constructor() {
    super();

    this.fs = require('fs');

    this.logInfo('Starting LocalFileHandler');

    if(this.fs.existsSync(this.config.mp3FilesFolder) === false) {
      this.logInfo(`Local main sound folder: ${this.config.mp3FilesFolder} does not exists creating it.`);
      this.fs.mkdirSync(this.config.mp3FilesFolder);
    }
  }

  /**
   * Reads the current local files
   */
  readSoundBoardFiles() {

    let localCurrentFolder = `${this.config.mp3FilesFolder}/soundboards`;

    if(this.fs.existsSync(localCurrentFolder) === false) {
      this.logInfo(`Local folder: ${localCurrentFolder} does not exists creating it.`);
      this.fs.mkdirSync(localCurrentFolder);
    }

    let result = [];

    this.fs.readdirSync(localCurrentFolder).forEach(boardFolder => {

      const dirPath = `${localCurrentFolder}/${boardFolder}`;

      if(this.fs.statSync(dirPath).isDirectory()) {

        let boardInfo = {
          "name": boardFolder,
          "files": []
        };

        this.fs.readdirSync(dirPath)
          .filter(file => file.endsWith(".mp3"))
          .forEach(file => boardInfo.files.push(file));

        result.push(boardInfo);
      }
    });

    return result;
  }

}

module.exports = new LocalFileHandler();
