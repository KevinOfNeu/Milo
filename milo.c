#include "milo.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

#define EXPECT(c, ch)      do { assert(*c->json == (ch)); c->json++;} while(0)

typedef struct {
    const char *json;
} milo_context;

static void milo_parse_whitespace(milo_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int milo_parse_null(milo_context *c, milo_value *v) {
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return MILO_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = MILO_NULL;
    return MILO_PARSE_OK;
}

static int milo_parse_true(milo_context *c, milo_value *v) {
    EXPECT(c, 't');
    if (c -> json[0] != 'r' || c -> json[1] != 'u' || c -> json[2] != 'e')
        return MILO_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = MILO_TRUE;
    return MILO_PARSE_OK;
}

static int milo_parse_false(milo_context *c, milo_value *v) {
    EXPECT(c, 'f');
    if (c -> json[0] != 'a' || c -> json[1] != 'l' || c -> json[2] != 's' || c -> json[3] != 'e')
        return MILO_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = MILO_FALSE;
    return MILO_PARSE_OK;
}

static int milo_parse_value(milo_context *c, milo_value *v) {
    switch (*c->json) {
        case 'f':
            return milo_parse_false(c, v);
        case 't':
            return milo_parse_true(c, v);
        case 'n':
            return milo_parse_null(c, v);
        case '\0':
            return MILO_PARSE_EXPECT_VALUE;
        default:
            return MILO_PARSE_INVALID_VALUE;
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
            ret = MILO_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

milo_type milo_get_type(const milo_value *v) {
    assert(v != NULL);
    return v->type;
}
