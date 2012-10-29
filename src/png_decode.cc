#include <node.h>
#include <node_buffer.h>
#include <png.h>

#include "macros.h"
#include "png_decode.h"
#include "byte_builder.h"
#include "baton.h"

using namespace node_png_encode;

static const int BYTES_PER_PIXEL = 4;

struct will_destroy_read_struct {
  png_structp ob;
  will_destroy_read_struct(png_structp ob_): ob(ob_) {}
  ~will_destroy_read_struct() { png_destroy_read_struct(&ob, NULL, NULL); }
};
struct will_destroy_info_struct {
  png_structp png;
  png_infop info;
  will_destroy_info_struct(png_structp png_, png_infop info_): png(png_), info(info_) {}
  ~will_destroy_info_struct() { png_destroy_info_struct(png, &info); }
};

struct DecodeBaton : Baton {
    Persistent<Object> png_byte_buffer;
    png_byte *png_bytes;
    size_t png_byte_count;
    size_t png_byte_offset;

    png_byte *pixel_bytes;
    size_t width;
    size_t height;

    DecodeBaton(Handle<Function> cb_, Handle<Object> buffer_) : Baton(cb_) {
        png_byte_buffer = Persistent<Object>::New(buffer_);
        png_bytes = (png_byte *)Buffer::Data(png_byte_buffer);
        png_byte_count = Buffer::Length(png_byte_buffer);
        png_byte_offset = 0;

        pixel_bytes = NULL;
        width = 0;
        height = 0;
    }

    ~DecodeBaton() {
        png_byte_buffer.Dispose();
        delete[] pixel_bytes;
    }

    static void read_memory_fn(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
        DecodeBaton *baton = (DecodeBaton *)png_get_io_ptr(png_ptr);

        if (baton->png_byte_offset + byteCountToRead > baton->png_byte_count) {
          byteCountToRead = baton->png_byte_count - baton->png_byte_offset;
          printf("We don't have enough data in read_memory_fn!\n");
        }

        memcpy(outBytes, baton->png_bytes + baton->png_byte_offset, byteCountToRead);

        baton->png_byte_offset += byteCountToRead;
    }

    static void Work(uv_work_t *req) {
        DecodeBaton *baton = static_cast<DecodeBaton *>(req->data);
        printf("Decode is working!\n");
        //int write_result = write_png(baton->pixel_bytes, baton->width, baton->height, &baton->png_bytes);
        //printf("write_result = %d\n", write_result);

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr)
          return;
        will_destroy_read_struct read_destroyer(png_ptr);

        printf("Allocated\n");

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            return;
        }
        will_destroy_info_struct info_destroyer(png_ptr, info_ptr);
        printf("Made info!\n");

        png_set_read_fn(png_ptr, baton, read_memory_fn);
        //png_init_io(png_ptr, infile);
       // png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);

        baton->width = png_get_image_width(png_ptr, info_ptr);
        baton->height = png_get_image_height(png_ptr, info_ptr);
        int color_type = png_get_color_type(png_ptr, info_ptr);
        int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
        printf("Read stuff w=%d, h=%d, ct=%d, bd=%d\n", baton->width, baton->height, color_type, bit_depth);
        baton->pixel_bytes = new png_byte[baton->width*baton->height*4];

        switch(color_type) {
          case PNG_COLOR_TYPE_RGB_ALPHA: {
            printf("It is RGBA!\n");

            const png_uint_32 bytesPerRow = png_get_rowbytes(png_ptr, info_ptr);
            if (bytesPerRow != 4*baton->width) {
              printf("Unexpected bytes per row!\n");
            }
            png_bytep rowData = baton->pixel_bytes;

            // read single row at a time
            for(size_t rowIdx = 0; rowIdx < baton->height; ++rowIdx) {
              png_read_row(png_ptr, (png_bytep)rowData, NULL);
              rowData += 4*baton->width;
            }
          } break;

          default:
            printf("Unhandled color type\n");
            baton->width = 1;
            baton->height = 1;
            baton->pixel_bytes = new png_byte[5];
            ((int *)baton->pixel_bytes)[0] = 0xff00ffff;
            break;
        }


    }

    static void After(uv_work_t *req) {
        HandleScope scope;
        DecodeBaton *baton = static_cast<DecodeBaton *>(req->data);

        printf("After!\n");

        size_t pixel_byte_count = baton->width * baton->height * 4;
        Buffer *buffer = Buffer::New(pixel_byte_count);
        memcpy(Buffer::Data(buffer), baton->pixel_bytes, pixel_byte_count);

        Local<Value> argv[4];
        argv[0] = Local<Value>::New(Null());
        argv[1] = Integer::New(baton->width);
        argv[2] = Integer::New(baton->height);
        argv[3] = Local<Object>::New(buffer->handle_);

        if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
            TRY_CATCH_CALL(baton->context, baton->callback, 4, argv);
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
