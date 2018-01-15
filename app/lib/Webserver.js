const {BaseClass} = require('./BaseClass');

class Webserver extends BaseClass {

  constructor() {
    super();

    this.logInfo('Starting Webserver');

    const express = require('express');

    this.https = require('https');


    this.expApp = require('express')();
    this.myInstantsProvider = require('./MyInstantsProvider');
    this.localFileHandler = require('./LocalFileHandler');

    this._declareExpressUses(express);

    const instance = this;

    // start the web server
    this.expApp.listen(this.config.webserverPort, function() {
      instance.logInfo('listens on *:' + instance.config.webserverPort);
    });


    // reads the current local files
    this.expApp.get('/configuration', function(req, res) {

      const localFiles = instance.localFileHandler.readSoundBoardFiles();

      let response = {
        "config" : instance.config,
        "soundBoards" : localFiles
      };
      res.json(response);
    });

    // streams the given file to the user
    this.expApp.get('/localFile/:sndBoard/:fileName', function(req,res) {
      res.header('Content-Type', 'audio/mp3');

      const stream = instance.localFileHandler.getLocalFile(req.params.sndBoard, req.params.fileName);

      stream.pipe(res);
      //res.send(stream);
    });

    // when the user wants to search myinstants.com
    this.expApp.get('/myinstants', function(req, res) {
      //res.header('Content-Type', 'application/json; charset=utf-8');
      instance.myInstantsProvider.search(req.query.query, (myInstantsRes) => {
        res.json(myInstantsRes);
      });
    });

    // when the user set an url and wants to pre listen it
    this.expApp.get('/prelisten', function(req,res) {
      instance.logInfo(`User wants to listen to: ${req.query.url}`);
      instance.https.get(req.query.url, function(response) {
        response.pipe(res);
      });

    });

  }

  /**
   * Some express use path setup.
   * @private
   */
  _declareExpressUses(express) {
    this.expApp.use('/', express.static(__dirname + '/../web'));
    this.expApp.use('/materialize', express.static('./node_modules/materialize-css/dist'));
    this.expApp.use('/materialize-autocomplete', express.static('./node_modules/materialize-autocomplete'));
    this.expApp.use('/jquery', express.static('./node_modules/jquery/dist'));
  }

}

module.exports = new Webserver();