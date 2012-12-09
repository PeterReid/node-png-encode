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
};

PngDecoder::~PngDecoder() {
  completionCallback.Dispose();
};

void PngDecoder::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("PngDecoder"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("feedBuffer"),
      FunctionTemplate::New(FeedBuffer)->GetFunction());

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

Handle<Value> PngDecoder::FeedBuffer(const Arguments& args) {
  HandleScope scope;

  PngDecoder* obj = ObjectWrap::Unwrap<PngDecoder>(args.This());
  
  REQUIRE_ARGUMENT_BUFFER(0, buffer);

  return scope.Close(Undefined());//Number::New(obj->counter_));
}

}