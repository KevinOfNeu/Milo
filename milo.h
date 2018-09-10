#ifndef  MILOJSON_H__
#define  MILOJSON_H__

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

typedef struct milo_value milo_value;
typedef struct milo_member milo_member;

struct milo_value {
    union {
        struct { milo_member* m; size_t  size; }o; /* object: members, member count */
        struct { milo_value* e; size_t size; }a; /* array:  elements, element count */
        struct { char* s; size_t len; }s;  /* string: null-terminated string, string length */
        double n;                          /* number */
    }u;
    milo_type type;
};

struct milo_member {
    char* k; size_t klen; /* member key string, key string length */
    milo_value v; /* member value */
};

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
    MILO_PARSE_INVALID_UNICODE_SURROGATE,
    MILO_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    MILO_PARSE_MISS_KEY,
    MILO_PARSE_MISS_COLON,
    MILO_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

#define milo_init(v) do { (v)->type = MILO_NULL; } while(0)

int milo_parse(milo_value *value, const char *json);
char* milo_stringify(const milo_value* v, size_t* length);

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

size_t milo_get_array_size(const milo_value* v);
milo_value* milo_get_array_element(const milo_value* v, size_t index);

size_t milo_get_object_size(const milo_value* v);
const char* milo_get_object_key(const milo_value* v, size_t index);
size_t milo_get_object_key_length(const milo_value* v, size_t index);
milo_value* milo_get_object_value(const milo_value* v, size_t index);

#endif /* MILOJSON_H__ */