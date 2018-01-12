const {BaseClass} = require('./BaseClass');

class Webserver extends BaseClass {

  constructor() {
    super();

    this.logInfo('Starting Webserver');

    const express = require('express');

    this.expApp = require('express')();

    this.myInstantsProvider = require('./MyInstantsProvider');

    this._declareExpressUses(express);

    const instance = this;

    // start the web server
    this.expApp.listen(this.config.webserverPort, function() {
      instance.logInfo('listens on *:' + instance.config.webserverPort);
    });
    

    // when the user wants to search myinstants.com
    this.expApp.get("/myinstants", function(req, res) {
      res.header("Content-Type", "application/json; charset=utf-8");
      instance.myInstantsProvider.search(req.query.query, (myInstantsRes) => {
        res.send(JSON.stringify(myInstantsRes));
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
    this.expApp.use('/jquery', express.static('./node_modules/jquery/dist'));
    this.expApp.use('/config.json', express.static('./config.json'));

  }

}

module.exports = new Webserver();