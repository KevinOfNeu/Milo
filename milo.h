#ifndef  MILOJSON_H_
#define  MILOJSON_H_

#include <stddef.h> /* size_t */

typedef enum {
    MILO_NULL,
    MILO_FALSE,
    MILO_TRUE,
    MILO_NUMBER,
    MILO_STRING,
    MILO_ARRAY,
    MILO_OBJECT
} milo_type;


typedef struct {
    union {
        struct { char* s; size_t len; }s;  /* string: null-terminated string, string length */
        double n;                          /* number */
    }u;
    milo_type type;
} milo_value;

enum {
    MILO_PARSE_OK = 0,
    MILO_PARSE_EXPECT_VALUE,
    MILO_PARSE_INVALID_VALUE,
    MILO_PARSE_ROOT_NOT_SINGULAR,
    MILO_PARSE_NUMBER_TOO_BIG,
    MILO_PARSE_MISS_QUOTATION_MARK,
    MILO_PARSE_INVALID_STRING_ESCAPE,
    MILO_PARSE_INVALID_STRING_CHAR,
    MILO_PARSE_INVALID_UNICODE_HEX,
    MILO_PARSE_INVALID_UNICODE_SURROGATE
};

#define milo_init(v) do { (v)->type = MILO_NULL; } while(0)

int milo_parse(milo_value *value, const char *json);

void milo_free(milo_value* v);

milo_type milo_get_type(const milo_value *v);

#define milo_set_null(v) milo_free(v)

int milo_get_boolean(const milo_value* v);
void milo_set_boolean(milo_value* v, int b);

double milo_get_number(const milo_value *v);
void milo_set_number(milo_value* v, double n);

const char* milo_get_string(const milo_value* v);
size_t milo_get_string_length(const milo_value* v);
void milo_set_string(milo_value* v, const char* s, size_t len);

#endif /*MILOJSON_H_*/