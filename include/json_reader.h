#ifndef JSON_READER_H
#define JSON_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum key length for JSON parsing */
#define JSON_READER_MAX_KEY 64

/* Maximum string value length */
#define JSON_READER_MAX_STRING 256

/* Token types */
typedef enum {
  JSON_TOK_NONE = 0,
  JSON_TOK_OBJECT_START,
  JSON_TOK_OBJECT_END,
  JSON_TOK_ARRAY_START,
  JSON_TOK_ARRAY_END,
  JSON_TOK_KEY,
  JSON_TOK_STRING,
  JSON_TOK_NUMBER,
  JSON_TOK_BOOL,
  JSON_TOK_NULL,
  JSON_TOK_EOF,
  JSON_TOK_ERROR,
} json_token_type_t;

/* Reader state */
typedef struct {
  const char *data;
  size_t len;
  size_t pos;

  /* Current token */
  json_token_type_t token;
  char key[JSON_READER_MAX_KEY];
  char str_val[JSON_READER_MAX_STRING];
  double num_val;
  bool bool_val;

  /* Nesting depth tracking */
  int depth;
  bool in_array;
} json_reader_t;

/* Initialize reader with JSON data */
void json_reader_init(json_reader_t *r, const char *data, size_t len);

/* Advance to next token */
json_token_type_t json_reader_next(json_reader_t *r);

/* Skip current value (object, array, or primitive) */
bool json_reader_skip(json_reader_t *r);

/* Convenience: get current string value (copies to buffer) */
const char *json_reader_get_string(const json_reader_t *r);

/* Convenience: get current number as int64 */
int64_t json_reader_get_int(const json_reader_t *r);

/* Convenience: get current number as uint64 */
uint64_t json_reader_get_uint(const json_reader_t *r);

/* Convenience: get current number as double */
double json_reader_get_double(const json_reader_t *r);

/* Convenience: get current bool */
bool json_reader_get_bool(const json_reader_t *r);

/* Check if current key matches */
bool json_reader_key_equals(const json_reader_t *r, const char *key);

#endif
