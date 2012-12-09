#ifndef NODE_PNG_ENCODE_SRC_PNG_DECODE_PROGRESSIVE_H
#define NODE_PNG_ENCODE_SRC_PNG_DECODE_PROGRESSIVE_H

#include <node.h>

using namespace v8;
using namespace node;

namespace node_png_encode {
  class PngDecoder : public node::ObjectWrap {
   public:
    static void Init(v8::Handle<v8::Object> target);

   private:
    PngDecoder(Handle<Function> completionCallback_);
    ~PngDecoder();

    Persistent<Function> completionCallback;

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> FeedBuffer(const v8::Arguments& args);
    double counter_;
  };

  /* Takes: a callback(err, width, height, buffer) for when it is done decoding
   * Returns: a function(Buffer|null) to pass data or end of file
   */
  Handle<Value> DecodeProgressive(const Arguments& args);
}

#endif
