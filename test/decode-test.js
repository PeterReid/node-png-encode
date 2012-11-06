var fs = require('fs');

var png = require('../build/Release/node_png_encode')

png.decode(fs.readFileSync('mixed-unlit.png'), function() {
  console.log(arguments);
});