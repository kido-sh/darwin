/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_XPC_H
#define KIDO_XPC_XPC_H

#include <xpc/base.h>
#include <os/object.h>
#include <dispatch/dispatch.h>
#include <bsm/audit.h>
#include <sys/mman.h>
#include <uuid/uuid.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__BEGIN_DECLS

/*
 * Objects are reference counted. Objective-C sees them as NSObject<OS_xpc_object>,
 * C and C++ as opaque pointers.
 */
#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(xpc_object);
typedef xpc_object_t xpc_connection_t;
typedef xpc_object_t xpc_endpoint_t;
#define XPC_RETURNS_RETAINED OS_OBJECT_RETURNS_RETAINED
#else
typedef void *xpc_object_t;
typedef struct _xpc_connection_s *xpc_connection_t;
typedef struct _xpc_endpoint_s *xpc_endpoint_t;
#define XPC_RETURNS_RETAINED
#endif

#define __XPC_GLOBAL(symbol) ((OS_OBJECT_BRIDGE xpc_object_t)&(symbol))

typedef const struct _xpc_type_s *xpc_type_t;
#define XPC_TYPE(type) const struct _xpc_type_s type

__XPC_API XPC_TYPE(_xpc_type_array);
__XPC_API XPC_TYPE(_xpc_type_bool);
__XPC_API XPC_TYPE(_xpc_type_connection);
__XPC_API XPC_TYPE(_xpc_type_data);
__XPC_API XPC_TYPE(_xpc_type_date);
__XPC_API XPC_TYPE(_xpc_type_dictionary);
__XPC_API XPC_TYPE(_xpc_type_double);
__XPC_API XPC_TYPE(_xpc_type_endpoint);
__XPC_API XPC_TYPE(_xpc_type_error);
__XPC_API XPC_TYPE(_xpc_type_fd);
__XPC_API XPC_TYPE(_xpc_type_int64);
__XPC_API XPC_TYPE(_xpc_type_null);
__XPC_API XPC_TYPE(_xpc_type_shmem);
__XPC_API XPC_TYPE(_xpc_type_string);
__XPC_API XPC_TYPE(_xpc_type_uint64);
__XPC_API XPC_TYPE(_xpc_type_uuid);

#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_CONNECTION (&_xpc_type_connection)
#define XPC_TYPE_DATA (&_xpc_type_data)
#define XPC_TYPE_DATE (&_xpc_type_date)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_DOUBLE (&_xpc_type_double)
#define XPC_TYPE_ENDPOINT (&_xpc_type_endpoint)
#define XPC_TYPE_ERROR (&_xpc_type_error)
#define XPC_TYPE_FD (&_xpc_type_fd)
#define XPC_TYPE_INT64 (&_xpc_type_int64)
#define XPC_TYPE_NULL (&_xpc_type_null)
#define XPC_TYPE_SHMEM (&_xpc_type_shmem)
#define XPC_TYPE_STRING (&_xpc_type_string)
#define XPC_TYPE_UINT64 (&_xpc_type_uint64)
#define XPC_TYPE_UUID (&_xpc_type_uuid)

/* The two boolean objects are shared singletons. */
__XPC_API const struct _xpc_bool_s _xpc_bool_true;
__XPC_API const struct _xpc_bool_s _xpc_bool_false;
#define XPC_BOOL_TRUE __XPC_GLOBAL(_xpc_bool_true)
#define XPC_BOOL_FALSE __XPC_GLOBAL(_xpc_bool_false)

/* Error objects read like dictionaries; this key holds a readable reason. */
__XPC_API const char *const _xpc_error_key_description;
#define XPC_ERROR_KEY_DESCRIPTION _xpc_error_key_description

/* An array index that adds the value at the end. */
#define XPC_ARRAY_APPEND ((size_t)(-1))

#ifdef __BLOCKS__
typedef void (^xpc_handler_t)(xpc_object_t object);
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t value);
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
#endif

typedef void (*xpc_connection_handler_t)(xpc_connection_t peer);

/* Any object */
__XPC_API xpc_object_t xpc_retain(xpc_object_t object);
__XPC_API void xpc_release(xpc_object_t object);
__XPC_API xpc_type_t xpc_get_type(xpc_object_t object);
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_copy(xpc_object_t object);
__XPC_API bool xpc_equal(xpc_object_t a, xpc_object_t b);
__XPC_API size_t xpc_hash(xpc_object_t object);
/* The caller frees the returned string. */
__XPC_API char *xpc_copy_description(xpc_object_t object);

#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#define xpc_retain(object) ({ xpc_object_t __xpc_obj = (object); [__xpc_obj retain]; })
#define xpc_release(object) ({ xpc_object_t __xpc_obj = (object); [__xpc_obj release]; })
#endif

/* Scalars */
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_null_create(void);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_bool_create(bool value);
__XPC_API bool xpc_bool_get_value(xpc_object_t object);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_int64_create(int64_t value);
__XPC_API int64_t xpc_int64_get_value(xpc_object_t object);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_uint64_create(uint64_t value);
__XPC_API uint64_t xpc_uint64_get_value(xpc_object_t object);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_double_create(double value);
__XPC_API double xpc_double_get_value(xpc_object_t object);

/* Dates count nanoseconds since the Unix epoch. */
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_date_create(int64_t nanoseconds);
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_date_create_from_current(void);
__XPC_API int64_t xpc_date_get_value(xpc_object_t object);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_uuid_create(const unsigned char *uuid);
__XPC_API const uint8_t *xpc_uuid_get_bytes(xpc_object_t object);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_fd_create(int fd);
__XPC_API int xpc_fd_dup(xpc_object_t object);

/* Bytes and strings */
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_data_create(const void *bytes, size_t length);
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_data_create_with_dispatch_data(dispatch_data_t ddata);
__XPC_API size_t xpc_data_get_length(xpc_object_t object);
__XPC_API const void *xpc_data_get_bytes_ptr(xpc_object_t object);
__XPC_API size_t xpc_data_get_bytes(xpc_object_t object, void *buffer, size_t offset, size_t length);

__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_string_create(const char *string);
__XPC_API XPC_RETURNS_RETAINED __XPC_PRINTF(1, 2)
xpc_object_t xpc_string_create_with_format(const char *format, ...);
__XPC_API XPC_RETURNS_RETAINED __XPC_PRINTF(1, 0)
xpc_object_t xpc_string_create_with_format_and_arguments(const char *format, va_list args);
__XPC_API size_t xpc_string_get_length(xpc_object_t object);
__XPC_API const char *xpc_string_get_string_ptr(xpc_object_t object);

/* Arrays */
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_array_create(const xpc_object_t *values, size_t count);
__XPC_API size_t xpc_array_get_count(xpc_object_t array);
__XPC_API xpc_object_t xpc_array_get_value(xpc_object_t array, size_t index);
__XPC_API void xpc_array_set_value(xpc_object_t array, size_t index, xpc_object_t value);
__XPC_API void xpc_array_append_value(xpc_object_t array, xpc_object_t value);
#ifdef __BLOCKS__
__XPC_API bool xpc_array_apply(xpc_object_t array, xpc_array_applier_t applier);
#endif

__XPC_API bool xpc_array_get_bool(xpc_object_t array, size_t index);
__XPC_API int64_t xpc_array_get_int64(xpc_object_t array, size_t index);
__XPC_API uint64_t xpc_array_get_uint64(xpc_object_t array, size_t index);
__XPC_API double xpc_array_get_double(xpc_object_t array, size_t index);
__XPC_API int64_t xpc_array_get_date(xpc_object_t array, size_t index);
__XPC_API const void *xpc_array_get_data(xpc_object_t array, size_t index, size_t *length);
__XPC_API const char *xpc_array_get_string(xpc_object_t array, size_t index);
__XPC_API const uint8_t *xpc_array_get_uuid(xpc_object_t array, size_t index);
__XPC_API int xpc_array_dup_fd(xpc_object_t array, size_t index);
__XPC_API XPC_RETURNS_RETAINED xpc_connection_t xpc_array_create_connection(xpc_object_t array, size_t index);

__XPC_API void xpc_array_set_bool(xpc_object_t array, size_t index, bool value);
__XPC_API void xpc_array_set_int64(xpc_object_t array, size_t index, int64_t value);
__XPC_API void xpc_array_set_uint64(xpc_object_t array, size_t index, uint64_t value);
__XPC_API void xpc_array_set_double(xpc_object_t array, size_t index, double value);
__XPC_API void xpc_array_set_date(xpc_object_t array, size_t index, int64_t value);
__XPC_API void xpc_array_set_data(xpc_object_t array, size_t index, const void *bytes, size_t length);
__XPC_API void xpc_array_set_string(xpc_object_t array, size_t index, const char *string);
__XPC_API void xpc_array_set_uuid(xpc_object_t array, size_t index, const unsigned char *uuid);
__XPC_API void xpc_array_set_fd(xpc_object_t array, size_t index, int fd);
__XPC_API void xpc_array_set_connection(xpc_object_t array, size_t index, xpc_connection_t connection);

/* Dictionaries */
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_dictionary_create(const char *const *keys,
    const xpc_object_t *values, size_t count);
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_dictionary_create_reply(xpc_object_t request);
__XPC_API size_t xpc_dictionary_get_count(xpc_object_t dictionary);
__XPC_API xpc_object_t xpc_dictionary_get_value(xpc_object_t dictionary, const char *key);
__XPC_API void xpc_dictionary_set_value(xpc_object_t dictionary, const char *key, xpc_object_t value);
#ifdef __BLOCKS__
__XPC_API bool xpc_dictionary_apply(xpc_object_t dictionary, xpc_dictionary_applier_t applier);
#endif
/* The connection a received message came in on, or NULL. */
__XPC_API xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t dictionary);

__XPC_API bool xpc_dictionary_get_bool(xpc_object_t dictionary, const char *key);
__XPC_API int64_t xpc_dictionary_get_int64(xpc_object_t dictionary, const char *key);
__XPC_API uint64_t xpc_dictionary_get_uint64(xpc_object_t dictionary, const char *key);
__XPC_API double xpc_dictionary_get_double(xpc_object_t dictionary, const char *key);
__XPC_API int64_t xpc_dictionary_get_date(xpc_object_t dictionary, const char *key);
__XPC_API const void *xpc_dictionary_get_data(xpc_object_t dictionary, const char *key, size_t *length);
__XPC_API const char *xpc_dictionary_get_string(xpc_object_t dictionary, const char *key);
__XPC_API const uint8_t *xpc_dictionary_get_uuid(xpc_object_t dictionary, const char *key);
__XPC_API xpc_object_t xpc_dictionary_get_dictionary(xpc_object_t dictionary, const char *key);
__XPC_API int xpc_dictionary_dup_fd(xpc_object_t dictionary, const char *key);
__XPC_API XPC_RETURNS_RETAINED xpc_connection_t xpc_dictionary_create_connection(xpc_object_t dictionary,
    const char *key);

__XPC_API void xpc_dictionary_set_bool(xpc_object_t dictionary, const char *key, bool value);
__XPC_API void xpc_dictionary_set_int64(xpc_object_t dictionary, const char *key, int64_t value);
__XPC_API void xpc_dictionary_set_uint64(xpc_object_t dictionary, const char *key, uint64_t value);
__XPC_API void xpc_dictionary_set_double(xpc_object_t dictionary, const char *key, double value);
__XPC_API void xpc_dictionary_set_date(xpc_object_t dictionary, const char *key, int64_t value);
__XPC_API void xpc_dictionary_set_data(xpc_object_t dictionary, const char *key, const void *bytes,
    size_t length);
__XPC_API void xpc_dictionary_set_string(xpc_object_t dictionary, const char *key, const char *string);
__XPC_API void xpc_dictionary_set_uuid(xpc_object_t dictionary, const char *key, const unsigned char *uuid);
__XPC_API void xpc_dictionary_set_fd(xpc_object_t dictionary, const char *key, int fd);
__XPC_API void xpc_dictionary_set_connection(xpc_object_t dictionary, const char *key,
    xpc_connection_t connection);

/* Service runtime */
__XPC_API __XPC_NORETURN void xpc_main(xpc_connection_handler_t handler);
__XPC_API void xpc_transaction_begin(void);
__XPC_API void xpc_transaction_end(void);
#ifdef __BLOCKS__
__XPC_API void xpc_set_event_stream_handler(const char *stream, dispatch_queue_t queue,
    xpc_handler_t handler);
#endif

__END_DECLS

#include <xpc/endpoint.h>
#include <xpc/debug.h>
#ifdef __BLOCKS__
#include <xpc/connection.h>
#include <xpc/activity.h>
#endif
#include <launch.h>

#endif
