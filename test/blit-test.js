var fs = require('fs');

var png = require('../build/Release/node_png_encode')

png.decode(fs.readFileSync('mixed-unlit.png'), function(err, towerWidth, towerHeight, towerBuffer) {
  png.decode(fs.readFileSync('gradient.png'), function(err, bgWidth, bgHeight, bgBuffer) {
    console.log(arguments);
    console.log('going to blit!');
    png.blitTransparently(bgBuffer, bgWidth, bgHeight, 
             20,20,
             towerBuffer, towerWidth, towerHeight,
             0,0,
             towerWidth, towerHeight);
    png.blitTransparently(bgBuffer, bgWidth, bgHeight, 
             -5,0,
             towerBuffer, towerWidth, towerHeight,
             0,0,
             200, towerHeight);
    console.log('encoding...');
    png.encode(bgBuffer, bgWidth, bgHeight, function(err, buffer) {
      console.log(arguments);
      fs.writeFileSync('./composedTransparently.png', buffer);
    });
  })
});