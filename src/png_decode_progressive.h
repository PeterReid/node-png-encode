#ifndef NODE_PNG_ENCODE_SRC_PNG_DECODE_PROGRESSIVE_H
#define NODE_PNG_ENCODE_SRC_PNG_DECODE_PROGRESSIVE_H

#include <node.h>
#include <queue>
#include "png.h"

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
    static v8::Handle<v8::Value> Data(const v8::Arguments& args);
    static v8::Handle<v8::Value> Error(const v8::Arguments& args);
    static v8::Handle<v8::Value> End(const v8::Arguments& args);
    double counter_;

    /* Data() and the worker thread both need to touch 'buffers'. This lock keeps them from
     * walking on each other. I am not sure if a spinlock would make more sense; this will
     * only be help instantaneously, enough to push/pop the queue.
     */
    uv_mutex_t chunks_lock;

    struct DataChunk {
      Persistent<Object> buffer;
      // Extracted from the buffer, but I don't think I'm supposed to call Buffer::Data, Buffer::Length
      // from other threads.
      png_bytep bytes;
      png_uint_32 length;
    };
    std::queue<DataChunk> chunks;
  };

  /* Takes: a callback(err, width, height, buffer) for when it is done decoding
   * Returns: a function(Buffer|null) to pass data or end of file
   */
  Handle<Value> DecodeProgressive(const Arguments& args);
}

#endif
