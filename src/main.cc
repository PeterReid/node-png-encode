#include "png_encode.h"
#include "png_decode.h"


namespace {
  void RegisterModule(v8::Handle<Object> target) {
    NODE_SET_METHOD(target, "encode", node_png_encode::Encode);
    NODE_SET_METHOD(target, "decode", node_png_encode::Decode);
  }
}

NODE_MODULE(node_png_encode, RegisterModule);
