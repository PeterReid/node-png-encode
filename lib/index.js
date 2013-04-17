var raw = require('../build/Release/node_png_encode.node');

function Png (buffer, width, height) {
  if (arguments[0] && arguments[0].constructor === Png) {
    // Copy an existing png
    var source = arguments[0];
    this.width = source.width;
    this.height = source.height;
    this.buffer = new Buffer(source.buffer.length);
    source.buffer.copy(this.buffer);
    return;
  }

  if (!buffer) {
    buffer = new Buffer(width*height*4);
    buffer.fill(0);
  }
  
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

Png.prototype.line = function(x0,y0, x1,y1, r,g,b, dotted) {
  return raw.line(this.buffer, this.width, this.height, x0,y0, x1,y1, r,g,b, !!dotted);
}

Png.prototype.recolor = function(r, g, b) {
  return raw.recolor(this.buffer, r, g, b);
}

exports = module.exports = Png;
