var request = require('request');
var fs = require('fs');

var req = request.post('http://192.168.0.116/upload', function (err, resp, body) {
  if (err) {
    console.log('Error!');
  } else {
    console.log('URL: ' + body);
  }
});
var form = req.form();
form.append('file', fs.createReadStream('./1.mp3'), {
  filename: '1.mp3',
  contentType: 'audio/mp3'
});
