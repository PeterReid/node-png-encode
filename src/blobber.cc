#include <string.h>
#include <node.h>
#include <node_buffer.h>
#include <algorithm>

#include "macros.h"
#include "blobber.h"

using namespace node_sqlite3;



#define RADAR_COLOR_COUNT 15


Persistent<FunctionTemplate> Foozle::constructor_template;

void Foozle::Init(Handle<Object> target)
{
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor_template->SetClassName(String::NewSymbol("Foozle"));
    
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "mash", Mash);
    
    target->Set(String::NewSymbol("Foozle"), 
        constructor_template->GetFunction());
        
}
    
Foozle::Foozle()
{
    pixels = NULL;
}

Foozle::~Foozle()
{
    delete[] pixels;
    printf("Freed\n");
}
    
Handle<Value> Foozle::New(const Arguments& args)
{
    HandleScope scope;

    if (!args.IsConstructCall()) {
        return ThrowException(Exception::TypeError(
            String::New("Use the new operator to create new Foozle objects"))
        );
    }

    REQUIRE_ARGUMENT_BUFFER(0, buffer);
    REQUIRE_ARGUMENT_FUNCTION(1, callback);
    
    Foozle* foozle = new Foozle();
    
    foozle->Wrap(args.This());

    args.This()->Set(String::NewSymbol("fooLevel"), String::NewSymbol("bazzle"), ReadOnly);
    //args.This()->Set(String::NewSymbol("mode"), Integer::New(mode), ReadOnly);

    // Start opening the database.
    SlurpBaton *baton = new SlurpBaton(foozle, callback, buffer);
    Work_BeginSlurp(baton);

    return args.This();
}

Handle<Value> Foozle::Mash(const Arguments& args)
{ 
    HandleScope scope;
    
    Foozle* foozle = ObjectWrap::Unwrap<Foozle>(args.This());
    
    REQUIRE_ARGUMENT_DOUBLE(0, source_left);
    REQUIRE_ARGUMENT_DOUBLE(1, source_right);
    REQUIRE_ARGUMENT_DOUBLE(2, source_top);
    REQUIRE_ARGUMENT_DOUBLE(3, source_bottom);
    REQUIRE_ARGUMENT_INTEGER(4, result_width);
    REQUIRE_ARGUMENT_INTEGER(5, result_height);
    REQUIRE_ARGUMENT_BUFFER(6, dest_buffer);
    REQUIRE_ARGUMENT_FUNCTION(7, callback);
    
    if (result_width <= 0 || result_height <= 0 || result_width*result_height*4 != Buffer::Length(dest_buffer)) {
        return ThrowException(Exception::Error( 
            String::New("Buffer length is not consistent with given width and height"))
        );  
    }
    
    
    StretchBaton *baton = new StretchBaton(foozle, callback, dest_buffer, 
        source_left, source_right, source_top, source_bottom,
        result_width, result_height);
    Work_BeginStretch(baton);
    
    return args.This();
}



void Foozle::Work_BeginSlurp(Baton* baton) {
    int status = uv_queue_work(uv_default_loop(),
        &baton->request, Work_Slurp, Work_AfterSlurp);
    assert(status == 0);
}

int Foozle::SlurpBaton::ReadMemoryGif (GifFileType *gif_file, GifByteType *buffer, int size) {
  SlurpBaton *baton = static_cast<SlurpBaton *>(gif_file->UserData);
  if (baton->spewed_length == baton->gif_length) {
    return 0;
  }

  size_t copy_length = baton->gif_length - baton->spewed_length;
  if (copy_length > (size_t)size) {
    // Trim to request
    copy_length = (size_t)size;
  }
  
  memcpy(buffer, baton->gif_buffer + baton->spewed_length, copy_length);
  baton->spewed_length += copy_length;
  return copy_length;
}

static inline int clamp(int inclusive_min, int x, int inclusive_max) {
  return x <= inclusive_min ? inclusive_min
       : x >= inclusive_max ? inclusive_max
       : x;
}

void Foozle::Work_Slurp(uv_work_t* req) {
    SlurpBaton* baton = static_cast<SlurpBaton*>(req->data);
    Foozle* foozle = baton->foozle;

    baton->status = GIF_OK;
    GifFileType *gif_file = DGifOpen(baton, SlurpBaton::ReadMemoryGif, &baton->status);
    if (baton->status != GIF_OK) {
        DGifCloseFile(gif_file);
        return;
    }
    printf("DGifOpen seems to have worked...\n");
    
    baton->status = DGifSlurp(gif_file);
    
    if (baton->status != GIF_OK) {
        DGifCloseFile(gif_file);
        return;
    }

    printf("Width = %d, height = %d\n", gif_file->SWidth, gif_file->SHeight);

    int pixel_count = gif_file->SWidth * gif_file->SHeight;
    unsigned char *filtered = new unsigned char[pixel_count];
    unsigned char *unfiltered = gif_file->SavedImages[0].RasterBits;
    const int filterThreshold = 7;
    for (int i = 0; i < pixel_count; i++) {
      filtered[i] = clamp(0, (int)unfiltered[i] - filterThreshold, RADAR_COLOR_COUNT-1);  
    }

    DGifCloseFile(gif_file);
    
    foozle->pixels = filtered;
}

void Foozle::Work_AfterSlurp(uv_work_t* req) {
    HandleScope scope;
    SlurpBaton* baton = static_cast<SlurpBaton*>(req->data);
    Foozle* foozle = baton->foozle;

    Local<Value> argv[1];
    if (baton->status != GIF_OK) {
        EXCEPTION(String::New("GIF deocode failed"), baton->status, exception);
        argv[0] = exception;
    }
    else {
        argv[0] = Local<Value>::New(Null());
    }

    if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
        TRY_CATCH_CALL(foozle->handle_, baton->callback, 1, argv);
        printf("After teh try-catch\n");
    }
    printf("hmm\n");
    
    delete baton;
}


void Foozle::Work_BeginStretch(Baton* baton) {
    int status = uv_queue_work(uv_default_loop(),
            &baton->request, Work_Stretch, Work_AfterStretch);
    assert(status == 0);
}

void Foozle::Work_Stretch(uv_work_t* req) {
    StretchBaton* baton = static_cast<StretchBaton*>(req->data);
    Foozle* foozle = baton->foozle;
    
    int count = baton->result_width * baton->result_height;
    for (int i = 0; i < count; i++) {
        baton->dest_pixels[i] = 0xff00ffff;
    }
}

void Foozle::Work_AfterStretch(uv_work_t* req) {
    HandleScope scope;
    StretchBaton *baton = static_cast<StretchBaton *>(req->data);
    Foozle *foozle = baton->foozle;

    Local<Value> argv[1];
    argv[0] = Local<Value>::New(baton->dest_buffer);
    
    if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
        TRY_CATCH_CALL(foozle->handle_, baton->callback, 1, argv);
        printf("After teh try-catch\n");
    }
    printf("hmm\n");
    
    delete baton;
}

