#include "png_encode.h"
#include "png_decode.h"
#include "blit.h"

namespace {
  void RegisterModule(v8::Handle<Object> target) {
    NODE_SET_METHOD(target, "encode", node_png_encode::Encode);
    NODE_SET_METHOD(target, "decode", node_png_encode::Decode);
    NODE_SET_METHOD(target, "blitTransparently", node_png_encode::BlitTransparently);
    NODE_SET_METHOD(target, "line", node_png_encode::Line);
  }
}

NODE_MODULE(node_png_encode, RegisterModule);
