// wool code to mr.sheep vm bytecode compiler
// [syntax]:
//      name ::= [a-zA-Z_]+
//      tag  ::= name:
//      inst ::= name [arg]*
//      arg  ::= [&^]? number|name|char
//   NOTE: instruction names are case insensitive, commas don't do nothing
//         special, they can be used as separators. all is space insensitive.
//         numbers can be decimal, or 0xHex or 0bBynary
//         chars can contain only one char 'j' or if scaped 2, but only this are
//         valid: { '\'', '\\', '\n', '\r', '\0' }
//     main:
//          mov     &9,     10
//     loop:
//          inc     &10        ; comment, commas are optional :)
//          mov     ^10,    0
//          dec
//          jmp     loop

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mr.sheep.h"

#define issep(c)    (!(isalnum(c) && c != '_'))
#define isodigit(c) ('0' <= c || c <= '7')
#define isbdigit(c) (c == '0' || c == '1')

#define ASSERT_PTR(ptr) (assert(ptr && "buy more RAM! :)"), ptr)
#define GLUE(a, b)      a##b
#define _STRINGSING(a)  #a
#define STRINGSING(a)   _STRINGSING(a)
#define MIN(a, b)       ((a < b) ? a : b)

#define NEED_SCAPE(c)   (c == '\'' || c == '\\')
#define VALID_SCAPED(c) (c == 'n' || c == 'r' || c == '0' || NEED_SCAPE(c))
char scape_map[] = {
    ['\\'] = '\\',
    ['\''] = '\'',
    ['0'] = 0,
    ['n'] = '\n',
    ['r'] = '\r',
};

typedef enum {
    RET_SUCCESS = 0,
    RET_FAILURE = 1,
} return_code_t;

#define BIT(n) (1 << n)
#define CHUNK \
    X(NAME)   \
    X(CHAR)   \
    X(NUMBER)
#define SIMBOL        \
    X(COMMA, ',')     \
    X(COLON, ':')     \
    X(AMPERSAND, '&') \
    X(CAROT, '^')
// X(T_AT)
#define TOKENS \
    X(ILLEGAL) \
    CHUNK      \
    SIMBOL

#define X(t, ...) __T_##t,
enum { TOKENS };
#undef X

#define X(t, ...) T_##t = BIT(__T_##t),
typedef enum { TOKENS } token_t;
#undef X

#define X(t, ...) [T_##t] = STRINGSING(T_##t),
const char* token_rep_map[] = { TOKENS };
#undef X

#define R_VALUE_MASK (T_NAME | T_NUMBER | T_CHAR)

typedef struct {
    token_t type;
    const char* rep;
    size_t rep_size;
    size_t row;    /* base 1 */
    size_t column; /* base 1 */
} Token;
#define TOKEN_INIT(token, _type, _rep, _rep_size, _row, _column) \
    do {                                                         \
        (token).type = _type;                                    \
        (token).rep = _rep;                                      \
        (token).rep_size = _rep_size;                            \
        (token).row = _row;                                      \
        (token).column = _column + 1;                            \
    } while ( 0 );

#define DA_INIT_SIZE 1024
typedef struct {
    const char* file_name;
    size_t capacity;
    size_t count;
    Token** arr;
} Token_List;
#define _DA_APPEND_ITEM(da, item, init_size)                      \
    do {                                                          \
        if ( (da).count >= (da).capacity ) {                      \
            if ( (da).capacity == 0 ) {                           \
                (da).capacity = init_size;                        \
                (da).arr = malloc(sizeof(*(da).arr) * init_size); \
            } else {                                              \
                (da).capacity = (da).capacity * 2;                \
                (da).arr = realloc((da).arr, (da).capacity);      \
            }                                                     \
            ASSERT_PTR((da).arr);                                 \
        }                                                         \
        (da).arr[(da).count++] = item;                            \
    } while ( 0 );
#define DA_APPEND_ITEM(da, item) _DA_APPEND_ITEM(da, item, DA_INIT_SIZE)
#define DA_STRUCT(T)     \
    struct {             \
        size_t capacity; \
        size_t count;    \
        T* arr;          \
    }

// declarations ///////////////////////////////////////////////////////////////
inline static void tok_print_token(Token* t);

// lexer //////////////////////////////////////////////////////////////////////
#define X(...)
#define Z(kw, ...) K_##kw,
typedef enum { K_UNWOWN, BYTE_CODES _K_COUNT } keyword_t;
#undef Z

#define Z(kw, ...) [K_##kw] = #kw,
const char* keyword_rep_map[] = { [K_UNWOWN] = "?", BYTE_CODES };
#undef Z

#define Z(inst, n, args, ...) [K_##inst] = args,
int valid_args[_K_COUNT][MAX_ARGC] = { { 0 }, BYTE_CODES };
#undef Z

#define Z(inst, n, args, ...) [K_##inst] = n,
uint8_t instruction_size_map[] = { 0, BYTE_CODES };
#undef Z
#undef X

#define Z(...)
#define X(inst, _, args, ...) [inst] = args,
int valid_args_2[_BC_COUNT][MAX_ARGC] = { BYTE_CODES };
#undef X
#undef Z


// NOTE this is a copy of the enum defined in ./mr.sheep.h
typedef enum {
    SY_INV = ARG_INVALID,
    SY_REF = ARG_REFERENCE,
    SY_LIT = ARG_LITERAL,
    SY_PTR = ARG_POINTER
} syllable_t;

// typedef enum { S_LABEL, S_INSTRUCTION } statement_t;

typedef struct {
    syllable_t type;
    Token* token; // NOTE: you only need the NAME|NUMBER + if its REF or LIT
} Syllable;
typedef struct {
    keyword_t type;
    Token* base;
    union {
        Syllable* tail; // NULL if no arg inst
        size_t offset;
    } meta;
} Statement;
#define _STATEMENT_INIT(statement, _type, _base, _meta, _val) \
    do {                                                      \
        (statement).type = _type;                             \
        (statement).base = _base;                             \
        (statement).meta._meta = _val;                        \
    } while ( 0 );
#define LABEL_STATEMENT_INIT(statement, _type, _base, _val) \
    _STATEMENT_INIT(statement, _type, _base, offset, _val)
#define INSTR_STATEMENT_INIT(statement, _type, _base, _val) \
    _STATEMENT_INIT(statement, _type, _base, tail, _val)
typedef struct {
    size_t capacity;
    size_t count;
    Statement** arr;
} Statement_List;

inline static void lex_print_labl(Statement* s)
{
    Token* t = s->base;
    char* r = malloc(sizeof(char) * (t->rep_size + 1));
    ASSERT_PTR(r);

    memcpy(r, t->rep, t->rep_size);
    r[t->rep_size] = '\0';

    printf("%% S_LABL('%s'):[%zu]\n", r, s->meta.offset);

    free(r);
}
inline static void lex_print_inst(Statement* s)
{
    Token* t = s->base;
    char* r = malloc(sizeof(char) * (t->rep_size + 1));
    ASSERT_PTR(r);

    memcpy(r, t->rep, t->rep_size);
    r[t->rep_size] = '\0';

    // printf("%% S_INST('%s')\n", r);

    for ( int i = 0; i < instruction_size_map[s->type]; i++ ) {
        printf("\t");
        if ( s->meta.tail[i].type == SY_LIT ) {
            printf("[LIT]");
        } else if ( s->meta.tail[i].type == SY_REF ) {
            printf("[REF]");
        } else {
            printf("[PTR]");
        }
        tok_print_token(s->meta.tail[i].token);
    }

    free(r);
}

static return_code_t lex_process_tokenss(Token_List* tokens, int out_fd)
{
    Statement_List entities = { 0 };
    DA_STRUCT(Statement) labels = { 0 };

    { // fill the list
        int errors = 0;
        Statement* s = malloc(sizeof(Statement));
        ASSERT_PTR(s);
        size_t byte_count = 0;
        for ( size_t indx = 0; indx < tokens->count; indx++ ) {
            Token* t = tokens->arr[indx];

            if ( t->type != T_NAME ) {
                printf("%s:%zu:%zu:error: unexpected token:\n\t'%s'\n",
                    tokens->file_name, t->row, t->column, t->rep);
                tok_print_token(t);
                errors++;
                continue;
            }

            if ( indx + 1 < tokens->count ) { // check if label
                Token* t2 = tokens->arr[indx + 1];
                if ( t2->type == T_COLON ) {
                    Statement s_label = {
                        .type = K_UNWOWN,
                        .base = t,
                        .meta.offset = byte_count,
                    };
                    indx++;
                    // lex_print_labl(&s_label);
                    // TODO: check if label already exists...
                    DA_APPEND_ITEM(labels, s_label);
                    continue;
                }
            }
            // HACK: make the ominous if (strA==strB) {} else ... block disapear
            // NOTE: this checks one of all the instruction match with the rep
            //       of the token.
            int* arg_mask;
            int arg_count, keyword;
#define X(...)
#define Z(kw, n, args)                                  \
    if ( strncasecmp(#kw, t->rep, t->rep_size) == 0 ) { \
        arg_mask = valid_args[K_##kw];                  \
        arg_count = n;                                  \
        keyword = K_##kw;                               \
    } else
            BYTE_CODES
#undef Z
#undef X
            /* else */ {
                printf("%s:%zu:%zu:error: unknown instruction:\n\t'%s'\n",
                    tokens->file_name, t->row, t->column, t->rep);
                errors++;
                continue;
            }

            if ( arg_count == 0 ) {
                INSTR_STATEMENT_INIT(*s, keyword, t, NULL);
                goto append_statement;
            }

            Syllable* tail = calloc(sizeof(Syllable), arg_count);
            ASSERT_PTR(tail);

            int init_errors = errors, init_indx = indx;
            for ( int i = 0; i < arg_count; i++ ) {
                if ( indx + 1 >= tokens->count ) {
                    printf("%s:%zu:%zu:error: unfinised statement:\n\t'%s'\n",
                        tokens->file_name, t->row, t->column, t->rep);
                    free(tail);
                    errors++;
                    break;
                }

                Token* t2 = tokens->arr[indx + 1];

                // NOTE, we removed the commas
                if ( t2->type & R_VALUE_MASK ) {
                    tail[i].token = t2;
                    tail[i].type = SY_LIT;
                    indx += 1;
                } else if ( t2->type == T_AMPERSAND ) {
                    if ( indx + 2 >= tokens->count
                        || !(tokens->arr[indx + 2]->type & R_VALUE_MASK) ) {
                        printf("%s:%zu:%zu: expected a name or literal after "
                               "'&':\n\t%s\n",
                            tokens->file_name, t->row, t->column, t->rep);
                        free(tail);
                        errors++;
                        break;
                    }
                    tail[i].token = tokens->arr[indx + 2];
                    tail[i].type = SY_REF;
                    indx += 2;
                } else if ( t2->type == T_CAROT ) {
                    if ( indx + 2 >= tokens->count
                        || !(tokens->arr[indx + 2]->type & R_VALUE_MASK) ) {
                        printf("%s:%zu:%zu:error: expected a name or literal "
                               "after '^':\n\t%s\n",
                            tokens->file_name, t->row, t->column, t->rep);
                        free(tail);
                        errors++;
                        break;
                    }
                    tail[i].token = tokens->arr[indx + 2];
                    tail[i].type = SY_PTR;
                    indx += 2;
                }
            }
            if ( init_errors != errors ) { continue; }
            INSTR_STATEMENT_INIT(*s, keyword, t, tail);
append_statement:
            // printf("[%zu] ", byte_count);
            // lex_print_inst(s);
            byte_count += arg_count + 1;
            DA_APPEND_ITEM(entities, s);
            s = malloc(sizeof(Statement));
            ASSERT_PTR(s);
        }
        free(s);
        if ( errors ) { return RET_FAILURE; }
    }
    { // check labels
        bool errors = false;
        for ( int i = 0; i < labels.count; i++ ) {
            // we do this, because latter we will need to strcmp and our setup
            // with global strings is dificult to work with ._.
            Token* l1 = labels.arr[i].base;
            char* rep = malloc(sizeof(char) * l1->rep_size);
            ASSERT_PTR(rep);
            strncpy(rep, l1->rep, l1->rep_size);
            rep[l1->rep_size] = '\0';
            l1->rep = rep;
            // printf("label[%d]: '%s'\n", i, rep);

            for ( size_t j = 0; j < i; j++ ) { // check redefined labels
                Token* l0 = labels.arr[j].base;
                if ( strcmp(l0->rep, l1->rep) ) { continue; }
                errors = true;
                printf("%s:%zu:%zu:error: redefined label '%s' first declared "
                       "at '%zu:%zu'\n",
                    tokens->file_name, l0->row, l0->column, l0->rep, l1->row,
                    l1->column);
                break;
            }
        }
        if ( errors ) { return RET_FAILURE; }
    }
    DA_STRUCT(uint8_t) byte_code = { 0 };
    { // resolve the entities
        // printf("size labl: %zu\n", labels.count);
        // printf("size inst: %zu\n", entities.count);

        for ( int i = 0; i < entities.count; i++ ) {
            Statement* s = entities.arr[i];
            Token* t = entities.arr[i]->base;
            keyword_t keyword = entities.arr[i]->type;
            Syllable* tail = entities.arr[i]->meta.tail;
            byte_code_t bc;
            uint8_t argc;
            // printf("checking '%s'\n", keyword_rep_map[keyword]);
            switch ( keyword ) { // HACK: I hope the compiler optimizes this...
#define Z(kw, n, args)   \
    goto no_match_error; \
    case K_##kw:
#define X(kw, n, ...)                                             \
    if ( n == 0 || (n == 1 && valid_args_2[kw][0] & tail[0].type) \
        || (n == 2 && valid_args_2[kw][0] & tail[0].type          \
            && valid_args_2[kw][1] & tail[1].type) ) {            \
        bc = kw;                                                  \
        argc = n;                                                 \
        break;                                                    \
    }
                case K_UNWOWN:
                    printf("invalid keyword '%s'\n", keyword_rep_map[keyword]);
                    return RET_FAILURE;
                    BYTE_CODES break;
#undef X
#undef Z
                case _K_COUNT:
                default:
                    printf("invalid keyword '%s'\n", keyword_rep_map[keyword]);
                    return RET_FAILURE;
            }
            DA_APPEND_ITEM(byte_code, bc);
            for ( int i = 0; i < argc; i++ ) {
                // printf("\tresolving arg %d of %d\n", i + 1, argc);
                Token* ti = tail[i].token;
                int8_t val;
                if ( ti->type == T_CHAR ) {
                    // TODO: scape stuff
                    if ( ti->rep_size < 3 ) {
                        printf("%s:%zu:%zu:error: empty char \n\t%s\n",
                            tokens->file_name, t->row, t->column, t->rep);
                        return RET_FAILURE;
                    } else if ( ti->rep_size == 3 ) {
                        if ( NEED_SCAPE(ti->rep[1]) ) {
                            printf("%s:%zu:%zu:error: char value '%c' "
                                   "needs to "
                                   "be scaped\n\t%s\n",
                                tokens->file_name, t->row, t->column,
                                ti->rep[1], t->rep);
                            return RET_FAILURE;
                        }
                        val = ti->rep[1];
                    } else if ( ti->rep_size == 4 ) {
                        if ( !VALID_SCAPED(ti->rep[1]) ) {
                            printf("%s:%zu:%zu:error: char value '%c' "
                                   "can't be "
                                   "scaped\n\t%s\n",
                                tokens->file_name, t->row, t->column,
                                ti->rep[1], t->rep);
                            return RET_FAILURE;
                        }
                        val = scape_map[ti->rep[2]];
                    } else {
                        printf("%s:%zu:%zu:error: char value can only have 1 "
                               "or 2 characters (if scaped)\n\t%s\n",
                            tokens->file_name, t->row, t->column, t->rep);
                    }
                } else if ( ti->type == T_NUMBER ) {
                    long v;
                    if ( ti->rep_size > 2 ) {
                        if ( ti->rep[0] == '0' ) {
                            if ( ti->rep[1] == 'x' ) {
                                v = strtol(ti->rep + 2, NULL, 16);
                                goto strtol_done;
                            } else if ( ti->rep[1] == 'o' ) {
                                v = strtol(ti->rep + 2, NULL, 8);
                                goto strtol_done;
                            } else if ( ti->rep[1] == 'b' ) {
                                v = strtol(ti->rep + 2, NULL, 2);
                                goto strtol_done;
                            }
                        }
                    };
                    v = strtol(ti->rep, NULL, 10);
strtol_done:
                    if ( v < INT8_MIN || UINT8_MAX < v ) {
                        // tok_print_token(ti);
                        printf("'%s'\n", ti->rep);
                        printf("%s:%zu:%zu:warning: value overflow truncating "
                               "(%d < %ld < %d), in \n\t%s\n",
                            tokens->file_name, t->row, t->column, INT8_MIN, v,
                            UINT8_MAX, t->rep);
                    }
                    val = (int8_t)v;
                } else {
                    bool find = false;
                    for ( size_t j = 0; j < labels.count; j++ ) {
                        Statement* label = &labels.arr[j];
                        if ( label->base->rep_size == ti->rep_size
                            && (!strncmp(label->base->rep, ti->rep,
                                label->base->rep_size)) ) {
                            long v = label->meta.offset - byte_code.count;
                            // v = (v >= 0) ? (v - 1) : (v + argc - 1);

                            if ( v < INT8_MIN || INT8_MAX < v ) {
                                printf("%s:%zu:%zu:error: value overflow in "
                                       "jump (%d < %ld < %d), in \n\t%s\n",
                                    tokens->file_name, t->row, t->column,
                                    INT8_MIN, v, INT8_MAX, t->rep);
                                return RET_FAILURE;
                            }
                            val = (int8_t)v;
                            // printf("\toffset %d\n", val);
                            find = true;
                            break;
                        }
                    }
                    if ( !find ) {
                        char* rep = calloc(sizeof(char), ti->rep_size + 1);
                        ASSERT_PTR(rep);
                        strncpy(rep, ti->rep, ti->rep_size);
                        printf("%s:%zu:%zu:error: unknown label '%s', "
                               "in\n\t%s\n",
                            tokens->file_name, t->row, t->column, rep, t->rep);
                        free(rep);
                        return RET_FAILURE;
                    }
                }
                DA_APPEND_ITEM(byte_code, val);
            }
            continue;
no_match_error:
            printf("%s:%zu:%zu:error: invalid args for '%s', in %s\n",
                tokens->file_name, t->row, t->column, keyword_rep_map[keyword],
                t->rep);
            return RET_FAILURE;
        }

        { // dump bytecode
            uint16_t magic_number = MR_SHEEP_MAGIC_NUMBER;
            write(out_fd, &magic_number, sizeof(uint16_t));
            write(out_fd, byte_code.arr,
                sizeof(byte_code.arr[0]) * byte_code.count);
        }
        return RET_SUCCESS;
    }
}

// tokenize
// ///////////////////////////////////////////////////////////////////
#define NUMBER_CHECK(isX)                                           \
    do {                                                            \
        do {                                                        \
            i++;                                                    \
            if ( i >= size ) { break; }                             \
            if ( !isX(line[i]) ) {                                  \
                if ( issep(line[i]) ) { goto number_done; }         \
                do {                                                \
                    i++;                                            \
                } while ( i < size && !issep(line[i]) );            \
                TOKEN_INIT(*t, T_ILLEGAL, &line[n], i - n, row, n); \
                goto append_token;                                  \
            }                                                       \
        } while ( true );                                           \
        goto number_done;                                           \
    } while ( 0 );
#define NUMBER_CHECK_0(isX)                                 \
    do {                                                    \
        i += 2;                                             \
        if ( !isX(line[i]) ) {                              \
            TOKEN_INIT(*t, T_ILLEGAL, &line[i], 2, row, i); \
            goto append_token;                              \
        }                                                   \
        NUMBER_CHECK(isX);                                  \
    } while ( 0 )

inline static void tok_print_token(Token* t)
{
    char* r = malloc(sizeof(char) * (t->rep_size + 1));
    ASSERT_PTR(r);

    memcpy(r, t->rep, t->rep_size);
    r[t->rep_size] = '\0';

    printf("\t> %s('%s', len(%zu), [%zu:%zu])\n", token_rep_map[t->type], r,
        t->rep_size, t->row, t->column);
    free(r);
}
inline static void tok_print_tokens(Token_List* tokens)
{
    printf("printing, tokens:\n");
    for ( size_t i = 0; i < tokens->count; i++ ) {
        tok_print_token(tokens->arr[i]);
    }
}
static void tok_process_line(
    const char* line, size_t size, size_t row, Token_List* tokens)
{
    Token* t = malloc(sizeof(Token));
    row++;

    for ( size_t i = 0; i < size; i++ ) {
        if ( isspace(line[i]) ) { continue; }

        if ( line[i] == ';' ) { break; }
        if ( line[i] == ',' ) { continue; }
#define X(tok, s)                                     \
    else if ( line[i] == s )                          \
    {                                                 \
        TOKEN_INIT(*t, T_##tok, &line[i], 1, row, i); \
    }
        SIMBOL
#undef X
        else if ( line[i] == '\'' )
        { // char
            bool scape = false;
            size_t n = i;
            while ( true ) {
                if ( ++i >= size ) {
                    TOKEN_INIT(*t, T_ILLEGAL, &line[n], i - n, row, i);
                    goto append_token;
                }
                if ( scape ) {
                    scape = false;
                    continue;
                }
                if ( line[i] == '\\' ) {
                    scape = true;
                } else if ( line[i] == '\'' ) {
                    break;
                }
            }
            TOKEN_INIT(*t, T_CHAR, &line[n], i - n + 1, row, n);
            // tok_print_token(t);
        }
        else if ( isalpha(line[i]) || line[i] == '_' )
        { // name
            size_t n = i;
            do {
                i++;
                if ( i >= size ) { break; }
            } while ( isalnum(line[i]) || line[i] == '_' );
            TOKEN_INIT(*t, T_NAME, &line[n], i - n, row, n);
            i--;
        }
        else if ( isdigit(line[i]) || line[i] == '-' )
        {
            size_t n = i;
            if ( line[i] == '-' ) {
                i++;
                if ( size <= i ) {
                    TOKEN_INIT(*t, T_ILLEGAL, &line[n], 1, row, i);
                    goto append_token;
                }
            } else if ( line[i] == '0' && i + 2 < size ) {
                if ( line[i + 1] == 'x' ) {        // hex
                    NUMBER_CHECK_0(isxdigit);
                } else if ( line[i + 1] == 'o' ) { // oct
                    NUMBER_CHECK_0(isodigit);
                } else if ( line[i + 1] == 'b' ) { // bin
                    NUMBER_CHECK_0(isbdigit);
                }
            } // dec
            NUMBER_CHECK(isdigit);
number_done:
            TOKEN_INIT(*t, T_NUMBER, &line[n], i - n, row, n);
            i--;
        }
        else
        {
            TOKEN_INIT(*t, T_ILLEGAL, &line[i], 1, row, i);
        }
append_token:
        // tok_print_token(t);
        DA_APPEND_ITEM(*tokens, t);
        t = malloc(sizeof(Token));
        ASSERT_PTR(t);
    }
    free(t);
}

// preprocess
// /////////////////////////////////////////////////////////////////
static return_code_t pre_process_line(const char* line, size_t size)
{
    return RET_SUCCESS;
}

inline static ssize_t pre_find_char(const char* buf, char c, size_t size)
{
    for ( size_t indx = 0; indx < size; indx++ ) {
        if ( buf[indx] == c ) { return indx; }
    }
    return -1;
}

#define BATCH_SIZE 4000
return_code_t pre_read_file(const char* file_name, Token_List* tokens)
{
    int fd = open(file_name, O_RDONLY);

    if ( fd == -1 ) {
        printf(
            "wool:error opening file '%s': %s\n", file_name, strerror(errno));
        return RET_FAILURE;
    }

    ssize_t bytes_read;
    size_t line_capacity = BATCH_SIZE;
    char* line_buf = malloc(line_capacity);

    if ( !line_buf ) {
        printf("wool:error allocating: %s\n", strerror(errno));
        close(fd);
        return RET_FAILURE;
    }

    // TODO: problem is that we are overwritting the buffer each loop, so we
    //       need to make a new one each step and save the previous one to
    //       another place... memory leak idea was good but we need to leak
    //       more
    char* buffer = line_buf;
    size_t size = 0, row = 0;
    while ( (bytes_read = read(fd, buffer, BATCH_SIZE)) > 0 ) {
        size = size + bytes_read;

        ssize_t line_size;
        buffer = line_buf;
        while ( (line_size = pre_find_char(buffer, '\n', size)) != -1 ) {
            buffer[line_size] = '\0';
            // printf("%s\\n lsize: %zu\n", buffer, line_size);
            tok_process_line(buffer, line_size, row++, tokens);

            // updates
            line_size++;
            buffer = &buffer[line_size]; // setup next find
            size = size - line_size;     // update size
        }
        // shift the remaining part + set the top of the buffer
        if ( line_buf != buffer ) {
            line_capacity = BATCH_SIZE;
            line_buf = malloc(
                line_capacity); // HACK: we memory leak intentionally :)
            ASSERT_PTR(line_buf);
            memmove(line_buf, buffer, size);
        }
        buffer = &line_buf[size];

        if ( size + BATCH_SIZE <= line_capacity ) { continue; }
        // we don't have enough capacity to add a new chunk on our line_buf
        line_capacity *= 2;
        char* new_line = realloc(line_buf, line_capacity);
        if ( !new_line ) {
            printf("wool:error rallocating: %s\n", strerror(errno));
            free(line_buf);
            close(fd);
            return RET_FAILURE;
        }
        line_buf = new_line;
    }

    // free(line_buf); // HACK: we leak memory to preserve the rep of tokens
    close(fd);

    if ( bytes_read == -1 ) {
        printf(
            "wool:error reading file '%s': %s\n", file_name, strerror(errno));
        return RET_FAILURE;
    }
    return RET_SUCCESS;
}

int main(int argc, char* argv[])
{
    if ( argc < 3 ) {
        fprintf(stderr, "Usage: %s <file_name>.sp <file_name>.bc\n", argv[0]);
        return 1;
    }

    int out_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if ( out_fd == -1 ) {
        printf("wool:error opening file '%s': %s\n", argv[2], strerror(errno));
        return EXIT_FAILURE;
    }

    Token_List tokens = { .file_name = argv[1], 0 };
    if ( pre_read_file(argv[1], &tokens) == RET_FAILURE ) {
        return EXIT_FAILURE;
    }
    // printf("==============================\n");
    // tok_print_tokens(&tokens);
    // printf("==============================\n");

    if ( lex_process_tokenss(&tokens, out_fd) == RET_FAILURE ) {
        return EXIT_FAILURE;
    }

    close(out_fd);

    return EXIT_SUCCESS;
}
