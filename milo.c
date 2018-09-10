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

static const char* milo_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

static void milo_encode_utf8(milo_context* c, unsigned u) {
    if (u <= 0x7F)
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int milo_parse_string_raw(milo_context* c, char** str, size_t* len) {
    size_t head = c->top;
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = milo_context_pop(c, *len);
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
                    case 'u':
                        if (!(p = milo_parse_hex4(p, &u)))
                            STRING_ERROR(MILO_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(MILO_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(MILO_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = milo_parse_hex4(p, &u2)))
                                STRING_ERROR(MILO_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(MILO_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        milo_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(MILO_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(MILO_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(MILO_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int milo_parse_string(milo_context* c, milo_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = milo_parse_string_raw(c, &s, &len)) == MILO_PARSE_OK)
        milo_set_string(v, s, len);
    return ret;
}

static int milo_parse_value(milo_context *c, milo_value *v); /* forward declaration */

static int milo_parse_array(milo_context* c, milo_value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    milo_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        v->type = MILO_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return MILO_PARSE_OK;
    }
    for(;;) {
        milo_value e;
        milo_init(&e);
        if ((ret = milo_parse_value(c, &e)) != MILO_PARSE_OK)
            break;
        memcpy(milo_context_push(c, sizeof(milo_value)), &e, sizeof(milo_value));
        size++;
        milo_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            milo_parse_whitespace(c);
        }
        else if (*c->json == ']') {
            c->json++;
            v->type = MILO_ARRAY;
            v->u.a.size = size;
            size *= sizeof(milo_value);
            memcpy(v->u.a.e = (milo_value*)malloc(size), milo_context_pop(c, size), size);
            return MILO_PARSE_OK;
        }
        else {
            ret = MILO_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* Pop and free values on the stack */
    for (i = 0; i < size; i++)
        milo_free((milo_value*)milo_context_pop(c, sizeof(milo_value)));
    return ret;
}

static int milo_parse_object(milo_context* c, milo_value* v) {
    size_t i, size;
    milo_member m;
    int ret;
    EXPECT(c, '{');
    milo_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = MILO_OBJECT;
        v->u.o.m = 0;
        v->u.o.size = 0;
        return MILO_PARSE_OK;
    }
    m.k = NULL;
    size = 0;
    for (;;) {
        char* str;
        milo_init(&m.v);
        /* parse key */
        if (*c->json != '"') {
            ret = MILO_PARSE_MISS_KEY;
            break;
        }
        if ((ret = milo_parse_string_raw(c, &str, &m.klen)) != MILO_PARSE_OK)
            break;
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        /* parse ws colon ws */
        milo_parse_whitespace(c);
        if (*c->json != ':') {
            ret = MILO_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        milo_parse_whitespace(c);
        /* parse value */
        if ((ret = milo_parse_value(c, &m.v)) != MILO_PARSE_OK)
            break;
        memcpy(milo_context_push(c, sizeof(milo_member)), &m, sizeof(milo_member));
        size++;
        m.k = NULL; /* ownership is transferred to member on stack */
        /* parse ws [comma | right-curly-brace] ws */
        milo_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            milo_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            size_t s = sizeof(milo_member) * size;
            c->json++;
            v->type = MILO_OBJECT;
            v->u.o.size = size;
            memcpy(v->u.o.m = (milo_member*)malloc(s), milo_context_pop(c, s), s);
            return MILO_PARSE_OK;
        }
        else {
            ret = MILO_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* Pop and free members on the stack */
    free(m.k);
    for (i = 0; i < size; i++) {
        milo_member* m = (milo_member*)milo_context_pop(c, sizeof(milo_member));
        free(m->k);
        milo_free(&m->v);
    }
    v->type = MILO_NULL;
    return ret;
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
        case '[':  return milo_parse_array(c, v);
        case '{':  return milo_parse_object(c, v);
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
    size_t  i;
    assert( v!= NULL);
    switch (v->type) {
        case MILO_STRING:
            free(v->u.s.s);
            break;
        case MILO_ARRAY:
            for (i = 0; i < v->u.a.size; i++)
                milo_free(&v->u.a.e[i]);
            free(v->u.a.e);
            break;
        case MILO_OBJECT:
            for (i = 0; i < v->u.o.size; i++) {
                free(v->u.o.m[i].k);
                milo_free(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default: break;
    }
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

size_t milo_get_array_size(const milo_value* v) {
    assert(v!= NULL && v->type == MILO_ARRAY);
    return v->u.a.size;
}

milo_value* milo_get_array_element(const milo_value* v, size_t index) {
    assert(v!=NULL && v->type == MILO_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

size_t milo_get_object_size(const milo_value* v) {
    assert(v != NULL && v->type == MILO_OBJECT);
    return v->u.o.size;
}

const char* milo_get_object_key(const milo_value* v, size_t index) {
    assert(v != NULL && v->type == MILO_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t milo_get_object_key_length(const milo_value* v, size_t index) {
    assert(v != NULL && v->type == MILO_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

milo_value* milo_get_object_value(const milo_value* v, size_t index) {
    assert(v != NULL && v->type == MILO_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}