#include "milo.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h>  /* NULL, strtod() */

#define EXPECT(c, ch)      do { assert(*c->json == (ch)); c->json++;} while(0)
#define ISDIGHT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char *json;
} milo_context;

static void milo_parse_whitespace(milo_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int milo_parse_literal(milo_context* c, milo_value* v, const char* literal, milo_type type) {
    size_t  i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++) {
        if (c->json[i] != literal[i + 1]) {
            return MILO_PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = type;
    return MILO_PARSE_OK;
}

static int milo_parse_number(milo_context *c, milo_value *v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return MILO_PARSE_INVALID_VALUE;
        for(p++;ISDIGHT(*p);p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGHT(*p)) return MILO_PARSE_INVALID_VALUE;
        for(++p;ISDIGHT(*p);p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p =='-') p++;
        if (!ISDIGHT(*p)) return MILO_PARSE_INVALID_VALUE;
        for(++p;ISDIGHT(*p);p++);
    }
    errno = 0;
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
        return MILO_PARSE_NUMBER_TOO_BIG;
    v->type = MILO_NUMBER;
    c->json = p;
    return MILO_PARSE_OK;
}

static int milo_parse_value(milo_context *c, milo_value *v) {
    switch (*c->json) {
        case 'f':
            return milo_parse_literal(c, v, "false", MILO_FALSE);
        case 't':
            return milo_parse_literal(c, v, "true", MILO_TRUE);
        case 'n':
            return milo_parse_literal(c, v, "null", MILO_NULL);
        case '\0':
            return MILO_PARSE_EXPECT_VALUE;
        default:
            return milo_parse_number(c, v);
    }
}

int milo_parse(milo_value *v, const char *json) {
    milo_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = MILO_NULL;
    milo_parse_whitespace(&c);
    if ((ret = milo_parse_value(&c, v)) == MILO_PARSE_OK) {
        milo_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = MILO_NULL;
            ret = MILO_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

milo_type milo_get_type(const milo_value *v) {
    assert(v != NULL);
    return v->type;
}

double milo_get_number(const milo_value* v) {
    assert(v != NULL && v->type == MILO_NUMBER);
    return v->n;
}