#include <node_api.h>
#include "deps/lpng1513/png.h"

struct decode_baton {
  napi_async_work work; // So we can delete when we are done
  napi_ref callback;
  napi_ref png_buffer;
  
  size_t png_byte_count;
  size_t png_byte_offset;
  png_byte *pixel_bytes;
  png_byte *png_bytes; // held valid by the persistent png_buffer reference
  
  int width;
  int height;
};

static void read_memory_fn(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
  decode_baton *baton = (decode_baton *)png_get_io_ptr(png_ptr);

  if (baton->png_byte_offset + byteCountToRead > baton->png_byte_count) {
    byteCountToRead = baton->png_byte_count - baton->png_byte_offset;
    printf("We don't have enough data in read_memory_fn!\n");
  }

  memcpy(outBytes, baton->png_bytes + baton->png_byte_offset, byteCountToRead);

  baton->png_byte_offset += byteCountToRead;
}


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

void decode_png_execute(napi_env env, void* data)
{
  decode_baton *baton = (decode_baton *)data;

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return;
  will_destroy_read_struct read_destroyer(png_ptr);

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
      return;
  }
  will_destroy_info_struct info_destroyer(png_ptr, info_ptr);

  png_set_read_fn(png_ptr, baton, read_memory_fn);
  //png_init_io(png_ptr, infile);
 // png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  baton->width = png_get_image_width(png_ptr, info_ptr);
  baton->height = png_get_image_height(png_ptr, info_ptr);
  int color_type = png_get_color_type(png_ptr, info_ptr);
  int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  //printf("Read stuff w=%d, h=%d, ct=%d, bd=%d\n", baton->width, baton->height, color_type, bit_depth);
  baton->pixel_bytes = new png_byte[baton->width*baton->height*4];

  switch(color_type) {
    case PNG_COLOR_TYPE_RGB_ALPHA: {

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

void decode_png_complete(napi_env env, napi_status status, void* data)
{
  decode_baton *baton = (decode_baton *)data;
  
  napi_value cb;
  status = napi_get_reference_value(env, baton->callback, &cb);
  if (status != napi_ok) goto out;
  
  napi_value err;
  napi_value width;
  napi_value height;
  napi_value pixel_buffer;
  
  status = napi_get_null(env, &err);
  if (status != napi_ok) goto out;

  status = napi_create_int32(env, baton->width, &width);
  if (status != napi_ok) goto out;
  
  status = napi_create_int32(env, baton->height, &height);
  if (status != napi_ok) goto out;
  
  void *buffer_data;
  size_t data_size = baton->width * baton->height * BYTES_PER_PIXEL;
  status = napi_create_buffer(env, data_size, &buffer_data, &pixel_buffer);
  if (status != napi_ok) goto out;
  
  memcpy(buffer_data, baton->pixel_bytes, data_size);
  
  napi_value args[5];
  args[0] = err;
  args[1] = width;
  args[2] = height;
  args[3] = pixel_buffer;

  napi_value result;
  napi_call_function(env, cb, cb, 4, args, &result);
  
  
out:
  delete[] baton->pixel_bytes;
  napi_delete_async_work(env, baton->work);
  napi_delete_reference(env, baton->callback);
  napi_delete_reference(env, baton->png_buffer);
  
  delete baton;
}

napi_value decode(napi_env env, napi_callback_info cbinfo) {
  napi_status status;
  napi_async_work work = nullptr;
  napi_ref callback_ref = nullptr;
  napi_ref buffer_ref = nullptr;
  
  size_t argc = 2;
  napi_value argv[2];
  napi_value cbinfo_this;
  void *cbinfo_data;
  png_byte *png_bytes;
  size_t png_byte_count;
  
  decode_baton *baton = nullptr;

  baton = new decode_baton();
  
  status = napi_get_cb_info(env, cbinfo, &argc, argv, &cbinfo_this, &cbinfo_data);
  if (status != napi_ok) goto out;
  
  napi_value buffer = argv[0];
  napi_value cb = argv[1];
  
  status = napi_get_buffer_info(env, buffer, &(void *)png_bytes, &png_byte_count);
  
  napi_value decode_description;
  status = napi_create_string_utf8(env, "png decode", NAPI_AUTO_LENGTH, &decode_description);
  if (status != napi_ok) goto out;
  
  status = napi_create_reference(env, cb, 1, &callback_ref);
  if (status != napi_ok) goto out;
  
  status = napi_create_reference(env, buffer, 1, &buffer_ref);
  if (status != napi_ok) goto out;

  status = napi_create_async_work(env, nullptr, decode_description, decode_png_execute, decode_png_complete, baton, &work); 
  if (status != napi_ok) goto out;
  
  baton->work = work;
  baton->callback = callback_ref;
  baton->png_buffer = buffer_ref;
  baton->png_bytes = png_bytes;
  baton->png_byte_count = png_byte_count;
  baton->png_byte_offset = 0;
  
  status = napi_queue_async_work(env, work);
  if (status != napi_ok) goto out;
    
  baton = nullptr;
  work = nullptr;
  callback_ref = nullptr;
  buffer_ref = nullptr;
out:

  if (callback_ref) napi_delete_reference(env, callback_ref);
  if (buffer_ref) napi_delete_reference(env, buffer_ref);
  if (work) napi_delete_async_work(env, work);
  if (baton) delete baton;
  
  return nullptr;
}
