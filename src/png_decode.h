#ifndef NODE_PNG_ENCODE_SRC_PNG_DECODE_H
#define NODE_PNG_ENCODE_SRC_PNG_DECODE_H

#include <node.h>

using namespace v8;
using namespace node;

namespace node_png_encode {
  Handle<Value> Decode(const Arguments& args);
}

#endif
