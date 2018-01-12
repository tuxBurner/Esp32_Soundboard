/**
 * Class which handles all the stuff when interacting with the espsoundboard.
 */
class EspSoundBoard {

  /**
   * The main constructor of this class.
   */
  constructor() {
    this._init();
  }

  /**
   * Initializes the data
   * @private
   */
  _init() {
    const instance = this;
    $.getJSON('./config.json', function(data) {

      instance.config = data;

      // load the infos from the board
      instance._readInfoFromEsp32();
    });
  }

  /**
   * Reads the info data from the esp32
   * @private
   */
  _readInfoFromEsp32() {

    const instance = this;
    $.getJSON('http://' + instance.config.esp32Ip + '/info', function(data) {

      instance.esp32Config = data;
      instance._createSoundButtons();
    });
  }

  /**
   * Creates the sound buttons from the esp32config
   * @private
   */
  _createSoundButtons() {

    $('#soundButtons').empty();

    this.config.buttonsMapping.forEach(mapping => {

      const buttonHtml = $(`<li class="collection-item">${mapping.name}</li>`);

      // try to check if there is a file for this on the esp32
      const fileOnEsp32 = this.esp32Config.files.find(function(espFile) {
        return espFile.name === '/' + mapping.espBtn + '.mp3';
      });

      if(fileOnEsp32 === undefined) {

      } else {
        buttonHtml.append(`<a class="waves-effect waves-light btn" onclick="espSoundBoard.playSoundOnEsp(${mapping.espBtn})">Play</a>`);
      }

      $('#soundButtons').append(buttonHtml);
    });
  }

  /**
   * Plays the given sound on the esp board
   * @param btnNr
   */
  playSoundOnEsp(btnNr) {
    $.get(`http://${this.config.esp32Ip}/play/${btnNr}`);
  }

}