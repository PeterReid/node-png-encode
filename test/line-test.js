var fs = require('fs');

var Png = require('../lib/index')

var background = new Png(null, 200,200);

for (var i = 0; i < 360; i+= 5) {
  var angle = i / 180 * Math.PI;
  background.line(100,100,Math.round(100 + 100*Math.cos(angle)), Math.round(100 + 100*Math.sin(angle)), 0,255,0);
}
background.encode(function(err, buffer) {
  fs.writeFileSync('./gradientWithLines.png', buffer);
});