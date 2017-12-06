
const winston = require('winston');

winston.level =   'debug';

winston.remove(winston.transports.Console);
winston.add(winston.transports.Console, {
  colorize: true,
  prettyPrint: true,
  timestamp: true
});

module.exports = winston;
