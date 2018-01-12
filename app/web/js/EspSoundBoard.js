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
    $.getJSON('./configuration', function(data) {

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
    $.getJSON(`http://${instance.config.config.esp32Ip}/info`, function(data) {

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

    this.config.config.buttonsMapping.forEach(mapping => {


      const rowHtml = $(`<div class="row"></div>`);


      rowHtml.append($(`<div class="col s2"><strong>${mapping.name}</strong></div>`));

      // try to check if there is a file for this on the esp32
      const fileOnEsp32 = this.esp32Config.files.find(function(espFile) {
        return espFile.name === '/' + mapping.espBtn + '.mp3';
      });

      let playHtml = '';
      if(fileOnEsp32 === undefined) {
        playHtml = `No sound on Esp`;
      } else {
        playHtml = `<a class="waves-effect waves-light btn" onclick="espSoundBoard.playSoundOnEsp(${mapping.espBtn})">Play</a> (${fileOnEsp32.size})`;
      }

      rowHtml.append($(`<div class="col s2">${playHtml}</div>`));


      rowHtml.append($(`<div class="col s2">${playHtml}</div>`));

      
      rowHtml.append($(`<div class="col s6">${playHtml}</div>`));

      const buttonHtml = $(`<div class="collection-item"></div>`);
      buttonHtml.append(rowHtml);



      $('#soundButtons').append(buttonHtml);
    });
  }

  /**
   * Plays the given sound on the esp board
   * @param btnNr
   */
  playSoundOnEsp(btnNr) {
    $.get(`http://${this.config.config.esp32Ip}/play/${btnNr}`);
  }

}