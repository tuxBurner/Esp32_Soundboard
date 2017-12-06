const {BaseClass} = require('./BaseClass');

class MyInstantsProvider extends BaseClass {

  constructor() {
    super();

    this.timeoutInMilliseconds = 10 * 1000;
    this.baseUrl = 'https://www.myinstants.com';
    this.baseSearchUrl = this.baseUrl + '/search/?name=';


    this.cherrio = require('cheerio');
    this.request = require('request');

    this.logInfo('Starting my instants provider');
  }

  /**
   * Search for sounds on myinstants
   * @param {String} query the search query
   * @return
   */
  search(query, wsCallBack) {


    const opts = {
      url: this.baseSearchUrl + query,
      timeout: this.timeoutInMilliseconds
    };

    const instance = this;
    const result = [];

    // call the myinstants page
    this.request(opts, function(err, res, body) {
      if(err) {
        instance.logError('An error happened while calling myinstants url: ${opts.url}', err);
        throw err;
      }

      let $ = instance.cherrio.load(body);

      $('.instant').each(function(i, elem) {
        const title = $(this).text().trim();
        const link = instance.baseUrl + $(this)
          .children('.small-button')
          .attr('onmousedown')
          .replace(/play\('/g, '')
          .replace(/'\)/g, '');
        const item = {
          title: title,
          link: link
        };

        result.push(item);
      });

      instance.logDebug('Found following items for url: ${opts.url', result);

      wsCallBack(result);

    });


  }

}

module.exports = new MyInstantsProvider();
