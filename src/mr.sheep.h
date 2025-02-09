#ifndef _MR_SHEEP_H
#define _MR_SHEEP_H

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#define BIT(n)                (1 << n)
#define MR_SHEEP_MEM          BIT(8)
#define _MAGIC_N              0xbaaa
#define MR_SHEEP_MAGIC_NUMBER ((uint16_t)((_MAGIC_N << 8) | (_MAGIC_N >> 8)))

typedef enum {
    ARG_INVALID = 0,
    ARG_LITERAL = BIT(0),   // raw value
    ARG_REFERENCE = BIT(1), // mem. address
    ARG_POINTER = BIT(2),   // indirect mem. address
} Arg_Type;

#define MAX_ARGC 2
// NOTE: L -> [L]iteral; R -> [R]eference; P -> [P]ointer;
//       X: {L,R,P}; Z: {R,P}
#define _ARG_X  (ARG_LITERAL | ARG_REFERENCE | ARG_POINTER)
#define _ARG_Z  (ARG_REFERENCE | ARG_POINTER)
#define _ARR_0  { ARG_INVALID }
#define _ARR_X  { _ARG_X }
#define _ARR_Z  { _ARG_Z }
#define _ARR_ZX { _ARG_Z, _ARG_X }
#define _ARR_L  { ARG_LITERAL }
#define _ARR_R  { ARG_REFERENCE }
#define _ARR_P  { ARG_POINTER }
#define _ARR_RL { ARG_REFERENCE, ARG_LITERAL }
#define _ARR_RR { ARG_REFERENCE, ARG_REFERENCE }
#define _ARR_RP { ARG_REFERENCE, ARG_POINTER }
#define _ARR_PL { ARG_POINTER, ARG_LITERAL }
#define _ARR_PR { ARG_POINTER, ARG_REFERENCE }
#define _ARR_PP { ARG_POINTER, ARG_POINTER }

#define PERM_0(inst)   \
    Z(inst, 0, _ARR_0) \
    X(inst, 0, _ARR_0)
#define PERM_Z(inst)       \
    Z(inst, 1, _ARR_Z)     \
    X(inst##_R, 1, _ARR_R) \
    X(inst##_P, 1, _ARR_P)
#define PERM_X(inst)       \
    Z(inst, 1, _ARR_X)     \
    X(inst##_L, 1, _ARR_L) \
    X(inst##_R, 1, _ARR_R) \
    X(inst##_P, 1, _ARR_P)
#define PERM_ZX(inst)        \
    Z(inst, 2, _ARR_ZX)      \
    X(inst##_RL, 2, _ARR_RL) \
    X(inst##_RR, 2, _ARR_RR) \
    X(inst##_RP, 2, _ARR_RP) \
    X(inst##_PL, 2, _ARR_PL) \
    X(inst##_PR, 2, _ARR_PR) \
    X(inst##_PP, 2, _ARR_PP)

// NOTE, maybe the cmp can be just jeq jgt instead of ops?
#define BYTE_CODES                                                           \
    /*[X(inst_*, argc, {arg_t})]+ Z(ints, argc, {arg_t} */                   \
    PERM_0(NOP)                                                              \
    /* op */                                                                 \
    PERM_ZX(MOV)                                                             \
    /* cmp op */                                                             \
    PERM_ZX(CMP)                                                             \
    PERM_ZX(EQ)                                                              \
    PERM_ZX(NE)                                                              \
    PERM_ZX(LT)                                                              \
    PERM_ZX(GT)                                                              \
    PERM_ZX(GE)                                                              \
    PERM_ZX(LE)                                                              \
    /* bitewise op */                                                        \
    PERM_ZX(BAND)                                                            \
    PERM_ZX(BOR)                                                             \
    PERM_ZX(BXOR)                                                            \
    /* logic op */                                                           \
    PERM_Z(NOT)                                                              \
    PERM_ZX(LAND)                                                            \
    PERM_ZX(LOR)                                                             \
    PERM_ZX(LXOR)                                                            \
    /* arithmetic op */                                                      \
    PERM_Z(INC)                                                              \
    PERM_Z(DEC)                                                              \
    PERM_ZX(ADD)                                                             \
    PERM_ZX(SUB)                                                             \
    PERM_ZX(MUL)                                                             \
    PERM_ZX(DIV)                                                             \
    PERM_ZX(MOD)                                                             \
    PERM_ZX(LSH)                                                             \
    PERM_ZX(RSH)                                                             \
    /* control flow */                                                       \
    PERM_ZX(JIT)                                                             \
    PERM_ZX(JIF)                                                             \
    PERM_ZX(JIN)                                                             \
    PERM_ZX(JIP)                                                             \
    PERM_X(JMP)                                                              \
    /* TODO: figure it the jump direction and set the right jump, maybe this \
     * should be just for litterals */                                       \
    /* PERM_X(JMF) */                                                        \
    /* PERM_X(JMB) */                                                        \
    /* misc */                                                               \
    PERM_X(PRINT_C)                                                          \
    PERM_X(PRINT_D)                                                          \
    PERM_X(PRINT_H)                                                          \
    PERM_0(VMCALL)                                                           \
    PERM_X(EXIT)

#define VMCALLS \
    X(OPEN)     \
    X(READ)     \
    X(WRITE)    \
    X(CLOSE)

#define X(call) VC_##call,
typedef enum { VMCALLS /*,*/ _VC_COUNT } vm_call_t;
#undef X
static_assert(_VC_COUNT <= UINT8_MAX,
    "vm call operations exceed maximum value for uint8_t.");

#define Z(...)
#define X(inst, ...) inst,
typedef enum { BYTE_CODES /*,*/ _BC_COUNT } byte_code_t;
#undef X
static_assert(_BC_COUNT <= UINT8_MAX,
    "bytecode operations exceed maximum value for uint8_t.");

#define X(inst, n, ...) inst##_SIZE = 1 + n,
enum Byte_Code_Size { BYTE_CODES };
#undef X
#undef Z

#endif // !_MR_SHEEP_H
