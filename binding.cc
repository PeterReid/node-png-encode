#include <node_api.h>
#include "png_decode.h"

napi_value blit_transparently(napi_env env, napi_callback_info args);
napi_value recolor(napi_env env, napi_callback_info args);
napi_value line(napi_env env, napi_callback_info args);
napi_value encode(napi_env env, napi_callback_info args);
napi_value decode(napi_env env, napi_callback_info args);


napi_value Method(napi_env env, napi_callback_info args) {
  napi_value greeting;
  napi_status status;

  status = napi_create_string_utf8(env, "world", NAPI_AUTO_LENGTH, &greeting);
  if (status != napi_ok) return nullptr;
  return greeting;
}

#define CREATE_FUNCTION(NAME, IMPLEMENTATION) \
  status = napi_create_function(env, nullptr, 0, IMPLEMENTATION, nullptr, &fn); \
  if (status != napi_ok) return nullptr; \
  status = napi_set_named_property(env, exports, NAME, fn); \
  if (status != napi_ok) return nullptr;

napi_value init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, nullptr, 0, Method, nullptr, &fn);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "hello", fn);
  if (status != napi_ok) return nullptr;

  CREATE_FUNCTION("decode", decode);
  CREATE_FUNCTION("blitTransparently", blit_transparently);
  CREATE_FUNCTION("recolor", recolor);
  CREATE_FUNCTION("line", line);
  CREATE_FUNCTION("encode", encode);
  
  
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
