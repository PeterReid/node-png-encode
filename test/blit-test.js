var fs = require('fs');

var Png = require('../lib/index')

Png.load(fs.readFileSync('mixed-unlit.png'), function(err, tower) {
  if (err) return console.log(err);
  
  Png.load(fs.readFileSync('gradient.png'), function(err, background) {
    tower.copy(background, 20, 20);
    tower.copy(background, -5, 0);
    tower.copy(background, 20, -4);
    background.encode(function(err, buffer) {
      fs.writeFileSync('./composedTransparently.png', buffer);
    });
  });
});
