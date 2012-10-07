#include <node.h>
#include <node_buffer.h>
#include <png.h>

#include "macros.h"
#include "png_encode.h"
#include "byte_builder.h"

using namespace node_png_encode;

static const int BYTES_PER_PIXEL = 4;

void vector_write_fn(png_structp png_ptr, png_bytep data, png_size_t size) {
	ByteBuilder *buffer = (ByteBuilder *)png_get_io_ptr(png_ptr);
	buffer->append((uint8_t *)data, (size_t)size);
}
void vector_flush_fn(png_structp png_ptr) {
}


int write_png(const png_byte *data, size_t width, size_t height, ByteBuilder *buffer) {
  /* Initialize the write struct. */
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    return -1;
  }

  /* Initialize the info struct. */
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_write_struct(&png_ptr, NULL);
    return -1;
  }

  /* Set up error handling. */
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }

  /*const int bit_depth = 4;
  png_color radar_colors[RADAR_COLOR_COUNT] =
              { {0,0,0},
                {4,233,231}, // teal
                {1, 159, 244}, // light blue
                {3, 0, 244}, // blue
                {2, 253, 2}, // light green
                {1, 197, 1}, // green
                {0, 142, 0}, // dark green
                {253, 248, 2}, // yellow
                {229, 188, 0}, // dark yellow
                {253,139,0}, // orange
                {212,0,0}, // light red
                {200,0,0}, // medium red?
                {188,0,0}, // dark red
                {248,0,253}, // purple
                {152,84,198} // grayish-purple
           };

  png_set_PLTE(png_ptr, info_ptr, radar_colors, RADAR_COLOR_COUNT );
  png_byte trans[] = {0};
    
  png_set_tRNS(png_ptr, info_ptr, trans, 1, 0);
  */
  /* Set image attributes. */
  png_set_IHDR(png_ptr,
               info_ptr,
               (png_uint_32)width,
               (png_uint_32)height,
               8,
               PNG_COLOR_TYPE_RGB_ALPHA,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  
  /* Initialize rows of PNG. */
  png_byte **row_pointers = new png_byte *[height];
  png_byte *row_pointer = const_cast<png_byte *>(data);
  png_uint_32 bytes_per_row = width * BYTES_PER_PIXEL;
  
  for (size_t y = 0; y < height; ++y) {
    row_pointers[y] = row_pointer;
    row_pointer += bytes_per_row;
  }

  /* Actually write the image data. */
	png_set_write_fn(png_ptr, (png_voidp)buffer, vector_write_fn, vector_flush_fn);
  png_set_rows(png_ptr, info_ptr, row_pointers);
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    
  delete[] row_pointers;

  /* Finish writing. */
  png_destroy_write_struct(&png_ptr, &info_ptr);

	return 0;
}

struct Baton {
    uv_work_t request;
    Persistent<Function> callback;
    Persistent<Object> context;
    
    Baton(Handle<Function> cb_) {
        request.data = this;
        callback = Persistent<Function>::New(cb_);
    }
    virtual ~Baton() {
        callback.Dispose();
        context.Dispose();
    }
};

struct EncodeBaton : Baton {
    Persistent<Object> buffer;
    ByteBuilder png_bytes;
    png_byte *pixel_bytes;
    size_t width;
    size_t height;
    
    EncodeBaton(Handle<Function> cb_, Handle<Object> buffer_) : Baton(cb_), png_bytes(1024*50) {
        buffer = Persistent<Object>::New(buffer_);
        pixel_bytes = (png_byte *)Buffer::Data(buffer);
    }
    
    static void Work(uv_work_t *req) {
        EncodeBaton *baton = static_cast<EncodeBaton *>(req->data);
        int write_result = write_png(baton->pixel_bytes, baton->width, baton->height, &baton->png_bytes);
        printf("write_result = %d\n", write_result);
    }
    
    static void After(uv_work_t *req) {
        HandleScope scope;
        EncodeBaton *baton = static_cast<EncodeBaton *>(req->data);
        
        Buffer *buffer = Buffer::New(baton->png_bytes.length);
        baton->png_bytes.copy_to( Buffer::Data(buffer) );
        
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

static Handle<Value> Encode(const Arguments& args) {
    HandleScope scope;

    REQUIRE_ARGUMENT_BUFFER(0, buffer);
    REQUIRE_ARGUMENT_INTEGER(1, width);
    REQUIRE_ARGUMENT_INTEGER(2, height);
    REQUIRE_ARGUMENT_FUNCTION(3, callback);

    if (width <= 0 || height <= 0 || width*height*BYTES_PER_PIXEL > (int)Buffer::Length(buffer)) {
        return ThrowException(Exception::TypeError(
            String::New("Invalid width or height"))
        );
    }
    
    EncodeBaton *baton = new EncodeBaton(callback, buffer);
    baton->width = width;
    baton->height = height;
    baton->context = Persistent<Object>::New(args.This());
    
    uv_queue_work(uv_default_loop(),
        &baton->request, &baton->Work, &baton->After);
    
    return scope.Close(String::New("back from encode!"));
}


namespace {
  void RegisterModule(v8::Handle<Object> target) {
    NODE_SET_METHOD(target, "encode", Encode);
  }
}

NODE_MODULE(node_png_encode, RegisterModule);
