
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "mr.sheep.h"


#define loop for ( ;; )

#define SKIP \
    { }
#define ERR(msg, ...)                              \
    printf("mr.sheep:error: " msg, ##__VA_ARGS__); \
    return 1;

#define INST(off) code[inst_ptr + (off)]
#define LIT(off)  code[inst_ptr + (off)]
#define REF(off)  ram[code[inst_ptr + (off)]]
#define PTR(off)  ram[ram[code[inst_ptr + (off)]]]

#define GLUE(a, b) a##b

// NOTE: RAW_CASE doesn't add the offset of the readed instruction to the
//       inst_ptr, it's mainly used for the jmp instructions.
#define _RAW_CASE(inst, code) \
    case inst: code; break
#define RAW_CASE(inst, code) _RAW_CASE(inst, code)
#define _CASE(inst, code)              \
    case inst:                         \
        code;                          \
        inst_ptr += GLUE(inst, _SIZE); \
        break
#define CASE(inst, code) _CASE(inst, code)

#define OP(a, op, b) (a(1)) = (a(1))op(b(2))
#define CASE_OP(inst, op)                    \
    CASE(GLUE(inst, _RL), OP(REF, op, LIT)); \
    CASE(GLUE(inst, _RR), OP(REF, op, REF)); \
    CASE(GLUE(inst, _RP), OP(REF, op, PTR)); \
    CASE(GLUE(inst, _PL), OP(PTR, op, LIT)); \
    CASE(GLUE(inst, _PR), OP(PTR, op, REF)); \
    CASE(GLUE(inst, _PP), OP(PTR, op, PTR))

#define IF(cond)     (cond)
#define UNLESS(cond) (!cond)
#define IF_POS(n)    ((int8_t)n >= 0)
#define IF_NEG(n)    ((int8_t)n < 0)
#define _CASE_CJMP(inst, brch, A, B, size)                                    \
    RAW_CASE(inst, inst_ptr += brch(A(1)) ? (int8_t)B(2) + (size - 1) : size)
#define CASE_CJMP(inst, brch)                                          \
    _CASE_CJMP(GLUE(inst, _RL), brch, REF, LIT, GLUE(inst, _RL_SIZE)); \
    _CASE_CJMP(GLUE(inst, _RR), brch, REF, REF, GLUE(inst, _RR_SIZE)); \
    _CASE_CJMP(GLUE(inst, _RP), brch, REF, PTR, GLUE(inst, _RP_SIZE)); \
    _CASE_CJMP(GLUE(inst, _PL), brch, PTR, LIT, GLUE(inst, _PL_SIZE)); \
    _CASE_CJMP(GLUE(inst, _PR), brch, PTR, REF, GLUE(inst, _PR_SIZE)); \
    _CASE_CJMP(GLUE(inst, _PP), brch, PTR, PTR, GLUE(inst, _PP_SIZE))

#define _CASE_UJMP(inst, op, cast, A, size)                 \
    RAW_CASE(inst, inst_ptr op## = (cast)A(1) + (size - 1))
#define CASE_UJMP(inst, op, cast)                                   \
    _CASE_UJMP(GLUE(inst, _L), op, cast, LIT, GLUE(inst, _L_SIZE)); \
    _CASE_UJMP(GLUE(inst, _R), op, cast, REF, GLUE(inst, _R_SIZE)); \
    _CASE_UJMP(GLUE(inst, _P), op, cast, PTR, GLUE(inst, _P_SIZE))

#define CASE_PRINT(inst, format)                  \
    CASE(GLUE(inst, _L), printf(format, LIT(1))); \
    CASE(GLUE(inst, _R), printf(format, REF(1))); \
    CASE(GLUE(inst, _P), printf(format, PTR(1)))

#define _ROR(a, b)                                                       \
    (a(1)) = ((((b(2)) % 8) >> (a(1))) | ((((b(2)) - 8) % 8) >> (a(1))))
#define _ROL(a, b)                                                       \
    (a(1)) = (((a(1)) << ((b(2)) % 8)) | ((a(1)) << (((b(2)) - 8) % 8)))
#define _XOR(a, b) (a(1)) = !((a(1)) && (b(2))) && ((a(1)) || (b(2)))
#define _CMP(a, b)                                                 \
    (a(1)) = (((a(1)) == (b(2))) ? 0 : ((a(1)) < (b(2))) ? -1 : 1)
#define CASE_F(inst, fun)                 \
    CASE(GLUE(inst, _RL), fun(REF, LIT)); \
    CASE(GLUE(inst, _RR), fun(REF, REF)); \
    CASE(GLUE(inst, _RP), fun(REF, PTR)); \
    CASE(GLUE(inst, _PL), fun(PTR, LIT)); \
    CASE(GLUE(inst, _PR), fun(PTR, REF)); \
    CASE(GLUE(inst, _PP), fun(PTR, PTR))

#define CASE_ZX(inst, left, mid, right)                  \
    CASE(GLUE(inst, _RL), left REF(1) mid LIT(2) right); \
    CASE(GLUE(inst, _RR), left REF(1) mid REF(2) right); \
    CASE(GLUE(inst, _RP), left REF(1) mid PTR(2) right); \
    CASE(GLUE(inst, _PL), left PTR(1) mid LIT(2) right); \
    CASE(GLUE(inst, _PR), left PTR(1) mid REF(2) right); \
    CASE(GLUE(inst, _PP), left PTR(1) mid PTR(2) right)
#define CASE_Z(inst, right, left)            \
    CASE(GLUE(inst, _R), right REF(1) left); \
    CASE(GLUE(inst, _P), right PTR(1) left)
#define CASE_X(inst, right, left)            \
    CASE(GLUE(inst, _L), right LIT(1) left); \
    CASE(GLUE(inst, _R), right REF(1) left); \
    CASE(GLUE(inst, _P), right PTR(1) left)
#define RAW_CASE_X(inst, right, left)            \
    RAW_CASE(GLUE(inst, _L), right LIT(1) left); \
    RAW_CASE(GLUE(inst, _R), right REF(1) left); \
    RAW_CASE(GLUE(inst, _P), right PTR(1) left)

#define Z(...)
// #define X(inst, n, args, ...) [inst] = #inst,
#define X(inst, ...) [inst] = #inst,
const char* instruction_size_map[] = { BYTE_CODES };
#undef Z
#undef X

/* VMCALLS:
 *    .name    : the call will modify the value in place
 *    ^name    : the call will dereference the address to access the value
 *    *name    : the call will handle an external process input value
 *
 *    | 0xff    | 0xfe    | 0xfd    | 0xfc    | 0xfb    | 0xfa    |
 *    |---------|---------|---------|---------|---------|---------|
 *    | OPEN    | .fd     | ^path   | mode    | -       | -       |
 *    | READ    | fd      | &buf    | count   | .rvalue | -       |
 *    | WRITE   | fd      | &buf    | count   | .rvalue | -       |
 *    | CLOSE   | fd      | -       | -       | .rvalue | -       |
 *
 * TODO: Potential future VMCALLs to consider:
 *    | DUMP    | fd      | -       | -       | -       | -       |
 *    | FORK    | .ppid   | .cpid   | -       | -       | -       |
 *    | PIPE    | .fd_0   | .fd_1   | -       | .rvalue | -       |
 *    | SLEEP   | mult_1  | mult_2  | -       | .rvalue | *svalue |
 *    | HALT    | -       | -       | -       | .rvalue | *svalue |
 *    | SIGNAL  | pid     | signum  | -       | .rvalue | *svalue |
 *
 * NOTE: we could make all .name args dereferenceable (^name), and the user
 *       could just give the same addres of the param to mimic the .name. But it
 *       is tedious, while this adds flexibility, it will introduce complexity.
 */
#define VC_ARG(n)     ram[MR_SHEEP_MEM - (n) - 1]
#define VC_ARG_PTR(n) &ram[ram[MR_SHEEP_MEM - (n) - 1]]
// #define VC_ARG_PTR(n) &ram[ram[ram[MR_SHEEP_MEM - (n) - 1]]]
inline static uint8_t sheep_vmcall(uint8_t ram[MR_SHEEP_MEM])
{
    // STDIN_FILENO
    switch ( (vm_call_t)VC_ARG(0) ) {
        case VC_OPEN: { // NOTE: we could check the sanity of the args but...
            int fd = open((const char*)VC_ARG_PTR(2), VC_ARG(3));
            // NOTE: you can open 256 - 3 (stdout/err/in) - 1 (error fd=-1=0xff)
            if ( UINT8_MAX - 1 < fd ) { ERR("fd overflow"); }
            VC_ARG(1) = (uint8_t)fd;
            break;
        }
        case VC_READ:
            VC_ARG(4) = read(VC_ARG(1), VC_ARG_PTR(2), VC_ARG(3));
            // printf("\nread %d bytes\n", VC_ARG(4));
            break;
        case VC_WRITE:
            VC_ARG(4) = write(VC_ARG(1), VC_ARG_PTR(2), VC_ARG(3));
            // printf("\nwrite %d bytes\n", VC_ARG(4));
            break;
        case VC_CLOSE: VC_ARG(4) = close(VC_ARG(1)); break;
        default:       ERR("unimplemented vmcall '0x%02x'", VC_ARG(0));
    }
    return EXIT_SUCCESS;
}

static uint8_t sheep_exec(
    uint8_t* restrict code, size_t size, uint8_t ram[MR_SHEEP_MEM])
{
    uint32_t inst_ptr = 0;
    // printf("code %p\n", code);
    // printf("ram  %p\n", ram);
    // printf("[isnt_c|inst_p]\n");
    // NOTE: there's a lot of magic below. it's written this way to be somewhat
    //       readable (maintainability is in the eye of the beholder).
    uint32_t inst_count = 0;
    loop
    {
        // printf("\t|0x%04x:0x%04x|", inst_count++, inst_ptr);
        // printf("\t%s(0x%02x):\t|> 0x%02x(%02d), 0x%02x(%02d)",
        //     instruction_size_map[INST(0)], INST(0), LIT(1), LIT(1), LIT(2),
        //     LIT(2));
        // getchar();
        switch ( INST(0) ) {
            CASE(NOP, SKIP);
            /* op */
            // CASE_OP(MOV,   * 0 +); // HACK: but it works OwO
            CASE_ZX(MOV, , =, );
            /* cmp op */
            CASE_F(CMP, _CMP);
            CASE_OP(EQ, ==);
            CASE_OP(NE, !=);
            CASE_OP(LT, >);
            CASE_OP(LE, >=);
            CASE_OP(GT, <);
            CASE_OP(GE, <=);
            /* bitewise op */
            CASE_OP(BAND, &);
            CASE_OP(BOR, |);
            CASE_OP(BXOR, ^);
            /* logic op */
            CASE_OP(LAND, &);
            CASE_OP(LOR, |);
            CASE_F(LXOR, _XOR);
            /* arithmetic op */
            CASE_Z(INC, ++, );
            CASE_Z(DEC, --, );
            CASE_OP(ADD, +);
            CASE_OP(SUB, -);
            CASE_OP(MUL, *);
            CASE_OP(DIV, /);
            CASE_OP(MOD, %);
            CASE_OP(SHL, <<);
            CASE_OP(SHR, >>);
            CASE_F(ROL, _ROL);
            CASE_F(ROR, _ROR);
            /* control flow (they are relative jumps to the intruction ptr) */
            CASE_CJMP(JNZ, IF);        // cond. jmp if not zero
            CASE_CJMP(JZ, UNLESS);     // cond. jmp if zero
            CASE_CJMP(JNS, IF_POS);    // cond. jmp if not signed (positive)
            CASE_CJMP(JS, IF_NEG);     // cond. jmp if signed (negative)
            CASE_UJMP(JMP, +, int8_t); // uncond. jmp
            // TODO: wool is not prepared for this instruction...
            // CASE_UJMP(JMF, +, uint8_t); // uncond. extra pos jmp
            // CASE_UJMP(JMB, -, uint8_t); // uncond. extra neg jmp
            /* misc */
            // CASE_PRINT(PRINT_D, "%d");
            // CASE_PRINT(PRINT_H, "%02x");
            // CASE_PRINT(PRINT_C, "%c");
            RAW_CASE_X(EXIT, return, );
            CASE(VMCALL, sheep_vmcall(ram));
            default: ERR("unimplemented bytecode '0x%02x'", INST(0));
        }
        if ( inst_ptr < 0 || size <= inst_ptr ) {
            // printf("%d\n", inst_ptr);
            printf("\nmr.sheep:error: out of bounds\n");
            exit(1);
        }
    }
    return -1;
}

#define INIT_BUFFER_SIZE 1024
size_t sheep_load_file(const char* file_name, uint8_t** buffer)
{
    FILE* file = fopen(file_name, "rb");
    if ( !file ) {
        perror("mr.sheep: failed to open file");
        return 0;
    }

    // seek to the end of the file to get the total size
    size_t size = 0;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *buffer = (uint8_t*)malloc(size);
    if ( *buffer == NULL ) {
        perror("Failed to allocate memory");
        fclose(file);
        return 0;
    }

    size_t bytes_read = fread(*buffer, 1, size, file);
    if ( bytes_read != size ) {
        perror("mr.sheep: failed to read the full file");
        free(*buffer);
        fclose(file);
        return 0;
    }
    // printf("mr.sheep: successfully loaded '%s' of %zuB\n", file_name, size);

    fclose(file);

    return size;
}

// void dump_baa_kw_reps()
// {
// #define X(...)
// #define Z(inst, ...) printf("%-8s\n", #inst);
//     BYTE_CODES;
// #undef Z
// #undef X
// }
// void dump_baa_bc_reps()
// {
// #define Z(...)
// #define X(inst, ...) printf("0x%x %-8s\n", inst, #inst);
//     BYTE_CODES;
// #undef X
// #undef Z
// }

int main(int argc, char* argv[])
{
    if ( argc < 2 ) {
        fprintf(stderr, "Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    uint8_t* code;
    size_t size = sheep_load_file(argv[1], &code);

    // if ( size == -1 ) { return 1; }
    if ( size < 2 ) { return 1; }
    if ( MR_SHEEP_MAGIC_NUMBER != *(uint16_t*)code ) {
        printf("mr.sheep: magic number not found\n");
        return 1;
    }

    uint8_t ram[MR_SHEEP_MEM] = { 0 };

    // uint8_t exit_code = sheep_exec(&buf[MR_SHEEP_MEM], buf);
    uint8_t exit_code = sheep_exec(code + 2, size - 2, ram);
    // printf("\nmr.sheep: program exited with code: %d\n", exit_code);

    free(code);

    return exit_code;
}
