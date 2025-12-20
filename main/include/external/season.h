/* season.h
 * v1.3
 * public domain json reader/writer
 * https://github.com/Noderyos/SeaSON
 * no warranty implied; use at your own risk

USAGE

    Do this:
        #define SEASON_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation.

    // i.e. it should look like this:
    #include ...
    #include ...
    #include ...
    #define SEASON_IMPLEMENTATION
    #include "season.h"

DOCUMENTATION

    You shall not use any symbols prefixed with ‘_’,
    unless you know exactly what you're doing.

    List of supplied symbols
        * enum season_type
            - JSON element type, used for element creation
            - YOU NEED TO SET IT YOURSELF IF YOU CREATE A SEASON ELEMENT AT HAND

        * struct season
            - JSON element, includes all types using union,
                .type field indicates which element below is used
                    SEASON_NUMBER  > .number  - double
                    SEASON_BOOLEAN > .boolean - int, false if 0, true otherwise
                    SEASON_NULL    > [empty]  - is implied
                    SEASON_STRING  > .string  - char*, use season_string to create it
                    SEASON_OBJECT  > ._object - shall not access it directly, hence the _
                    SEASON_ARRAY   > ._array  - shall not access it directly, hence the _

        * Helper macros
            - season_object()   - Define empty object
            - season_array()    - Define empty array
            - season_number(x)  - Define number from x
            - season_boolean(x) - Define boolean from x
            - season_null()     - Define null

        * struct season season_string(const char *s);
            - Create season from char*

        * struct season *season_object_get(struct season *object, const char *key);
            - Retrieve value from object by key.
            - Returns NULL if key is not found.

        * int season_object_add(struct season *object, char *key, struct season item);
            - Add key-value pair to object.
            - Overwrites key if already present.
            - Returns 0 on success, negative value otherwise

        * int season_object_remove(struct season *object, char *key);
            - Remove key-value pair from object by key.
            - Does nothing if key not found.
            - Returns 0 on success, negative value otherwise

        * int season_array_len(struct season *array);
            - Returns array length, -1 if failed

        * struct season *season_array_get(struct season *array, size_t idx);
            - Retrieve item from array at idx.
            - Returns NULL if out of range.

        * int season_array_add(struct season *array, struct season item);
            - Append item to array.
            - Returns 0 on success, negative value otherwise

        * int season_array_remove(struct season *array, size_t idx);
            - Remove item from array at idx.
            - Does nothing if idx is out-of-range.
            - Returns 0 on success, negative value otherwise

        * int season_array_insert(struct season *array, struct season item, size_t idx);
            - Insert item into array at idx.
            - Shifts items right. (just to clarify)
            - Appends if idx is out-of-range.
            - Returns 0 on success, negative value otherwise

        * int season_load(struct season *season, char *json_string);
            - Parse json_string into season.
            - Returns 0 on success, negative value otherwise

        * int season_render(FILE *stream, struct season *season);
            - Write JSON representation of season to stream.
            - Returns 0 on success, negative value otherwise

        * int season_render_indent(FILE *stream, struct season *season, size_t indent);
            - Pretty write JSON representation of season to stream.
            - Returns 0 on success, negative value otherwise

        * char *season_render_string(struct season *season, size_t *len);
            - Write JSON representation of season to a char* and return it.
            - Stores string length in len
            - YOU need to free it yourself

        * int season_free(struct season *season);
            - Recursively free all memory associated with season structure.
            - Returns 0 on success, negative value otherwise

    Short mode
        Do this:
            #define SEASON_SHORT
        before you include this file

        Added enums
            * SEASON_STRING  > SEASON_STR
            * SEASON_NUMBER  > SEASON_NUM
            * SEASON_BOOLEAN > SEASON_BOOL
            * SEASON_OBJECT  > SEASON_OBJ
            * SEASON_ARRAY   > SEASON_ARR

        Added typedefs
            * struct season  > season

        Added macros
            * season_arr  > season_array
            * season_obj  > season_object
            * season_str  > season_string
            * season_num  > season_number
            * season_bool > season_boolean

            * season_obj_get    > season_object_get
            * season_obj_add    > season_object_add
            * season_obj_remove > season_object_remove

            * season_arr_len    > season_array_len
            * season_arr_get    > season_array_get
            * season_arr_add    > season_array_add
            * season_arr_remove > season_array_remove
            * season_arr_insert > season_array_insert

LICENSE

    See end of file for license information.

CHANGELOG

    v1.0 - 08/10/2025
        - Initial commit

    v1.1 - 08/20/2025
        - Adding season_render_string

    v1.2 - 08/23/2025
        - Converting asserts to return values, allowing a cleaner error handling
        - Adding changelog section

    v1.2.1 - 08/23/2025
        - Adding season_array_len

    v1.3 - 12/19/2025
        - Exposing string type
        - Changing arguments order for season_render
        - Adding season_render_indent
*/

#ifndef SEASON_H
#define SEASON_H

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum season_type {
    _SEASON_ERROR = -1,
    SEASON_STRING = 0,
    SEASON_NULL = 1,
    SEASON_NUMBER = 2,
    SEASON_BOOLEAN = 3,
    SEASON_OBJECT = 4,
    SEASON_ARRAY = 5,
#ifdef SEASON_SHORT
    SEASON_STR = 0,
    SEASON_NUM = 2,
    SEASON_BOOL = 3,
    SEASON_OBJ = 4,
    SEASON_ARR = 5,
#endif
};

struct season_string {
    size_t len;
    char *str;
};
struct _season_object {
    size_t count;
    size_t capacity;
    struct _season_object_el {
        char *key;
        struct season *value;
    } *items;
};
struct _season_array {
    size_t count;
    size_t capacity;
    struct season *items;
};

struct season {
    enum season_type type;
    union {
        double number;
        int boolean;
        struct season_string string;
        struct _season_object _object;
        struct _season_array _array;
    };
};

struct season season_string(const char *s);

#define season_object() ((struct season){.type=SEASON_OBJECT})
#define season_array() ((struct season){.type=SEASON_ARRAY})
#define season_number(x) ((struct season){.type=SEASON_NUMBER, .number=x})
#define season_boolean(x) ((struct season){.type=SEASON_BOOLEAN, .boolean=x})
#define season_null() ((struct season){.type=SEASON_NULL})

struct season *season_object_get(struct season *object, const char *key);
int season_object_add(struct season *object, char *key, struct season item);
int season_object_remove(struct season *object, char *key);

struct season *season_array_get(struct season *array, size_t idx);
int season_array_len(struct season *array);
int season_array_add(struct season *array, struct season item);
int season_array_remove(struct season *array, size_t idx);
int season_array_insert(struct season *array, struct season item, size_t idx);

int season_load(struct season *season, char *json_string);
int season_render(FILE *stream, struct season *season);
int season_render_indent(FILE *stream, struct season *season, size_t indent);
char *season_render_string(struct season *season, size_t *len);
int season_free(struct season *season);

#ifdef SEASON_SHORT
typedef struct season season;
#define season_obj_get season_object_get
#define season_obj_add season_object_add
#define season_obj_remove season_object_remove

#define season_arr_get season_array_get
#define season_arr_len season_array_len
#define season_arr_add season_array_add
#define season_arr_remove season_array_remove
#define season_arr_insert season_array_insert

#define season_arr season_array
#define season_obj season_object
#define season_str season_string
#define season_num season_number
#define season_bool season_boolean
#endif

#endif
#ifdef SEASON_IMPLEMENTATION
#undef SEASON_IMPLEMENTATION

#include <stdarg.h>

#define _SEASON_ASSERT(x, fmt, ...) \
        do { \
            if(!(x)) { \
                fprintf(stderr, "assert: %s:%d: %s: '%s': " \
                        fmt "\n", __FILE__, __LINE__, __FUNCTION__, \
                        #x, ##__VA_ARGS__); \
                exit(1); \
            } \
        }while(0)

enum _season_token_type {
    _SEASON_TOK_END = 0,
    _SEASON_TOK_OPEN_CURLY,
    _SEASON_TOK_CLOSE_CURLY,
    _SEASON_TOK_OPEN_BRACKET,
    _SEASON_TOK_CLOSE_BRACKET,
    _SEASON_TOK_COMMA,
    _SEASON_TOK_COLON,
    _SEASON_TOK_STRING,
    _SEASON_TOK_NUMBER,
    _SEASON_TOK_NULL,
    _SEASON_TOK_TRUE,
    _SEASON_TOK_FALSE,
    _SEASON_TOK_SYMBOL, // should not be used outside of season_lex_next
    _SEASON_TOK_INVALID
};

struct _season_token {
    enum _season_token_type type;
    size_t line;
    size_t column;
    const char *text;
    size_t text_len;
};

struct _season_lexer {
    char *content;
    size_t content_len;
    size_t cursor;
    size_t line;
    size_t bol;
};

static const enum _season_token_type SEASON_LITERAL_MAP[256] = {
    ['{'] = _SEASON_TOK_OPEN_CURLY,
    ['}'] = _SEASON_TOK_CLOSE_CURLY,
    ['['] = _SEASON_TOK_OPEN_BRACKET,
    [']'] = _SEASON_TOK_CLOSE_BRACKET,
    [','] = _SEASON_TOK_COMMA,
    [':'] = _SEASON_TOK_COLON
};

struct _season_lexer _season_lex_init(char *content, size_t content_len){
    struct _season_lexer l = {content, content_len, 0, 0, 0};
    return l;
}

#define _season_is_num_start(c) (strchr("0123456789-", c) != NULL)
#define _season_is_num(c) (strchr("0123456789-.eE", c) != NULL)

char _season_lex_chop_char(struct _season_lexer *l){
    char x = l->content[l->cursor];
    l->cursor++;
    if(x == '\n'){
        l->line++;
        l->bol = l->cursor;
    }
    return x;
}

struct _season_token _season_lex_next(struct _season_lexer *l){
    while (l->cursor < l->content_len && isspace((int)l->content[l->cursor])){
        (void)_season_lex_chop_char(l);
    }

    struct _season_token token = {
        .type = _SEASON_TOK_END,
        .line = l->line+1,
        .column = l->cursor-l->bol+1,
        .text = &l->content[l->cursor]
    };

    if (l->cursor >= l->content_len) return token;

    if (l->content[l->cursor] == '"') {
        token.type = _SEASON_TOK_STRING;
        l->cursor++;
        token.text++;
        while (l->cursor < l->content_len && l->content[l->cursor] != '"') {
            token.text_len++;
            l->cursor++;
        }
        l->cursor++;
        return token;
    }

    if (_season_is_num_start(l->content[l->cursor])) {
        token.type = _SEASON_TOK_NUMBER;
        while (l->cursor < l->content_len && _season_is_num(l->content[l->cursor])) {
            token.text_len++;
            l->cursor++;
        }
        return token;
    }

    if (islower((int)l->content[l->cursor])) {
        token.type = _SEASON_TOK_INVALID;
        if (strncmp(&l->content[l->cursor], "null", 4) == 0) {
            token.type = _SEASON_TOK_NULL;
            token.text_len = 4;
            l->cursor += 4;
        } else if (strncmp(&l->content[l->cursor], "true", 4) == 0) {
            token.type = _SEASON_TOK_TRUE;
            token.text_len = 4;
            l->cursor += 4;
        } else if (strncmp(&l->content[l->cursor], "false", 5) == 0) {
            token.type = _SEASON_TOK_FALSE;
            token.text_len = 5;
            l->cursor += 5;
        }
        return token;
    }

    enum _season_token_type lit_type = SEASON_LITERAL_MAP[(uint8_t)l->content[l->cursor]];
    if (lit_type) {
        l->cursor++;
        token.type = lit_type;
        token.text_len = 1;
        return token;
    }

    (void)_season_lex_chop_char(l);
    token.type = _SEASON_TOK_INVALID;
    token.text_len = 1;
    return token;
}


char *_season_strdup(const char *s) { // C99 don't have strdup
    size_t size = strlen(s) + 1;
    char *str = malloc(size);
    if (str) memcpy(str, s, size);
    return str;
}

int _season_is_int(double x) {
    const double eps = 1e-9;
    long xi = (long)x;
    double diff = x - (double)xi;

    if (diff < 0) diff = -diff;
    return diff < eps;
}


char *_season_escape(const char *str, size_t len) {
    char *out = malloc(len*2 + 1);
    _SEASON_ASSERT(out != NULL, "Buy more RAM lol");

    char *p = out;

    while (len--) {
        char c = *str++;
        switch (c) {
            case '"':  *p++ = '\\'; *p++ = '"';  break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '/':  *p++ = '\\'; *p++ = '/';  break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\v': *p++ = '\\'; *p++ = 'v';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            default:
                *p++ = c;
                break;
        }
    }
    *p = '\0';
    return out;
}

char *_season_unescape(const char *str, size_t len) {
    char *out = malloc(len + 1);
    _SEASON_ASSERT(out != NULL, "Buy more RAM lol");
    char *p = out;

    while(len--) {
        if (*str == '\\') {
            str++;
            char next = *str;
            switch(next) {
                case '"':
                case '\\':
                case '/':
                    *p++ = next;
                    break;
                case 'b':*p++ = 8;break;
                case 't':*p++ = 9;break;
                case 'n':*p++ = 10;break;
                case 'v':*p++ = 11;break;
                case 'f':*p++ = 12;break;
                case 'r':*p++ = 13;break;
                default:
                    // Skip it
            }
            str++;
        } else {
            *p++ = *str++;
        }
    }
    *p = '\0';
    return out;
}

struct season _season_parse_symbol(struct _season_token t) {
    struct season value;
    switch (t.type) {
        case _SEASON_TOK_STRING:
            value.type = SEASON_STRING;
            value.string.str = _season_unescape(t.text, t.text_len);
            value.string.len = t.text_len;
            break;
        case _SEASON_TOK_NUMBER:
            value.type = SEASON_NUMBER;
            value.number = strtod(t.text, NULL);
            break;
        case _SEASON_TOK_NULL:
            value.type = SEASON_NULL;
            break;
        case _SEASON_TOK_TRUE:
            value.type = SEASON_BOOLEAN;
            value.boolean = 1;
            break;
        case _SEASON_TOK_FALSE:
            value.type = SEASON_BOOLEAN;
            value.boolean = 0;
            break;
        default:
            return (struct season){.type = _SEASON_ERROR};
    }
    return value;
}

struct season _season_parse_array(struct _season_lexer *l);
struct season _season_parse_object(struct _season_lexer *l) {
    struct season object = {.type = SEASON_OBJECT};
    struct _season_token t = _season_lex_next(l);
    while (t.type != _SEASON_TOK_CLOSE_CURLY) {
        if (t.type != _SEASON_TOK_STRING)
            return (struct season){.type = _SEASON_ERROR}; // Expecting key
        char *key = _season_unescape(t.text, t.text_len);
        t = _season_lex_next(l);
        if (t.type != _SEASON_TOK_COLON)
            return (struct season){.type = _SEASON_ERROR}; // Expecting :
        t = _season_lex_next(l);

        struct season value;
        switch (t.type) {
            case _SEASON_TOK_STRING:
            case _SEASON_TOK_NUMBER:
            case _SEASON_TOK_NULL:
            case _SEASON_TOK_TRUE:
            case _SEASON_TOK_FALSE:
                value = _season_parse_symbol(t);
                break;

            case _SEASON_TOK_OPEN_CURLY:
                value = _season_parse_object(l);
                if (value.type == _SEASON_ERROR) return value;
                break;
            case _SEASON_TOK_OPEN_BRACKET:
                value = _season_parse_array(l);
                if (value.type == _SEASON_ERROR) return value;
                break;
            default:
                return (struct season){.type = _SEASON_ERROR}; // Invalid token
        }
        if (season_object_add(&object, key, value) < 0)
            return (struct season){.type = _SEASON_ERROR};
        free(key);
        t = _season_lex_next(l);
        if (t.type != _SEASON_TOK_CLOSE_CURLY && t.type != _SEASON_TOK_COMMA)
            return (struct season){.type = _SEASON_ERROR}; // Expecting ,
        if (t.type == _SEASON_TOK_COMMA) {
            t = _season_lex_next(l);
            if (t.type == _SEASON_TOK_CLOSE_CURLY)
                return (struct season){.type = _SEASON_ERROR}; // Trailing comma
        }
    }
    return object;
}

struct season _season_parse_array(struct _season_lexer *l) {
    struct season array = {.type = SEASON_ARRAY};
    struct _season_token t = _season_lex_next(l);
    while (t.type != _SEASON_TOK_CLOSE_BRACKET) {
        struct season value;
        switch (t.type) {
            case _SEASON_TOK_STRING:
            case _SEASON_TOK_NUMBER:
            case _SEASON_TOK_NULL:
            case _SEASON_TOK_TRUE:
            case _SEASON_TOK_FALSE:
                value = _season_parse_symbol(t);
                break;

            case _SEASON_TOK_OPEN_CURLY:
                value = _season_parse_object(l);
                if (value.type == _SEASON_ERROR) return value;
                break;
            case _SEASON_TOK_OPEN_BRACKET:
                value = _season_parse_array(l);
                if (value.type == _SEASON_ERROR) return value;
                break;
            default:
                return (struct season){.type = _SEASON_ERROR}; // Invalid token
        }
        if (season_array_add(&array, value) < 0)
            return (struct season){.type = _SEASON_ERROR};
        t = _season_lex_next(l);
        if (t.type != _SEASON_TOK_CLOSE_BRACKET && t.type != _SEASON_TOK_COMMA)
            return (struct season){.type = _SEASON_ERROR}; // Expecting ,
        if (t.type == _SEASON_TOK_COMMA) {
            t = _season_lex_next(l);
            if (t.type == _SEASON_TOK_CLOSE_BRACKET)
                return (struct season){.type = _SEASON_ERROR}; // Trailing comma
        }
    }
    return array;
}

int _season_object_idx(struct season *object, const char *key) {
    if (!object || object->type != SEASON_OBJECT) return -2;

    for (size_t i = 0; i < object->_object.count; i++) {
        if (strcmp(object->_object.items[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

struct season season_string(const char *s) {
    struct season string = {
        .type = SEASON_STRING,
        .string.str = _season_strdup(s),
        .string.len = strlen(s)
    };
    return string;
}

struct season *season_object_get(struct season *object, const char *key) {
    int idx = _season_object_idx(object, key);
    if (idx < 0) return NULL;
    return object->_object.items[idx].value;
}

int season_object_add(struct season *object, char *key, struct season item) {
    if (!object || object->type != SEASON_OBJECT) return -1;

    int key_idx = _season_object_idx(object, key);
    if (key_idx == -2) return -1;
    if (key_idx < 0) {
        if (object->_object.count >= object->_object.capacity) {
            object->_object.capacity =
                object->_object.capacity == 0 ? 8 : object->_object.capacity*2;
            object->_object.items = realloc(
                object->_object.items,
                object->_object.capacity*sizeof(*object->_object.items));
            _SEASON_ASSERT(object->_object.items != NULL, "Buy more RAM lol");
        }
        struct _season_object_el *el = &object->_object.items[object->_object.count];
        el->key = _season_strdup(key);
        el->value = malloc(sizeof(item));
        memcpy(el->value, &item, sizeof(item));
        object->_object.count++;
    } else {
        if (season_free(object->_object.items[key_idx].value) < 0) return -1;
        memcpy(object->_object.items[key_idx].value, &item, sizeof(item));
    }
    return 0;
}

int season_object_remove(struct season *object, char *key) {
    if (!object || object->type != SEASON_OBJECT) return -1;

    int idx = _season_object_idx(object, key);
    if (idx == -2) return -1;
    if (idx >= 0) {
        free(object->_object.items[idx].key);
        if (season_free(object->_object.items[idx].value) < 0) return -1;
        free(object->_object.items[idx].value);
        memcpy(&object->_object.items[idx], &object->_object.items[idx+1],
            (object->_object.count-idx-1)*sizeof(*object->_object.items));
        object->_object.count--;
    }
    return 0;
}

int season_array_len(struct season *array) {
    if (!array || array->type != SEASON_ARRAY) return -1;
    return array->_array.count;
}

struct season *season_array_get(struct season *array, size_t idx) {
    if (!array || array->type != SEASON_ARRAY) return NULL;

    if (idx < array->_object.count) {
        return &array->_array.items[idx];
    }
    return NULL;
}

int season_array_add(struct season *array, struct season item) {
    if (!array || array->type != SEASON_ARRAY) return -1;
    if (season_array_insert(array, item, array->_array.count) < 0) return -1;
    return 0;
}

int season_array_remove(struct season *array, size_t idx) {
    if (!array || array->type != SEASON_ARRAY) return -1;

    if (idx >= array->_object.count) return 0;

    if (season_free(&array->_array.items[idx]) < 0) return -1;
    memcpy(&array->_array.items[idx], &array->_array.items[idx+1],
        (array->_array.count-idx-1)*sizeof(*array->_array.items));
    array->_array.count--;
    return 0;
}

int season_array_insert(struct season *array, struct season item, size_t idx) {
    if (!array || array->type != SEASON_ARRAY) return -1;

    if (idx > array->_object.count) idx = array->_object.count;

    if (array->_array.count >= array->_array.capacity) {
        array->_array.capacity =
            array->_array.capacity == 0 ? 8 : array->_array.capacity*2;
        array->_array.items = realloc(
            array->_array.items, array->_array.capacity*sizeof(*array->_array.items));
        _SEASON_ASSERT(array->_array.items != NULL, "Buy more RAM lol");
    }
    memcpy(&array->_array.items[idx+1], &array->_array.items[idx],
        (array->_array.count-idx)*sizeof(*array->_array.items));
    array->_array.items[idx] = item;
    array->_array.count++;
    return 0;
}

int season_load(struct season *season, char *json_string) {
    struct _season_lexer l = _season_lex_init(json_string, strlen(json_string));
    struct _season_token t = _season_lex_next(&l);
    switch (t.type) {
        case _SEASON_TOK_OPEN_CURLY:
            *season = _season_parse_object(&l);
            if (season->type == _SEASON_ERROR) return -1;
            break;
        case _SEASON_TOK_OPEN_BRACKET:
            *season = _season_parse_array(&l);
            if (season->type == _SEASON_ERROR) return -1;
            break;
        case _SEASON_TOK_STRING:
        case _SEASON_TOK_NUMBER:
        case _SEASON_TOK_NULL:
        case _SEASON_TOK_TRUE:
        case _SEASON_TOK_FALSE:
            *season = _season_parse_symbol(t);
            break;
        default:
            return -1; // Invalid token
    }
    return 0;
}

int season_render(FILE *stream, struct season *season) {
    if (!season) return -1;
    switch (season->type) {
        case SEASON_NULL:
            fprintf(stream, "null");
            break;
        case SEASON_BOOLEAN:
            fprintf(stream, "%s", season->boolean ? "true" : "false");
            break;
        case SEASON_STRING:
            char *escaped = _season_escape(season->string.str, season->string.len);
            fprintf(stream, "\"%s\"", escaped);
            free(escaped);
            break;
        case SEASON_NUMBER:
            if (_season_is_int(season->number))
                fprintf(stream, "%ld", (long)season->number);
            else
                fprintf(stream, "%lf", season->number);
            break;
        case SEASON_OBJECT:
            fprintf(stream, "{");
            for (size_t i = 0; i < season->_object.count; i++) {
                if (i) fprintf(stream, ", ");
                struct _season_object_el object = season->_object.items[i];
                fprintf(stream, "\"%s\": ", object.key);
                if (season_render(stream, object.value) < 0) return -1;
            }
            fprintf(stream, "}");
            break;
        case SEASON_ARRAY:
            fprintf(stream, "[");
            for (size_t i = 0; i < season->_array.count; i++) {
                if (i) fprintf(stream, ", ");
                if (season_render(stream, &season->_array.items[i]) < 0) return -1;
            }
            fprintf(stream, "]");
            break;
        default:
            return -1;
    }
    return 0;
}

#define _season_indent(stream, indent, level) \
    { \
       size_t n = (indent) * (level); \
       while (n--) { \
           fputc(' ', stream); \
       } \
    }

int _season_render_indent(FILE *stream, struct season *season,
                          size_t indent, size_t level) {
    if (!season) return -1;
    switch (season->type) {
        case SEASON_NULL:
            fprintf(stream, "null");
            break;
        case SEASON_BOOLEAN:
            fprintf(stream, "%s", season->boolean ? "true" : "false");
            break;
        case SEASON_STRING:
            char *escaped = _season_escape(season->string.str, season->string.len);
            fprintf(stream, "\"%s\"", escaped);
            free(escaped);
            break;
        case SEASON_NUMBER:
            if (_season_is_int(season->number))
                fprintf(stream, "%ld", (long)season->number);
            else
                fprintf(stream, "%lf", season->number);
            break;
        case SEASON_OBJECT:
            fprintf(stream, "{\n");
            for (size_t i = 0; i < season->_object.count; i++) {
                if (i) fprintf(stream, ", \n");
                struct _season_object_el object = season->_object.items[i];
                _season_indent(stream, indent, level+1);
                fprintf(stream, "\"%s\": ", object.key);
                if (_season_render_indent(stream, object.value, indent, level+1) < 0)
                    return -1;
            }
            fputc('\n', stream);
            _season_indent(stream, indent, level);
            fputc('}', stream);
            break;
        case SEASON_ARRAY:
            fprintf(stream, "[\n");
            for (size_t i = 0; i < season->_array.count; i++) {
                if (i) fprintf(stream, ", \n");
                _season_indent(stream, indent, level+1);
                if (_season_render_indent(stream, &season->_array.items[i],
                    indent, level+1) < 0) return -1;
            }
            fputc('\n', stream);
            _season_indent(stream, indent, level);
            fputc(']', stream);
            break;
        default:
            return -1;
    }
    return 0;
}

int season_render_indent(FILE *stream, struct season *season, size_t indent) {
    return _season_render_indent(stream, season, indent, 0);
}

struct _season_var_str {
    char *buf;
    size_t cap;
    size_t len;
};

static void _season_var_str_fmt(struct _season_var_str *var_str, const char *fmt, ...) {
    va_list args;
    while (1) {
        va_start(args, fmt);
        int needed = vsnprintf(var_str->buf + var_str->len,
                               var_str->cap - var_str->len,
                               fmt, args);
        va_end(args);

        if (needed < 0) return;

        if ((size_t)needed + 1 <= var_str->cap - var_str->len) {
            var_str->len += needed;
            return;
        }

        size_t newcap = var_str->cap * 2;
        if (newcap < var_str->len + (size_t)needed + 1)
            newcap = var_str->len + (size_t)needed + 1;

        char *tmp = realloc(var_str->buf, newcap);
        _SEASON_ASSERT(tmp != NULL, "Buy more RAM lol");
        var_str->buf = tmp;
        var_str->cap = newcap;
    }
}

int _season_render_string(struct season *season, struct _season_var_str *var_str) {
    if (!season) return -1;
    switch (season->type) {
        case SEASON_NULL:
            _season_var_str_fmt(var_str, "null");
            break;
        case SEASON_BOOLEAN:
            _season_var_str_fmt(var_str, "%s", season->boolean ? "true" : "false");
            break;
        case SEASON_STRING:
            char *escaped = _season_escape(season->string.str, season->string.len);
            _season_var_str_fmt(var_str, "\"%s\"", escaped);
            free(escaped);
            break;
        case SEASON_NUMBER:
            if (_season_is_int(season->number))
                _season_var_str_fmt(var_str, "%ld", (long)season->number);
            else
                _season_var_str_fmt(var_str, "%lf", season->number);
            break;
        case SEASON_OBJECT:
            _season_var_str_fmt(var_str, "{");
            for (size_t i = 0; i < season->_object.count; i++) {
                if (i) _season_var_str_fmt(var_str, ", ");
                struct _season_object_el object = season->_object.items[i];
                _season_var_str_fmt(var_str, "\"%s\": ", object.key);
                if (_season_render_string(object.value, var_str) < 0) return -1;
            }
            _season_var_str_fmt(var_str, "}");
            break;
        case SEASON_ARRAY:
            _season_var_str_fmt(var_str, "[");
            for (size_t i = 0; i < season->_array.count; i++) {
                if (i) _season_var_str_fmt(var_str, ", ");
                if (_season_render_string(&season->_array.items[i], var_str) < 0)
                    return -1;
            }
            _season_var_str_fmt(var_str, "]");
            break;
        default:
            return -1;
    }
    return 0;
}

char *season_render_string(struct season *season, size_t *len) {
    struct _season_var_str var_str = {0};
    if (_season_render_string(season, &var_str) < 0) {
        free(var_str.buf);
        *len = 0;
        return NULL;
    }
    *len = var_str.len;
    return var_str.buf;
}


int season_free(struct season *season) {
    if (!season) return -1;
    switch (season->type) {
        case SEASON_STRING:
            free(season->string.str);
            season->string.str = NULL;
            season->string.len = 0;
            break;
        case SEASON_OBJECT:
            for (size_t i = 0; i < season->_object.count; i++) {
                struct _season_object_el object = season->_object.items[i];
                free(object.key);
                if (season_free(object.value) < 0) return -1;
                free(object.value);
            }
            free(season->_object.items);
            season->_object.items = NULL;
            season->_object.count = 0;
            season->_object.capacity = 0;
            break;
        case SEASON_ARRAY:
            for (size_t i = 0; i < season->_array.count; i++) {
                if (season_free(&season->_array.items[i]) < 0) return -1;
            }
            free(season->_array.items);
            season->_array.items = NULL;
            season->_array.count = 0;
            season->_array.capacity = 0;
            break;
        case SEASON_NULL:
        case SEASON_BOOLEAN:
        case SEASON_NUMBER:
        default:
            // Nothing to free
            break;
    }
    return 0;
}

#endif

/*
------------------------------------------------------------------------------
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/