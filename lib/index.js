var raw = require('../build/Release/node_png_encode.node');

function Png (buffer, width, height) {
  this.buffer = buffer;
  this.width = width;
  this.height = height;
}

Png.load = function(buffer, cb) {
  return raw.decode(buffer, function(err, width, height, buffer) {
    if(err) return cb(err);
    
    return cb(null, new Png(buffer, width, height));
  });
};

Png.prototype.copy = function(dest, xOrOptions, y) {
  if (typeof xOrOptions === 'number') {
    return raw.blitTransparently(dest.buffer, dest.width, dest.height, 
             xOrOptions,y,
             this.buffer, this.width, this.height,
             0,0,
             this.width, this.height);
  } else throw new Error('not implemented')
};

Png.prototype.encode = function(cb) {
  return raw.encode(this.buffer, this.width, this.height, cb);
}

exports = module.exports = Png;
