const {BaseClass} = require('./BaseClass');

class LocalFileHandler extends BaseClass {

  constructor() {
    super();

    this.fs = require('fs');

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

}

module.exports = new LocalFileHandler();
