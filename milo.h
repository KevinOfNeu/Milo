#ifndef  MILOJSON_H_
#define  MILOJSON_H_

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
    double n;
    milo_type type;
} milo_value;

enum {
    MILO_PARSE_OK = 0,
    MILO_PARSE_EXPECT_VALUE,
    MILO_PARSE_INVALID_VALUE,
    MILO_PARSE_ROOT_NOT_SINGULAR,
    MILO_PARSE_NUMBER_TOO_BIG
};

int milo_parse(milo_value *value, const char *json);

milo_type milo_get_type(const milo_value *v);

double milo_get_number(const milo_value *v);

#endif /*MILOJSON_H_*/