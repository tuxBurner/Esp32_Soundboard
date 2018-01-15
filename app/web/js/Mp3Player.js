/**
 * Plays a mp3 file.
 */
class Mp3Player {

  /**
   * Basic constructor setting some values
   */
  constructor() {

    // set the correct audio context in the browser
    window.AudioContext = window.AudioContext || window.webkitAudioContext;

    // the source for playing a mp3 file
    this.mp3Source = null;

    // the audio context for playing the mp3
    this.audioContext = new AudioContext();
  }

  /**
   * Plays the given url
   * @param url
   */
  playUrl(url) {

    // currently playing a mp3 file ? stop it
    if(this.mp3Source) {
      try {
        this.mp3Source.stop();
      } catch(err) {}
    }

    this.mp3Source= this.audioContext.createBufferSource();
    this.mp3Source.connect(this.audioContext.destination);

    const request = new XMLHttpRequest();
    const instance = this;

    request.open('GET', url, true);
    request.responseType = 'arraybuffer';
    request.onload = () => {
      instance.audioContext.decodeAudioData(request.response, function(buffer) {
        instance.mp3Source.buffer = buffer;
        instance.mp3Source.start(0);
      });
    };

    request.send();


  }

}