
#define REQUIRE_ARGUMENT_INTEGER(ARG_INDEX, NAME) \
  int32_t NAME; \
  status = napi_get_value_int32(env, argv[ARG_INDEX], &NAME); \
  if (status != napi_ok) { \
    error = invalid_arguments_error; \
    goto out; \
  }

#define REQUIRE_ARGUMENT_DOUBLE(ARG_INDEX, NAME) \
  double NAME; \
  status = napi_get_value_double(env, argv[ARG_INDEX], &NAME); \
  if (status != napi_ok) { \
    error = invalid_arguments_error; \
    goto out; \
  }
  
#define REQUIRE_ARGUMENT_BUFFER(ARG_INDEX, DATA, LENGTH) \
  size_t LENGTH; \
  char *DATA; \
  { \
    bool is_buffer; \
    status = napi_is_buffer(env, argv[ARG_INDEX], &is_buffer); \
    if (status != napi_ok || !is_buffer) { \
      error = invalid_arguments_error; \
      goto out; \
    } \
  } \
  status = napi_get_buffer_info(env, argv[ARG_INDEX], &(void *)DATA, &LENGTH); \
  if (status != napi_ok) { \
    error = invalid_arguments_error; \
    goto out; \
  }
