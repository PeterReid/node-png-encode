var fs = require('fs');

var png = require('../build/Release/node_png_encode')
var PngDecoder = png.PngDecoder;

var decoder = new PngDecoder(function(err, width, height, buffer) {
  console.log('PngDecoder callback:',err,width,height,buffer);
});
console.log(decoder)
console.log(decoder.feedBuffer)

var wholeBuffer = fs.readFileSync('./mixed-unlit.png');
var chunks = [];
for (var i = 0; i < wholeBuffer.length; i+= 100) {
  chunks.push(wholeBuffer.slice(i, Math.min(wholeBuffer.length, i+100)));
}
console.log(chunks);

var chunkIdx = 0;
var interval = setInterval(function() {
  if (chunkIdx == chunks.length) return clearInterval(interval);
  
  console.log('feeding chunk', chunkIdx);
  console.log(decoder.feedBuffer(chunks[chunkIdx]));
  chunkIdx++;
}, 1000);