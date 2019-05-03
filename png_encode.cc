#include <node_api.h>
#include <stdlib.h>
#include "deps/lpng1513/png.h"
#include "byte_builder.h"
#include "macros.h"

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

struct encode_baton {
  encode_baton(): png_bytes(1024*50) {}
  napi_async_work work; // So we can delete when we are done
  napi_ref callback;
  napi_ref buffer;
  ByteBuilder png_bytes;
  png_byte *pixel_bytes;
  size_t width;
  size_t height;
  
};


void encode_png_execute(napi_env env, void* data)
{
  encode_baton *baton = (encode_baton *)data;
  int write_result = write_png(baton->pixel_bytes, baton->width, baton->height, &baton->png_bytes);
}

void encode_png_complete(napi_env env, napi_status status, void* data) {
  encode_baton *baton = (encode_baton *)(data);
  
  napi_value buffer;
  char *buffer_data = nullptr;
  status = napi_create_buffer(env, baton->png_bytes.length, &(void *)buffer_data, &buffer);
  if (status != napi_ok) goto out;
  
  napi_value cb;
  status = napi_get_reference_value(env, baton->callback, &cb);
  if (status != napi_ok) goto out;
  
  baton->png_bytes.copy_to( buffer_data );
  
  napi_value err;
  status = napi_get_null(env, &err);
  if (status != napi_ok) goto out;
  
  napi_value args[5];
  args[0] = err;
  args[1] = buffer;
  
  napi_value result;
  napi_call_function(env, cb, cb, 2, args, &result);
  
out:
  napi_delete_async_work(env, baton->work);
  napi_delete_reference(env, baton->callback);
  napi_delete_reference(env, baton->buffer);
  
  delete baton;
}



napi_value encode(napi_env env, napi_callback_info cbinfo) {
  napi_status status;
  napi_async_work work = nullptr;
  napi_ref callback_ref = nullptr;
  napi_ref buffer_ref = nullptr;
  
  napi_value argv[4];
  size_t argc = 4;
  const char *error = nullptr;
  const char *invalid_arguments_error = "invalid argument types";
  napi_value cbinfo_this;
  void *cbinfo_data;

  status = napi_get_cb_info(env, cbinfo, &argc, argv, &cbinfo_this, &cbinfo_data);
  if (status != napi_ok) goto out;
  if (argc != 4) {
    error = "Wrong number of arguments.";
    goto out;
  }
  
  REQUIRE_ARGUMENT_BUFFER(0, buffer, bufferLength);
  REQUIRE_ARGUMENT_INTEGER(1, width);
  REQUIRE_ARGUMENT_INTEGER(2, height);
  //REQUIRE_ARGUMENT_FUNCTION(3, callback);

  if (width <= 0 || height <= 0 || width*height*BYTES_PER_PIXEL > (int)bufferLength) {
    error = "Invalid width or height";
    goto out;
  }
  
  napi_value cb = argv[3];


  encode_baton *baton = new encode_baton();
  
  
  napi_value encode_description;
  status = napi_create_string_utf8(env, "png encode", NAPI_AUTO_LENGTH, &encode_description);
  if (status != napi_ok) goto out;
  
  status = napi_create_reference(env, cb, 1, &callback_ref);
  if (status != napi_ok) goto out;
  
  status = napi_create_reference(env, argv[0], 1, &buffer_ref);
  if (status != napi_ok) goto out;

  status = napi_create_async_work(env, nullptr, encode_description, encode_png_execute, encode_png_complete, baton, &work); 
  if (status != napi_ok) goto out;
  
  baton->work = work;
  baton->callback = callback_ref;
  baton->buffer = buffer_ref;
  baton->width = width;
  baton->height = height;
  baton->pixel_bytes = (png_byte *)buffer;
  
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
  
  if (error) {
    napi_throw_error(env, NULL, error);
  }
  
  return nullptr;

}