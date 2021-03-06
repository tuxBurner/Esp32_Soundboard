/**
 * Class which handles all the stuff when interacting with the espsoundboard.
 */
class EspSoundBoard {

  /**
   * The main constructor of this class.
   */
  constructor() {
    this._init();

    this.config = {};

    this.esp32Config = {};

    // the currently selected soundboard
    this.currentSoundboard = null;

    // the current button to change
    this.currentBtnToSet = null;

    // we need this for mp3 playing
    this.mp3Player = new Mp3Player();

    //$(document).ajaxStart(this.showBlockUi).ajaxStop($.unblockUI);

  }

  /**
   * Initializes the data
   * @private
   */
  _init() {

    this._initMyInstantsAutoComplete();

    const instance = this;

    $('.modal').modal();


    $('#localUrlToDownload').on('input', () => instance.localUrlInputChanged());

    $('#localUrlPreListenBtn').on('click', () => instance.preListenToUrl());

    // read the configuration from the backend
    $.getJSON('./configuration', (data) => {
      instance.config = data;

      $('#boardSelector').empty();
      instance.config.soundBoards.forEach(soundBoard => {
        $('#boardSelector').append(`<option value="${soundBoard.name}">${soundBoard.name}</option>`);
      });
      $('#boardSelector').material_select();

      instance.currentSoundboard = instance.config.soundBoards.find(board => board.name === $('#boardSelector').val());

      // load the infos from the board
      instance._readInfoFromEsp32();
    });
  }

  /**
   * Initializes the my instant autocomplete
   * @private
   */
  _initMyInstantsAutoComplete() {

    const instance = this;

    $('#myinstants-autocomplete').materialize_autocomplete({
      limit: 20,
      getData: (value, callback) => {
        $.get(`myinstants?query=${value}`, (backendData) => {
          callback(value, backendData);
        });
      },
      onSelect(selectedItem) {
        $('#localUrlToDownload').val(selectedItem.id);
        instance.localUrlInputChanged();
        $('#myinstants-autocomplete').val('');
      }
    });
  }

  /**
   * This is called when the input for setting a local url changed.
   */
  localUrlInputChanged() {
    const currentVal = $('#localUrlToDownload').val();

    if(currentVal === '') {
      $('.localUrlBtn').addClass('disabled');
      return;
    }

    $('.localUrlBtn').removeClass('disabled');
  }

  preListenToUrl() {
    const currentVal = $('#localUrlToDownload').val();
    if(currentVal === '') {
      return;
    }

    this.mp3Player.playUrl(`prelisten?url=${currentVal}`);
  }

  /**
   * Reads the info data from the esp32
   * @private
   */
  _readInfoFromEsp32() {

    const instance = this;
    $.getJSON(`http://${instance.config.config.esp32Ip}/info`, (data) => {
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


      const rowHtml = $(`<tr></tr>`);


      rowHtml.append($(`<td><strong>${mapping.name}</strong></td>`));

      // try to check if there is a file for this on the esp32
      const fileOnEsp32 = this.esp32Config.files.find(espFile => espFile.name === '/' + mapping.espBtn + '.mp3');

      let playHtml = '';
      if(fileOnEsp32 === undefined) {
        playHtml = `No sound on Esp`;
      } else {
        playHtml = `<button class="waves-effect waves-light btn btn-small" onclick="espSoundBoard.playSoundOnEsp(${mapping.espBtn})">Play</button>`;
      }

      rowHtml.append(`<td>${playHtml}</td>`);

      rowHtml.append(`<td><button class="waves-effect waves-light btn btn-small" onclick="espSoundBoard.displayChangeLocalFileModal(${mapping.espBtn}, '${mapping.name}');">Change</button></td>`);

      const localFile = this.currentSoundboard.files.find(file => file.id === mapping.espBtn);


      let localHtml = 'No file';

      if(localFile !== undefined) {
        localHtml = `<button class="btn btn-small" onclick="espSoundBoard.mp3Player.playUrl('localFile/${this.currentSoundboard.name}/${localFile.id}_${localFile.name}.mp3');">Play</button> 
                     <button class="btn btn-small" onclick="espSoundBoard.uploadToEsp(${mapping.espBtn});">Upload</button> 
                     ${localFile.name}`;
      }


      rowHtml.append(`<td>${localHtml}</td>`);


      $('#soundButtons').append(rowHtml);
    });
  }

  /**
   * Plays the given sound on the esp board
   * @param btnNr
   */
  playSoundOnEsp(btnNr) {
    $.get(`http://${this.config.config.esp32Ip}/play/${btnNr}`);
  }

  restartEsp() {
    $.get(`http://${this.config.config.esp32Ip}/restart`);
  }


  /**
   * Calls the backend to upload the file to the esp
   * @param espBtn
   */
  uploadToEsp(espBtn) {
    const url = `uploadToEsp/${this.currentSoundboard.name}/${espBtn}`;
    const instance = this;
    this.showBlockUi();

    $.get(url, () => {

      instance.restartEsp();

      setTimeout(() => {
        $.unblockUI();
        instance._init();
      },15000);
    });
    
  }

  /**
   * Sets the current button to set and opens the modal dialog.
   * @param espBtn
   * @param btnName
   */
  displayChangeLocalFileModal(espBtn, btnName) {

    this.currentBtnToSet = {
      espBtn: espBtn,
      name: btnName
    };

    $('#localBtnNameToChange').text(btnName);
    $('#localUrlToDownload').val('');

    $('#changeLocalFileModal').modal('open');
  }

  /**
   * Sets a new local file in the current soundboard
   */
  setNewLocalFile() {

    const urlFromInput = $('#localUrlToDownload').val();
    const url = `setNewLocalFile/${this.currentSoundboard.name}/${this.currentBtnToSet.espBtn}?url=${urlFromInput}`;

    const instance = this;

    this.showBlockUi();
    $.get(url, () => {
      instance._init();
      $.unblockUI();
    });
  }

  /**
   * Show the block ui layer
   */
  showBlockUi() {
    $.blockUI({
      message: `
      <h4>Just a moment...</h4>
      <div class="progress">
        <div class="indeterminate"></div>
      </div>`
    });
  }
}