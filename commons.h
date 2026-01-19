#pragma once
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <stdint.h>

void assert(bool cond, std::string msg);
void assert_with_ip(bool cond, int32_t ip, std::string msg);

std::vector<char> read_file(std::string fname);

struct Header
{
    int32_t st_length;
    int32_t globals_length;
    int32_t pubs_length;
};

struct Pub
{
    int32_t a, b;
};

struct Result
{
    Header header;
    Pub *pubs;
    char *st;
    char *code;

    int32_t code_size;
    std::vector<char> bytes;
};

namespace instr
{
    enum Instr : char
    {
        ADD = 0x01,
        SUB,
        MUL,
        DIV,
        REM,
        LSS,
        LEQ,
        GRE,
        GEQ,
        EQU,
        NEQ,
        AND,
        OR,
        CONST = 0x10,
        STRING,
        SEXP,
        STI,
        STA,
        JMP,
        END,
        RET,
        DROP,
        DUP,
        SWAP,
        ELEM,
        LDG = 0x20,
        LDL,
        LDA,
        LDC,
        LDGR = 0x30,
        LDLR,
        LDAR,
        LDCR,
        STG = 0x40,
        STL,
        STA_,
        STC,
        CJMPZ = 0x50,
        CJMPNZ,
        BEGIN,
        CBEGIN,
        CLOSURE,
        CALLC,
        CALL,
        TAG,
        ARRAY,
        FAIL,
        LINE,
        PATT_eq = 0x60,
        PATT_is_string,
        PATT_is_array,
        PATT_is_sexp,
        PATT_is_ref,
        PATT_is_val,
        PATT_is_fun,
        CALL_Lread = 0x70,
        CALL_Lwrite,
        CALL_Llength,
        CALL_Lstring,
        CALL_Barray
    };

    const char *name(Instr _ins);
}

Result parse_and_validate(std::vector<char> bytes);

#pragma pack(push, 1)
struct Instruction
{
    struct CArg
    {
        enum CArgType : char
        {
            G = 0,
            L = 1,
            A = 2,
            C = 3
        };

        CArgType tag;
        int32_t arg;
    };

    instr::Instr tag;
    int32_t args[2];
    CArg cargs[0];

    size_t get_args_length() const;

    bool is_hex_arg(size_t arg) const;

    bool is_closure() const;

    const char *get_tag_name() const;

    size_t size() const;

    bool operator==(const Instruction &other) const;

    int cmp(const Instruction *other) const;

    int32_t get_popped();

    int32_t get_pushed();

    int32_t get_diff();
};
#pragma pack(pop, 1)

struct Code
{
    char *code;
    int32_t code_size;

    Instruction *get_by_id(int32_t id) const;
    const Instruction *get_by_string_view(std::string_view sw) const;

    int32_t to_id(const Instruction *_ins) const;
    std::string_view to_string_view(const Instruction *_ins, int32_t lookahead) const;

    Instruction *get_next(const Instruction *_ins) const;
    Instruction *get_next_inc(Instruction *_ins) const;

    Code(char *code_, int32_t size) : code(code_), code_size(size) {}
};

std::ostream &operator<<(std::ostream &os, const Instruction &_ins);
