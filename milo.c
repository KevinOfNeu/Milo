#include "milo.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h> #include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef MILO_PARSE_STACK_INIT_SIZE
#define MILO_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)      do { assert(*c->json == (ch)); c->json++;} while(0)
#define ISDIGHT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)milo_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
} milo_context;

static void* milo_context_push(milo_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = MILO_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1; /* c->size * 1.5 */
        c->stack = (char*) realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* milo_context_pop(milo_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

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
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return MILO_PARSE_NUMBER_TOO_BIG;
    v->type = MILO_NUMBER;
    c->json = p;
    return MILO_PARSE_OK;
}

static int milo_parse_string(milo_context* c, milo_value* v) {
    size_t  head = c->top, len;
    const char* p;
    EXPECT(c, '\"');
    p = c-> json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                milo_set_string(v, (const char*) milo_context_pop(c, len), len);
                c->json = p;
                return MILO_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default:
                        c->top = head;
                        return MILO_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            case '\0':
                c->top = head;
                return MILO_PARSE_MISS_QUOTATION_MARK;
            default:
                if ((unsigned char)ch < 0x20) {
                    c->top = head;
                    return MILO_PARSE_INVALID_STRING_CHAR;
                }
                PUTC(c, ch);
        }
    }
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
        case '"':
            return milo_parse_string(c, v);
        default:
            return milo_parse_number(c, v);
    }
}

int milo_parse(milo_value *v, const char *json) {
    milo_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    milo_init(v);
    milo_parse_whitespace(&c);
    if ((ret = milo_parse_value(&c, v)) == MILO_PARSE_OK) {
        milo_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = MILO_NULL;
            ret = MILO_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void milo_free(milo_value* v) {
    assert( v!= NULL);
    if (v ->type == MILO_STRING)
        free(v->u.s.s);
    v->type = MILO_NULL;
}

milo_type milo_get_type(const milo_value *v) {
    assert(v != NULL);
    return v->type;
}

int milo_get_boolean(const milo_value* v) {
    assert(v != NULL && (v->type == MILO_TRUE || v->type == MILO_FALSE));
    return v->type == MILO_TRUE;
}

void milo_set_boolean(milo_value* v, int b) {
    milo_free(v);
    v->type = b ? MILO_TRUE : MILO_FALSE;
}

double milo_get_number(const milo_value* v) {
    assert(v != NULL && v->type == MILO_NUMBER);
    return v->u.n;
}

void milo_set_number(milo_value*v, double n) {
    milo_free(v);
    v->u.n = n;
    v->type = MILO_NUMBER;
}

const char* milo_get_string(const milo_value* v) {
    assert(v != NULL && v->type == MILO_STRING);
    return v->u.s.s;
}

size_t milo_get_string_length(const milo_value* v) {
    assert(v != NULL && v->type == MILO_STRING);
    return v->u.s.len;
}

void milo_set_string(milo_value* v, const char* s, size_t len) {
    assert( v!= NULL && ( s != NULL || len == 0));
    milo_free(v);
    v->u.s.s = (char*) malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = MILO_STRING;
}

