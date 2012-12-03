var fs = require('fs');

var Png = require('../lib/index')

Png.load(fs.readFileSync('mixed-unlit.png'), function(err, tower) {
  if (err) return console.log(err);
  
  Png.load(fs.readFileSync('gradient.png'), function(err, background) {
  
    var redTower = new Png(tower);
    redTower.recolor(255,0,0);
    console.log(redTower.buffer);
    
    redTower.copy(background, 20, 20);
    
    background.encode(function(err, buffer) {
      fs.writeFileSync('./recoloredAndComposedTransparently.png', buffer);
    });
  });
});
