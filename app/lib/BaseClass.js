/**
 * Base class which most of the classes should extend from
 */
class BaseClass {

  constructor() {
    this._log = require('./Logger');
    this.config = require('../config.json');
  }

  /**
   * Info Logging with the name of the class as prefix
   * @param msg
   */
  logInfo(msg, metaData) {
    this.logMessage('info', msg, metaData);
  }

  /**
   * Debug Logging with the name of the class as prefix
   * @param msg
   */
  logDebug(msg, metaData) {
    this.logMessage('debug', msg, metaData);
  }

  /**
   * Error Logging with the name of the class as prefix
   * @param msg
   */
  logError(msg, metaData) {
    this.logMessage('error', msg, metaData);
  }

  /**
   * Logs a message at the given level and with the metadata
   * @param level
   * @param msg
   * @param metaData
   */
  logMessage(level, msg, metaData) {
    this._log.log(level, this.constructor.name + ': ' + msg, metaData);
  }
}

exports.BaseClass = BaseClass;