
window.AudioContext = window.AudioContext || window.webkitAudioContext;
let audioContext = new AudioContext();
let source;

function play(url) {

  const request = new XMLHttpRequest();

  if(source) {
    try {
      source.stop();
    } catch(err) {}
  }

  source = audioContext.createBufferSource();
  source.connect(audioContext.destination);
  request.open('GET', url, true);
  request.responseType = 'arraybuffer';
  request.onload = function() {
    audioContext.decodeAudioData(request.response, function(buffer) {
      source.buffer = buffer;
      source.start(0);
    });
  };

  request.send();
}

$(function() {

});