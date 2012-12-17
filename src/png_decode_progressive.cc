#include <node.h>
#include <node_buffer.h>
#include <png.h>

#include "macros.h"
#include "png_decode_progressive.h"
#include "byte_builder.h"
#include "baton.h"

using namespace node_png_encode;

namespace node_png_encode {

PngDecoder::PngDecoder(Handle<Function> completionCallback_) {
  completionCallback = Persistent<Function>::New(completionCallback_);
  uv_mutex_init(&chunks_lock);

  request.data = this;
  workerGoing = false;
};

PngDecoder::~PngDecoder() {

  // Make sure we drop all the references to buffers
  uv_mutex_lock(&chunks_lock);
  while (!chunks.empty()) {
    DataChunk chunk = chunks.front();
    chunk.buffer.Dispose();
    chunks.pop();
  }
  uv_mutex_unlock(&chunks_lock);

  completionCallback.Dispose();

  uv_mutex_destroy(&chunks_lock);
};

void PngDecoder::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("PngDecoder"));
  tpl->InstanceTemplate()->SetInternalFieldCount(3);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("data"),
      FunctionTemplate::New(Data)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("error"),
      FunctionTemplate::New(Error)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("end"),
      FunctionTemplate::New(End)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("PngDecoder"), constructor);
}

Handle<Value> PngDecoder::New(const Arguments& args) {
  HandleScope scope;
  
  REQUIRE_ARGUMENT_FUNCTION(0, completionCallback);

  PngDecoder* obj = new PngDecoder(completionCallback);
  obj->counter_ = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> PngDecoder::Data(const Arguments& args) {
  HandleScope scope;

  PngDecoder* obj = ObjectWrap::Unwrap<PngDecoder>(args.This());
  
  REQUIRE_ARGUMENT_BUFFER(0, buffer);

  DataChunk chunk = {
    Persistent<Object>::New(buffer),
    (png_bytep)Buffer::Data(buffer),
    (png_uint_32)Buffer::Length(buffer)
  };
  uv_mutex_lock(&obj->chunks_lock);
  obj->chunks.push(chunk);

  if (!obj->workerGoing) {
    // We need to have exactly one worker either decoding chunks or enqueued to be doing that.
    uv_queue_work(uv_default_loop(), &obj->request, DecodeChunksEntry, AfterDecodeChunksEntry);
  }
  uv_mutex_unlock(&obj->chunks_lock);

  return scope.Close(Undefined());//Number::New(obj->counter_));
}

Handle<Value> PngDecoder::Error(const Arguments& args) {
  HandleScope scope;

  // TODO: Abort. Callback will be called with the given error. Or maybe our own?

  return scope.Close(Undefined());//Number::New(obj->counter_));
}

Handle<Value> PngDecoder::End(const Arguments& args) {
  HandleScope scope;

  // TODO: No more data expected. If we are all done with the chunks we have been
  // given and aren't expecting any more (because End was called), we should 
  // callback with an error (unexpected end of stream).

  return scope.Close(Undefined());//Number::New(obj->counter_));
}

void PngDecoder::DecodeChunksEntry(uv_work_t *req)
{
  PngDecoder *baton = static_cast<PngDecoder *>(req->data);
  baton->DecodeChunks();
}

void PngDecoder::DecodeChunks()
{

}

void PngDecoder::AfterDecodeChunksEntry(uv_work_t *req)
{
  PngDecoder *baton = static_cast<PngDecoder *>(req->data);
  baton->AfterDecodeChunks();
}

void PngDecoder::AfterDecodeChunks()
{
  // If there are more chunks to decode, schedule those now.
}
}