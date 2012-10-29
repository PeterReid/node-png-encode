#include <node.h>
#include <node_buffer.h>
#include <png.h>

#include "macros.h"
#include "png_decode.h"
#include "byte_builder.h"
#include "baton.h"

using namespace node_png_encode;

static const int BYTES_PER_PIXEL = 4;


struct DecodeBaton : Baton {
    Persistent<Object> png_byte_buffer;
    png_byte *png_bytes;
    size_t png_byte_count;
    
    png_byte *pixel_bytes;
    size_t width;
    size_t height;
    
    DecodeBaton(Handle<Function> cb_, Handle<Object> buffer_) : Baton(cb_) {
        png_byte_buffer = Persistent<Object>::New(buffer_);
        png_bytes = (png_byte *)Buffer::Data(png_byte_buffer);
        png_byte_count = Buffer::Length(png_byte_buffer);
        
        pixel_bytes = NULL;
        width = 0;
        height = 0;
    }
    
    ~DecodeBaton() {
        png_byte_buffer.Dispose();
        delete[] pixel_bytes;
    }
    
    static void Work(uv_work_t *req) {
        DecodeBaton *baton = static_cast<DecodeBaton *>(req->data);
        printf("Decode is working!\n");
        //int write_result = write_png(baton->pixel_bytes, baton->width, baton->height, &baton->png_bytes);
        //printf("write_result = %d\n", write_result);
        baton->width = 1;
        baton->height = 1;
        baton->pixel_bytes = new png_byte[5];
        ((int *)baton->pixel_bytes)[0] = 0xff00ffff;
    }
    
    static void After(uv_work_t *req) {
        HandleScope scope;
        DecodeBaton *baton = static_cast<DecodeBaton *>(req->data);
        
        printf("After!\n");
        
        size_t pixel_byte_count = baton->width * baton->height * 4;
        Buffer *buffer = Buffer::New(pixel_byte_count);
        memcpy(Buffer::Data(buffer), baton->pixel_bytes, pixel_byte_count);
        
        Local<Value> argv[2];
        argv[0] = Local<Value>::New(Null());
        argv[1] = Local<Object>::New(buffer->handle_);
        
        if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
            TRY_CATCH_CALL(baton->context, baton->callback, 2, argv);
            printf("After teh try-catch\n");
        }
        
        delete baton;
    }
};

Handle<Value> node_png_encode::Decode(const Arguments& args) {
    HandleScope scope;

    REQUIRE_ARGUMENT_BUFFER(0, buffer);
    REQUIRE_ARGUMENT_FUNCTION(1, callback);

    DecodeBaton *baton = new DecodeBaton(callback, buffer);
    baton->context = Persistent<Object>::New(args.This());
    
    uv_queue_work(uv_default_loop(),
        &baton->request, &baton->Work, &baton->After);
    
    return scope.Close(String::New("back from decode!"));
}
