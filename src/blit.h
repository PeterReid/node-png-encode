#ifndef NODE_PNG_ENCODE_SRC_BLIT_H
#define NODE_PNG_ENCODE_SRC_BLIT_H

#include <node.h>

using namespace v8;
using namespace node;

namespace node_png_encode {
  Handle<Value> BlitTransparently(const Arguments& args);
  
  Handle<Value> Line(const Arguments &args);
  
  Handle<Value> Recolor(const Arguments &args);

  Handle<Value> Batch(const Arguments &args);
}

#endif
