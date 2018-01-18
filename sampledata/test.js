const args = process.argv;

if(args[2] === undefined) {
  console.error("Pass the number of mp3 to upload to the script as argument");
  return;
}


var request = require('request');
var fs = require('fs');

var req = request.post('http://192.168.0.124/upload', function (err, resp, body) {
  if (err) {
    console.log('Error!');
  } else {
    console.log('URL: ' + body);
  }
});
var form = req.form();
form.append('file', fs.createReadStream('./'+args[2]+'.mp3'), {
	filename: args[2]+'.mp3',
  contentType: 'audio/mp3'
});
