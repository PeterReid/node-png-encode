#ifndef NODE_PNG_ENCODE_SRC_DATABASE_H
#define NODE_PNG_ENCODE_SRC_DATABASE_H

#include <node.h>

using namespace v8;
using namespace node;

namespace node_png_encode {
  Handle<Value> Encode(const Arguments& args);
}

#endif
