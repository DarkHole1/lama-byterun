#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <span>
#include "runtime/gc.h"

extern "C" void *Lstring(aint *args);
extern "C" aint LtagHash(char *c);
extern "C" char *de_hash(aint n);

void assert(bool cond, std::string msg)
{
    if (!cond)
    {
        std::cout << msg << "\n";
        exit(1);
    }
}

void assert_with_ip(bool cond, int32_t ip, std::string msg)
{
    if (!cond)
    {
        std::cout << "[ip=" << std::hex << ip << std::dec << "] " << msg << "\n";
        exit(1);
    }
}

[[noreturn]] void unknown_instruction(int32_t ip, long name)
{
    std::cout << "[ip=" << std::hex << ip << std::dec << "] " << "Unknown instruction: " << name;
    exit(1);
}

[[noreturn]] void not_implemented(int32_t ip, std::string name)
{
    std::cout << "[ip=" << std::hex << ip << std::dec << "] " << "Instruction not implemented: " << name;
    exit(1);
}

[[noreturn]] void unreachable(int32_t ip)
{
    std::cout << "[ip=" << std::hex << ip << std::dec << "] " << "Unreachable code executed";
    exit(1);
}

std::vector<char> read_file(std::string fname)
{
    std::ifstream file;
    file.open(fname, std::ios_base::binary);

    file.seekg(0, std::ios_base::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios_base::beg);

    assert(length > 0, "File not exists or empty");

    std::vector<char> buffer;
    buffer.resize(length);
    file.read(&buffer[0], length);

    return buffer;
}

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

Result parse_and_validate(std::vector<char> bytes)
{
    Result result = {};
    Header header = {};

    const size_t header_size = sizeof(Header);

    assert(bytes.size() >= header_size, "File is too small");

    std::memcpy(&header, &bytes[0], sizeof(header));

    assert(header.st_length >= 0 && header.globals_length >= 0 && header.pubs_length >= 0,
           "Invalid header");

    const size_t file_size = header_size + header.pubs_length * sizeof(Pub) + header.st_length;

    assert(bytes.size() > file_size, "File is too small or header is invalid");

    result.header = header;

    const size_t pubs_offset = header_size;
    result.pubs = reinterpret_cast<Pub *>(&bytes[pubs_offset]);

    for (int32_t i = 0; i < header.pubs_length; i++)
    {
        Pub pub = result.pubs[i];
        assert(pub.a >= 0 && pub.b >= 0, "Unexpected negative value in pubs table");
    }

    const size_t st_offset = pubs_offset + header.pubs_length * sizeof(Pub);
    result.st = &bytes[st_offset];

    const size_t code_offset = st_offset + header.st_length;
    result.code_size = bytes.size() - code_offset;
    assert(result.code_size > 0, "Empty code section");
    result.code = &bytes[code_offset];

    result.bytes = std::move(bytes);

    return result;
}

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

    const char *name(Instr _ins)
    {
        switch (_ins)
        {
        case ADD:
            return "ADD";
        case SUB:
            return "SUB";
        case MUL:
            return "MUL";
        case DIV:
            return "DIV";
        case REM:
            return "REM";
        case LSS:
            return "LSS";
        case LEQ:
            return "LEQ";
        case GRE:
            return "GRE";
        case GEQ:
            return "GEQ";
        case EQU:
            return "EQU";
        case NEQ:
            return "NEQ";
        case AND:
            return "AND";
        case OR:
            return "OR";
        case CONST:
            return "CONST";
        case STRING:
            return "STRING";
        case SEXP:
            return "SEXP";
        case STI:
            return "STI";
        case STA:
            return "STA";
        case JMP:
            return "JMP";
        case END:
            return "END";
        case RET:
            return "RET";
        case DROP:
            return "DROP";
        case DUP:
            return "DUP";
        case SWAP:
            return "SWAP";
        case ELEM:
            return "ELEM";
        case LDG:
            return "LDG";
        case LDL:
            return "LDL";
        case LDA:
            return "LDA";
        case LDC:
            return "LDC";
        case LDGR:
            return "LDGR";
        case LDLR:
            return "LDLR";
        case LDAR:
            return "LDAR";
        case LDCR:
            return "LDCR";
        case STG:
            return "STG";
        case STL:
            return "STL";
        case STA_:
            return "STA_";
        case STC:
            return "STC";
        case CJMPZ:
            return "CJMPZ";
        case CJMPNZ:
            return "CJMPNZ";
        case BEGIN:
            return "BEGIN";
        case CBEGIN:
            return "CBEGIN";
        case CLOSURE:
            return "CLOSURE";
        case CALLC:
            return "CALLC";
        case CALL:
            return "CALL";
        case TAG:
            return "TAG";
        case ARRAY:
            return "ARRAY";
        case FAIL:
            return "FAIL";
        case LINE:
            return "LINE";
        case PATT_eq:
            return "PATT_eq";
        case PATT_is_string:
            return "PATT_is_string";
        case PATT_is_array:
            return "PATT_is_array";
        case PATT_is_sexp:
            return "PATT_is_sexp";
        case PATT_is_ref:
            return "PATT_is_ref";
        case PATT_is_val:
            return "PATT_is_val";
        case PATT_is_fun:
            return "PATT_is_fun";
        case CALL_Lread:
            return "CALL_Lread";
        case CALL_Lwrite:
            return "CALL_Lwrite";
        case CALL_Llength:
            return "CALL_Llength";
        case CALL_Lstring:
            return "CALL_Lstring";
        case CALL_Barray:
            return "CALL_Barray";
        default:
            return "UNK";
        }
    };
}

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

    size_t get_args_length() const
    {
        switch (tag)
        {
        case instr::ADD:
        case instr::SUB:
        case instr::MUL:
        case instr::DIV:
        case instr::REM:
        case instr::LSS:
        case instr::LEQ:
        case instr::GRE:
        case instr::GEQ:
        case instr::EQU:
        case instr::NEQ:
        case instr::AND:
        case instr::OR:
        {
            return 0;
        }
        case instr::CONST:
        case instr::STRING:
        {
            return 1;
        }
        case instr::SEXP:
        {
            return 2;
        }
        case instr::STI:
        case instr::STA:
        {
            return 0;
        }
        case instr::JMP:
        {
            return 1;
        }
        case instr::END:
        case instr::RET:
        case instr::DROP:
        case instr::DUP:
        case instr::SWAP:
        case instr::ELEM:
        {
            return 0;
        }
        case instr::LDG:
        case instr::LDL:
        case instr::LDA:
        case instr::LDC:
        case instr::LDGR:
        case instr::LDLR:
        case instr::LDAR:
        case instr::LDCR:
        case instr::STG:
        case instr::STL:
        case instr::STA_:
        case instr::STC:
        case instr::CJMPZ:
        case instr::CJMPNZ:
        {
            return 1;
        }
        case instr::BEGIN:
        case instr::CBEGIN:
        {
            return 2;
        }
        case instr::CLOSURE:
        {
            // Special case
            return 2;
        }
        case instr::CALLC:
        {
            return 1;
        }
        case instr::CALL:
        case instr::TAG:
        {
            return 2;
        }
        case instr::ARRAY:
        {
            return 1;
        }
        case instr::FAIL:
        {
            return 2;
        }
        case instr::LINE:
        {
            return 1;
        }
        case instr::PATT_eq:
        case instr::PATT_is_string:
        case instr::PATT_is_array:
        case instr::PATT_is_sexp:
        case instr::PATT_is_ref:
        case instr::PATT_is_val:
        case instr::PATT_is_fun:
        case instr::CALL_Lread:
        case instr::CALL_Lwrite:
        case instr::CALL_Llength:
        case instr::CALL_Lstring:
        {
            return 0;
        }
        case instr::CALL_Barray:
        {
            return 1;
        }
        default:
        {
            return 0;
        }
        }
    }

    bool is_hex_arg(size_t arg) const
    {
        switch (tag)
        {
        case instr::STRING:
        case instr::SEXP:
        case instr::JMP:
        case instr::CJMPZ:
        case instr::CJMPNZ:
        case instr::CLOSURE:
        case instr::CALL:
        case instr::TAG:
        {
            return arg == 0;
        }
        default:
        {
            return false;
        }
        }
    }

    bool is_closure() const
    {
        switch (tag)
        {
        case instr::CLOSURE:
            return true;

        default:
            return false;
        }
    }

    const char *get_tag_name() const
    {
        return instr::name(tag);
    }

    size_t size() const
    {
        size_t closure_args = 0;
        if (is_closure())
        {
            closure_args = args[1] * (sizeof(char) + sizeof(int32_t));
        }
        return sizeof(char) + sizeof(int32_t) * get_args_length() + closure_args;
    }

    bool operator==(const Instruction &other) const
    {
        return memcmp(this, &other, size()) == 0;
    }

    int32_t get_popped()
    {
        switch (tag)
        {
        case instr::ADD:
        case instr::SUB:
        case instr::MUL:
        case instr::DIV:
        case instr::REM:
        case instr::LSS:
        case instr::LEQ:
        case instr::GRE:
        case instr::GEQ:
        case instr::EQU:
        case instr::NEQ:
        case instr::AND:
        case instr::OR:
            return 2;
        case instr::CONST:
        case instr::STRING:
            return 0;
        case instr::SEXP:
            return args[1];
        case instr::STI:
            return 2;
        case instr::STA:
            return 3;
        case instr::JMP:
            return 0;
        case instr::END:
        case instr::RET:
        case instr::DROP:
        case instr::DUP:
            return 1;
        case instr::SWAP:
        case instr::ELEM:
            return 2;
        case instr::LDG:
        case instr::LDL:
        case instr::LDA:
        case instr::LDC:
        case instr::LDGR:
        case instr::LDLR:
        case instr::LDAR:
        case instr::LDCR:
            return 0;
        case instr::STG:
        case instr::STL:
        case instr::STA_:
        case instr::STC:
        case instr::CJMPZ:
        case instr::CJMPNZ:
            return 1;
        case instr::BEGIN:
        case instr::CBEGIN:
        case instr::CLOSURE:
            return 0;
        case instr::CALLC:
            return args[0] + 1;
        case instr::CALL:
            return args[1];
        case instr::TAG:
        case instr::ARRAY:
        case instr::FAIL:
            return 1;
        case instr::LINE:
            return 0;
        case instr::PATT_eq:
            return 2;
        case instr::PATT_is_string:
        case instr::PATT_is_array:
        case instr::PATT_is_sexp:
        case instr::PATT_is_ref:
        case instr::PATT_is_val:
        case instr::PATT_is_fun:
            return 1;
        case instr::CALL_Lread:
            return 0;
        case instr::CALL_Lwrite:
        case instr::CALL_Llength:
        case instr::CALL_Lstring:
            return 1;
        case instr::CALL_Barray:
            return args[0];
        default:
            return 0;
        }
    }

    int32_t get_pushed()
    {
        switch (tag)
        {
        case instr::ADD:
        case instr::SUB:
        case instr::MUL:
        case instr::DIV:
        case instr::REM:
        case instr::LSS:
        case instr::LEQ:
        case instr::GRE:
        case instr::GEQ:
        case instr::EQU:
        case instr::NEQ:
        case instr::AND:
        case instr::OR:
        case instr::CONST:
        case instr::STRING:
        case instr::SEXP:
        case instr::STI:
            return 1;
        case instr::STA:
            return 1;
        case instr::JMP:
            return 0;
        case instr::END:
        case instr::RET:
            return 1;
        case instr::DROP:
            return 0;
        case instr::DUP:
        case instr::SWAP:
            return 2;
        case instr::ELEM:
        case instr::LDG:
        case instr::LDL:
        case instr::LDA:
        case instr::LDC:
        case instr::LDGR:
        case instr::LDLR:
        case instr::LDAR:
        case instr::LDCR:
        case instr::STG:
        case instr::STL:
        case instr::STA_:
        case instr::STC:
            return 1;
        case instr::CJMPZ:
        case instr::CJMPNZ:
        case instr::BEGIN:
        case instr::CBEGIN:
            return 0;
        case instr::CLOSURE:
        case instr::CALLC:
        case instr::CALL:
        case instr::TAG:
        case instr::ARRAY:
            return 1;
        case instr::FAIL:
        case instr::LINE:
            return 0;
        case instr::PATT_eq:
        case instr::PATT_is_string:
        case instr::PATT_is_array:
        case instr::PATT_is_sexp:
        case instr::PATT_is_ref:
        case instr::PATT_is_val:
        case instr::PATT_is_fun:
        case instr::CALL_Lread:
        case instr::CALL_Lwrite:
        case instr::CALL_Llength:
        case instr::CALL_Lstring:
        case instr::CALL_Barray:
            return 1;
        default:
            return 0;
        }
    }

    int32_t get_diff()
    {
        return get_pushed() - get_popped();
    }
};
#pragma pack(pop, 1)

std::ostream &operator<<(std::ostream &os, const Instruction &_ins)
{
    os << _ins.get_tag_name();
    for (size_t i = 0; i < _ins.get_args_length(); i++)
    {
        if (_ins.is_hex_arg(i))
        {
            os << std::hex << " 0x" << _ins.args[i] << std::dec;
        }
        else
        {
            os << " " << _ins.args[i];
        }
    }
    if (_ins.is_closure())
    {
        for (int32_t i = 0; i < _ins.args[1]; i++)
        {
            auto carg = _ins.cargs[i];
            switch (carg.tag)
            {
            case carg.G:
                os << " G(";
                break;
            case carg.L:
                os << " L(";
                break;
            case carg.A:
                os << " A(";
                break;
            case carg.C:
                os << " C(";
                break;
            }
            os << carg.arg << ")";
        }
    }
    return os;
}

void dump_bytecode(char *code, int32_t code_size)
{
    for (int32_t ip = 0; ip < code_size;)
    {
        Instruction *cur = reinterpret_cast<Instruction *>(code + ip);
        if (ip + cur->size() > code_size)
        {
            // We reached end with trailing bytes
            return;
        }

        std::cout << "[ip=0x" << std::hex << ip << std::dec << "] " << *cur << "\n";

        ip += cur->size();
    }
}

extern "C" void *__gc_stack_top;
extern "C" void *__gc_stack_bottom;

struct SFrame
{
    size_t prev_ip;
    size_t prev_base;
    size_t prev_args;
    size_t prev_locals;
    size_t prev_captured;
    bool is_closure;
};

void stringify(std::ostream &s, aint v)
{
    if (UNBOXED(v))
    {
        s << UNBOX(v);
    }
    else
    {
        switch (get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))))
        {
        case ARRAY:
        {

            auto n = LEN(TO_DATA(v)->data_header);
            s << "[";

            for (size_t i = 0; i < n; ++i)
            {
                if (i > 0)
                {
                    s << ", ";
                }

                stringify(s, reinterpret_cast<aint *>(v)[i]);
            }

            s << "]";

            break;
        }

        case CLOSURE:
            s << "<function>";
            break;

        case STRING:
            s << '"' << TO_DATA(v)->contents << '"';
            break;

        case SEXP:
            s << de_hash(TO_SEXP(v)->tag);
            auto n = LEN(TO_DATA(v)->data_header);

            if (n > 0)
            {
                s << " (";

                for (size_t i = 0; i < n; ++i)
                {
                    if (i > 0)
                    {
                        s << ", ";
                    }

                    stringify(s, reinterpret_cast<aint *>(TO_SEXP(v)->contents)[i]);
                }

                s << ")";
            }

            break;
        }
    }
}

struct Interpreter
{
    // Approx ~4 MiB
    const size_t STACK_MAX_SIZE = 1024 * 1024;
    const size_t CALL_STACK_MAX_SIZE = 2048;

    Result result;
    std::vector<aint> stack;
    std::vector<SFrame> frames;

    size_t ip;
    size_t base;
    size_t args;
    size_t locals;
    size_t captured;
    bool is_closure;

    int32_t get_code_size()
    {
        return result.code_size;
    }

    int32_t read_i32()
    {
        assert(get_code_size() >= ip + sizeof(int32_t), "Unexpected file end while reading instruction arg");

        int32_t res;
        std::memcpy(&res, &result.code[ip], sizeof(int32_t));
        ip += 4;
        return res;
    }

    aint pop()
    {
        assert_with_ip(__gc_stack_bottom != __gc_stack_top, ip, "Failed to pop value: stack empty");
        __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_bottom) - 1;
        return *reinterpret_cast<aint *>(__gc_stack_bottom);
    }

    void push(aint v)
    {
        assert_with_ip(reinterpret_cast<aint *>(__gc_stack_bottom) < stack.data() + stack.size(), ip, "Failed to push value: stack overflow");
        *reinterpret_cast<aint *>(__gc_stack_bottom) = v;
        __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_bottom) + 1;
    }

    int interpret()
    {
        auto code = result.code;

        frames.push_back(SFrame{
            .prev_ip = 0,
            .prev_base = 0,
            .prev_args = 0,
            .prev_locals = 0,
            .prev_captured = 0,
            .is_closure = false,
        });

        while (true)
        {
            assert(ip < get_code_size() && ip >= 0, "Tried to read instruction outside of bytecode");
            char instr = code[ip];
            ip++;
            switch (instr)
            {
            case instr::ADD:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) + UNBOX(rhs)));
                break;
            }
            case instr::SUB:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) - UNBOX(rhs)));
                break;
            }
            case instr::MUL:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) * UNBOX(rhs)));
                break;
            }
            case instr::DIV:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                assert_with_ip(UNBOX(rhs) != 0, ip, "Division by zero");
                push(BOX(UNBOX(lhs) / UNBOX(rhs)));
                break;
            }
            case instr::REM:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                assert_with_ip(UNBOX(rhs) != 0, ip, "Remainder zero");
                push(BOX(UNBOX(lhs) % UNBOX(rhs)));
                break;
            }
            case instr::LSS:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) < UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::LEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) <= UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::GRE:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) > UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::GEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) >= UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::EQU:
            {
                auto rhs = pop();
                auto lhs = pop();
                push(BOX(lhs == rhs ? 1 : 0));
                break;
            }
            case instr::NEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::AND:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != 0 && UNBOX(rhs) != 0) ? 1 : 0));
                break;
            }
            case instr::OR:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != 0 || UNBOX(rhs) != 0) ? 1 : 0));
                break;
            }
            case instr::CONST:
            {
                auto v = read_i32();
                push(BOX(v));
                break;
            }
            case instr::STRING:
            {
                auto v = read_i32();
                assert_with_ip(v < result.header.st_length, ip, "String index out of table");
                std::string_view sv = &result.st[v];
                void *v_ = get_object_content_ptr(alloc_string(sv.length()));
                push(reinterpret_cast<aint>(v_));
                strcpy(TO_DATA(v_)->contents, sv.data());
                break;
            }
            case instr::SEXP:
            {
                int32_t s = read_i32();
                int32_t n = read_i32();
                assert_with_ip(s < result.header.st_length, ip, "String index out of table");
                char *tag = &result.st[s];
                auto *v = get_object_content_ptr(alloc_sexp(n));

                TO_SEXP(v)->tag = UNBOX(LtagHash(tag));

                for (int32_t i = n - 1; i >= 0; i--)
                {
                    auto vv = pop();
                    auto sexp_ = TO_SEXP(v);
                    reinterpret_cast<auint *>(sexp_->contents)[i] = vv;
                }

                push(reinterpret_cast<aint>(v));
                break;
            }
            case instr::STI:
            {
                not_implemented(ip, "STI");
            }
            case instr::STA:
            {
                auto v = pop();
                auto idx_v = pop();
                auto agg = pop();

                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");
                assert_with_ip(UNBOXED(idx_v), ip, "Index not integer");

                auto idx = UNBOX(idx_v);
                auto agg_ = TO_DATA(agg);
                aint len = static_cast<aint>(LEN(TO_DATA(agg)->data_header));
                assert_with_ip(idx >= 0 && idx < len, ip, "Index outside of range");
                switch (tag)
                {
                case ARRAY:
                    reinterpret_cast<aint *>(agg_->contents)[idx] = v;
                    break;
                case STRING:
                    assert_with_ip(UNBOXED(v) && v >= 0 && v <= 0xff, ip, "Can't assign value to string");
                    agg_->contents[idx] = UNBOX(v);
                    break;
                case SEXP:
                    TO_SEXP(agg_)->contents[idx] = v;
                    break;
                default:
                    unreachable(ip);
                }
                push(v);
                break;
            }
            case instr::JMP:
            {
                int32_t offset = read_i32();
                assert_with_ip(offset >= 0 && offset < get_code_size(), ip, "Tried to jump outside of code");
                ip = offset;
                break;
            }
            case instr::END:
            case instr::RET:
            {
                SFrame f = frames.back();
                aint v = pop();
                __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_top) + base - args - (is_closure ? 1 : 0);

                push(v);

                if (f.prev_ip == 0)
                {
                    return 0;
                }

                ip = f.prev_ip;
                base = f.prev_base;
                args = f.prev_args;
                locals = f.prev_locals;
                captured = f.prev_captured;
                is_closure = f.is_closure;
                frames.pop_back();

                break;
            }
            case instr::DROP:
            {
                pop();
                break;
            }
            case instr::DUP:
            {
                aint v = pop();
                push(v);
                push(v);

                break;
            }
            case instr::SWAP:
            {
                aint top = pop();
                aint second = pop();

                push(top);
                push(second);

                break;
            }
            case instr::ELEM:
            {
                aint idx_v = pop();
                aint agg = pop();

                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");
                assert_with_ip(UNBOXED(idx_v), ip, "Index not integer");

                auto idx = UNBOX(idx_v);
                auto agg_ = TO_DATA(agg);
                aint len = static_cast<aint>(LEN(TO_DATA(agg)->data_header));
                if (idx < 0 || idx >= len)
                {
                    std::cout << "Index outside of range\n";
                    exit(1);
                }
                switch (tag)
                {
                case ARRAY:
                    push(reinterpret_cast<aint *>(agg_->contents)[idx]);
                    break;
                case STRING:
                    push(BOX(agg_->contents[idx]));
                    break;
                case SEXP:
                {
                    auto sexp_ = TO_SEXP(agg);
                    push(reinterpret_cast<auint *>(sexp_->contents)[idx]);
                    break;
                }
                default:
                    unreachable(ip);
                }

                break;
            }
            case instr::LDG:
            {
                auto g = read_i32();
                assert_with_ip(g >= 0 && g < result.header.globals_length, ip, "Tried to get invalid global");
                push(stack[g]);

                break;
            }
            case instr::LDL:
            {
                auto l = read_i32();
                assert_with_ip(l >= 0 && l < locals, ip, "Tried to get invalid local");
                push(stack[base + l]);
                break;
            }
            case instr::LDA:
            {
                auto a = read_i32();
                assert_with_ip(a >= 0 && a < args, ip, "Tried to get invalid arg");
                push(stack[base - args + a]);

                break;
            }
            case instr::LDC:
            {
                auto c = read_i32();
                assert_with_ip(is_closure, ip, "Tried to captured variable in non closure context");
                auto cc = stack[base - args - 1];
                assert_with_ip(c >= 0 && c < captured, ip, "Tried to get invalid captured");
                auto cc_ = (aint *)cc;
                push(cc_[c + 1]);
                break;
            }

            case instr::LDGR:
            {
                not_implemented(ip, "LDGR");
            }
            case instr::LDLR:
            {
                not_implemented(ip, "LDLR");
            }
            case instr::LDAR:
            {
                not_implemented(ip, "LDAR");
            }
            case instr::LDCR:
            {
                not_implemented(ip, "LDCR");
            }
            case instr::STG:
            {
                auto g = read_i32();
                auto v = pop();

                assert_with_ip(g >= 0 && g < result.header.globals_length, ip, "Tried to get invalid global");
                stack[g] = v;
                push(v);

                break;
            }
            case instr::STL:
            {
                auto l = read_i32();
                auto v = pop();

                assert_with_ip(l >= 0 && l < locals, ip, "Tried to get invalid local");
                stack[base + l] = v;
                push(v);

                break;
            }
            case instr::STA_:
            {
                auto a = read_i32();
                auto v = pop();

                assert_with_ip(a >= 0 && a < args, ip, "Tried to get invalid argument");
                stack[base - args + a] = v;
                push(v);

                break;
            }
            case instr::STC:
            {
                auto c = read_i32();
                assert_with_ip(is_closure, ip, "Tried to get captured variable in non closure context");
                auto cc = stack[base - args - 1];
                assert_with_ip(c >= 0 && c < captured, ip, "Tried to get invalid captured");
                auto cc_ = (aint *)cc;
                auto v = pop();

                cc_[c + 1] = v;
                push(v);

                break;
            }
            case instr::CJMPZ:
            {
                auto l = read_i32();
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                if (UNBOX(v) == 0)
                {
                    assert_with_ip(l >= 0 && l < get_code_size(), ip, "Tried to jump outside of code");
                    ip = l;
                }

                break;
            }
            case instr::CJMPNZ:
            {
                auto l = read_i32();
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                if (UNBOX(v) != 0)
                {
                    assert_with_ip(l >= 0 && l < get_code_size(), ip, "Tried to jump outside of code");
                    ip = l;
                }

                break;
            }
            case instr::BEGIN:
            case instr::CBEGIN:
            {
                read_i32();
                auto n = read_i32();
                auto locs = n & 0xFFFF;

                locals = locs;

                for (int32_t i = 0; i < locs; i++)
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::CLOSURE:
            {
                auto l = read_i32();
                auto n = read_i32();
                auto *closure = get_object_content_ptr(alloc_closure(n + 1));
                push(reinterpret_cast<aint>(closure));
                static_cast<aint *>(closure)[0] = l;

                assert_with_ip(l >= 0 && l < get_code_size(), ip, "Try to create closure outside of code");

                for (int32_t i = 0; i < n; ++i)
                {
                    auto kind = static_cast<char>(code[this->ip]);
                    ip++;
                    auto m = read_i32();

                    switch (kind)
                    {
                    case 0:
                        static_cast<aint *>(closure)[i + 1] = stack[m];
                        break;

                    case 1:
                        static_cast<aint *>(closure)[i + 1] = stack[base + m];
                        break;

                    case 2:
                        static_cast<aint *>(closure)[i + 1] = stack[base - args + m];
                        break;

                    case 3:
                        static_cast<aint *>(closure)[i + 1] = ((aint *)stack[base - args - 1])[m + 1];
                        break;

                    default:
                        std::cout << "Not implemented\n";
                        exit(1);
                    }
                }
                break;
            }
            case instr::CALLC:
            {
                auto n = read_i32();
                auto closure = *(reinterpret_cast<aint *>(__gc_stack_bottom) - n - 1);

                assert_with_ip(!UNBOXED(closure) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(closure))) == CLOSURE,
                               ip, "Try to call not closure");

                assert_with_ip(frames.size() < CALL_STACK_MAX_SIZE, ip, "Cant call closure: call stack overflow");

                frames.push_back(SFrame{
                    .prev_ip = ip,
                    .prev_base = base,
                    .prev_args = args,
                    .prev_locals = locals,
                    .prev_captured = captured,
                    .is_closure = is_closure,
                });

                ip = reinterpret_cast<aint *>(closure)[0];
                is_closure = true;
                base = reinterpret_cast<aint *>(__gc_stack_bottom) - reinterpret_cast<aint *>(__gc_stack_top);
                args = n;
                locals = 0;
                captured = LEN(TO_DATA(closure)->data_header) - 1;

                break;
            }
            case instr::CALL:
            {
                auto l = read_i32();
                auto n = read_i32();

                assert_with_ip(frames.size() < CALL_STACK_MAX_SIZE, ip, "Cant call function: call stack overflow");

                frames.push_back(SFrame{
                    .prev_ip = ip,
                    .prev_base = base,
                    .prev_args = args,
                    .prev_locals = locals,
                    .prev_captured = captured,
                    .is_closure = is_closure,
                });

                ip = l;
                is_closure = false;
                base = reinterpret_cast<aint *>(__gc_stack_bottom) - reinterpret_cast<aint *>(__gc_stack_top);
                args = n;
                captured = 0;
                locals = 0;

                break;
            }
            case instr::TAG:
            {
                auto s = read_i32();
                auto n = read_i32();
                auto v = pop();

                assert_with_ip(s < result.header.st_length, ip, "String index out of table");
                char *exp = &result.st[s];
                aint exp_ = UNBOX(LtagHash(exp));

                if (!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == SEXP)
                {
                    auto sexp_ = TO_SEXP(v);
                    auto tag = sexp_->tag;

                    push((LEN(sexp_->data_header) == n && exp_ == tag) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::ARRAY:
            {
                auto n = read_i32();
                auto v = pop();

                if (!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == ARRAY)
                {
                    auto data_ = TO_DATA(v);
                    push((LEN(data_->data_header) == n) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::FAIL:
            {
                auto ln = read_i32();
                auto col = read_i32();
                pop();

                std::cout << "Match failure at " << ln << ":" << col << "\n";
                exit(1);
            }
            case instr::LINE:
            {
                read_i32();
                break;
            }
            case instr::PATT_eq:
            {
                auto rhs = pop();
                auto lhs = pop();

                if (!UNBOXED(lhs) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(lhs))) == STRING &&
                    !UNBOXED(rhs) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(rhs))) == STRING)
                {
                    push((strcmp(TO_DATA(lhs)->contents, TO_DATA(rhs)->contents) == 0) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::PATT_is_string:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == STRING)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_array:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == ARRAY)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_sexp:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == SEXP)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_ref:
            {
                auto v = pop();

                push((!UNBOXED(v))
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_val:
            {
                auto v = pop();

                push((UNBOXED(v))
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_fun:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == CLOSURE)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::CALL_Lread:
            {
                aint v = 0;
                std::cout << " > " << std::flush;
                std::cin >> v;

                push(BOX(v));

                break;
            }
            case instr::CALL_Lwrite:
            {
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                std::cout << UNBOX(v) << "\n";

                push(BOX(0));

                break;
            }
            case instr::CALL_Llength:
            {
                auto agg = pop();
                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");

                push(BOX(LEN(TO_DATA(agg)->data_header)));
                break;
            }
            case instr::CALL_Lstring:
            {
                auto v = pop();

                push(reinterpret_cast<aint>(Lstring(&v)));

                // std::ostringstream s_;
                // stringify(s_, v);

                // std::string s = s_.str();
                // auto *r = get_object_content_ptr(alloc_string(s.size()));

                // push(reinterpret_cast<aint>(r));
                // strcpy(TO_DATA(r)->contents, s.data());

                break;
            }
            case instr::CALL_Barray:
            {
                auto n = read_i32();
                auto *v = get_object_content_ptr(alloc_array(n));

                for (int32_t i = n - 1; i >= 0; i--)
                {
                    static_cast<aint *>(v)[i] = pop();
                }

                push(reinterpret_cast<aint>(v));
                break;
            }

            default:
                assert(false, "Unsupported instruction");
                break;
            }
        }
        return 0;
    }

    Interpreter(Result result_)
    {
        // Globals + dummy main arguments
        const size_t base_ = result_.header.globals_length + 2;

        result = result_;
        stack.resize(STACK_MAX_SIZE, BOX(0));
        ip = 0;
        base = base_;
        args = 2;
        is_closure = false;

        frames.reserve(CALL_STACK_MAX_SIZE);

        __gc_stack_top = stack.data();
        __gc_stack_bottom = stack.data() + base_;

        __init();
    }

    ~Interpreter()
    {
        __shutdown();
    }
};

struct Block
{
    uint32_t offset_start;
    uint32_t offset_end;

    uint32_t get_start()
    {
        return offset_start >> 1;
    }

    void set_start(uint32_t start)
    {
        offset_start = start << 1 | (offset_start & 1);
    }

    uint32_t get_end()
    {
        return offset_end >> 1;
    }

    void set_end(uint32_t end)
    {
        offset_end = end << 1 | (offset_end & 1);
    }

    bool get_reachable()
    {
        return (offset_start & 1U) == 1;
    }

    void set_reachable(bool reachable)
    {
        if (reachable)
        {
            offset_start |= 1U;
        }
        else
        {
            offset_start &= ~1U;
        }
    }

    bool get_visited()
    {
        return (offset_end & 1U) == 1;
    }

    void set_visited(bool visited)
    {
        if (visited)
        {
            offset_end |= 1U;
        }
        else
        {
            offset_end &= ~1U;
        }
    }

    Block(uint32_t start = 0, uint32_t end = 0)
        : offset_start(start << 1), offset_end(end << 1) {}
};

struct Code
{
    char *code;
    int32_t code_size;

    Instruction *get_by_id(int32_t id) const
    {
        assert(id >= 0 && id < code_size, "Tried to get instruction outside of code");
        return reinterpret_cast<Instruction *>(code + id);
    }

    int32_t to_id(Instruction *_ins) const
    {
        auto res = reinterpret_cast<char *>(_ins) - code;
        assert(res >= 0 && res < code_size, "Tried to get id outside of code");
        return res;
    }

    Instruction *get_next(Instruction *_ins) const
    {
        auto id = to_id(_ins);
        if (id + _ins->size() >= code_size)
        {
            return nullptr;
        }
        return get_by_id(id + _ins->size());
    }

    Code(char *code_, int32_t size) : code(code_), code_size(size) {}
};

struct Analyser
{
    Result result;
    Code code;
    // Bitvectors
    std::vector<bool> reachable, visited, boundary;

    std::vector<std::tuple<uint32_t, uint32_t>> occurencies;
    std::vector<std::tuple<uint32_t, uint32_t>> double_occurencies;

    Analyser(Result res) : code(Code(res.code, res.code_size)), result(res)
    {
        reachable.resize(res.code_size, false);
        visited.resize(res.code_size, false);
        boundary.resize(res.code_size, false);
    }

    void sort_occurencies()
    {
        std::sort(occurencies.begin(), occurencies.end(),
                  [](const auto &a, const auto &b)
                  {
                      return std::get<1>(a) > std::get<1>(b);
                  });

        std::sort(double_occurencies.begin(), double_occurencies.end(),
                  [](const auto &a, const auto &b)
                  {
                      return std::get<1>(a) > std::get<1>(b);
                  });
    }

    void mark_instructions()
    {
        std::vector<int32_t> stack;

        for (int i = 0; i < result.header.pubs_length; i++)
        {
            assert(result.pubs[i].b >= 0 && result.pubs[i].b < result.code_size, "Public symbol points outside of code");
            stack.push_back(i);
        }

        for (; stack.size() != 0;)
        {
            auto cur = code.get_by_id(stack.back());
            stack.pop_back();
            while (cur != nullptr)
            {
                auto cur_id = code.to_id(cur);
                if (visited[cur_id])
                {
                    break;
                }
                reachable[cur_id] = true;
                visited[cur_id] = true;

                switch (cur->tag)
                {
                case instr::JMP:
                    boundary[cur_id] = true;
                    boundary[cur->args[0]] = true;
                    cur = code.get_by_id(cur->args[0]);
                    // Skip default next iter
                    continue;
                case instr::END:
                case instr::RET:
                case instr::FAIL:
                    boundary[cur_id] = true;
                    cur = nullptr;
                    continue;
                case instr::CALL:
                case instr::CJMPZ:
                case instr::CJMPNZ:
                case instr::CLOSURE:
                    assert(cur->args[0] >= 0 && cur->args[0] < code.code_size, "Tried to create closure outside of code");
                    stack.push_back(cur->args[0]);
                    break;
                default:
                    break;
                }
                cur = code.get_next(cur);
            }
        }
    }

    void add_instr(Instruction *_inst)
    {
        for (int32_t i = 0; i < occurencies.size(); i++)
        {
            auto _inst2 = code.get_by_id(std::get<0>(occurencies[i]));
            if (!(*_inst == *_inst2))
            {
                continue;
            }
            occurencies[i] = std::tuple(std::get<0>(occurencies[i]), std::get<1>(occurencies[i]) + 1);
            return;
        }
        occurencies.push_back(std::tuple(reinterpret_cast<char *>(_inst) - result.code, 1));
    }

    void add_double_instr(Instruction *_inst)
    {
        for (int32_t i = 0; i < double_occurencies.size(); i++)
        {
            auto _inst2 = code.get_by_id(std::get<0>(double_occurencies[i]));
            auto _inst3 = code.get_next(_inst);
            auto _inst4 = code.get_next(_inst2);
            auto size1 = _inst->size();
            if (!(*_inst == *_inst2 && *_inst3 == *_inst4))
            {
                continue;
            }
            double_occurencies[i] = std::tuple(std::get<0>(double_occurencies[i]), std::get<1>(double_occurencies[i]) + 1);
            return;
        }
        double_occurencies.push_back(std::tuple(reinterpret_cast<char *>(_inst) - result.code, 1));
    }

    void count_occurencies()
    {
        Instruction *prev = nullptr;
        for (auto cur = code.get_by_id(0); cur != nullptr; cur = code.get_next(cur))
        {
            auto cur_id = code.to_id(cur);
            // std::cout << (reachable[cur_id] ? "* " : "  ") << *cur << "\n";
            if (!reachable[cur_id])
            {
                prev = nullptr;
                continue;
            }

            add_instr(cur);
            if (prev != nullptr)
            {
                add_double_instr(prev);
            }

            if (boundary[cur_id])
            {
                prev = nullptr;
            }
            else
            {
                prev = cur;
            }
        }
    }

    void analyse()
    {
        mark_instructions();
        count_occurencies();
        sort_occurencies();
    }
};

struct Verifier
{
    struct Borders
    {
        int32_t start_offset, end_offset;
        Instruction *header;
    };

    Result res;
    Code code;
    std::vector<Borders> borders;

    Verifier(Result res_) : res(res_), code(res.code, res.code_size) {}

    void analyse_borders()
    {
        Borders cur_borders;
        bool is_function = false;
        auto cur = code.get_by_id(0);
        while (cur != nullptr)
        {
            switch (cur->tag)
            {
            case instr::BEGIN:
            case instr::CBEGIN:
                assert_with_ip(!is_function, code.to_id(cur), "Expected function end");
                cur_borders.start_offset = code.to_id(cur);
                cur_borders.header = cur;
                is_function = true;
                break;
            case instr::END:
                assert_with_ip(is_function, code.to_id(cur), "Unexpected function end");
                cur_borders.end_offset = code.to_id(cur);
                borders.push_back(cur_borders);
                is_function = false;
                break;
            default:
                break;
            }
            cur = code.get_next(cur);
        }
    }
    void verify()
    {
        analyse_borders();

        struct State
        {
            int32_t cur_id;
            int32_t cur_stack_size;
            Borders cur_borders;
        };

        std::vector<int32_t> stack_sizes;
        stack_sizes.resize(code.code_size, -1);

        Borders cur_borders;
        int32_t cur_stack_size = 0;

        auto cur = code.get_by_id(0);

        std::vector<State> stack;
        std::vector<bool> visited;
        visited.resize(code.code_size, false);

        for (int32_t i = 0; i < borders.size(); i++)
        {
            stack.push_back({
                .cur_id = code.to_id(borders[i].header),
                .cur_stack_size = 0,
                .cur_borders = borders[i],
            });
        }

        for (; stack.size() != 0;)
        {
            auto cur = code.get_by_id(stack.back().cur_id);
            cur_stack_size = stack.back().cur_stack_size;
            cur_borders = stack.back().cur_borders;
            stack.pop_back();
            while (cur != nullptr)
            {
                auto cur_id = code.to_id(cur);
                if (visited[cur_id])
                {
                    break;
                }
                visited[cur_id] = true;

                // TODO: Remove
                auto dump_state = [&]()
                {
                    auto cur = code.get_by_id(0);
                    while (cur != nullptr)
                    {
                        auto cur_id = code.to_id(cur);
                        std::cout << (visited[cur_id] ? "* " : "  ")
                                  << stack_sizes[cur_id] << " "
                                  << std::hex << cur_id << std::dec << " "
                                  << *cur << "\n";
                        cur = code.get_next(cur);
                    }
                };
                if (!(stack_sizes[cur_id] < 0 || cur_stack_size == stack_sizes[cur_id]))
                {
                    dump_state();
                }

                assert_with_ip(stack_sizes[cur_id] < 0 || cur_stack_size == stack_sizes[cur_id] || cur->tag == instr::END || cur->tag == instr::RET, cur_id, "Stack sizes don't match");
                stack_sizes[cur_id] = cur_stack_size;

                if (!(cur_stack_size >= cur->get_popped()))
                    dump_state();
                assert_with_ip(cur_stack_size >= cur->get_popped(), cur_id, "Insufficient stack size for operation");
                cur_stack_size += cur->get_diff();

                auto m = cur_borders.header->args[1] >> 16;
                auto locs = cur_borders.header->args[1] & 0xFFFF;
                cur_borders.header->args[1] = locs | (std::max(m, cur_stack_size) << 16);

                auto check_jump = [&](int32_t l)
                {
                    assert_with_ip(l >= cur_borders.start_offset && l <= cur_borders.end_offset, cur_id, "Tried to jump outside of function block");
                    if (stack_sizes[l] >= 0)
                    {
                        auto then = code.get_by_id(l);
                        assert_with_ip(stack_sizes[l] == cur_stack_size || then->tag == instr::END || then->tag == instr::RET, cur_id, "Stack sizes don't match");
                    }
                    else
                    {
                        stack_sizes[l] = cur_stack_size;
                    }
                    return l;
                };

                auto find_borders = [&](int32_t l) {
                    for (int32_t i = 0; i < borders.size(); i++)
                    {
                        if (borders[i].start_offset == l)
                        {
                            return i;
                        }
                    }
                    return -1;
                };

                auto check_call = [&](int32_t l)
                {
                    assert_with_ip(l >= 0 && l < code.code_size, cur_id, "Tried to call function outside of code");
                    auto b_id = find_borders(l);
                    assert_with_ip(b_id >= 0, cur_id, "Tried to call unknown function");
                    return l;
                };

                auto check_access = [&](Instruction::CArg::CArgType typ, int32_t a)
                {
                    switch (typ)
                    {
                    case Instruction::CArg::G:
                        assert_with_ip(a >= 0 && a < res.header.globals_length, cur_id, "Trying to access invalid global");
                        return;
                    case Instruction::CArg::L:
                        if (!(a >= 0 && a < locs))
                            dump_state();
                        assert_with_ip(a >= 0 && a < locs, cur_id, "Trying to access invalid local");
                        return;
                    case Instruction::CArg::A:
                        assert_with_ip(a >= 0 && a < cur_borders.header->args[0], cur_id, "Trying to access invalid argument");
                        return;
                    case Instruction::CArg::C:
                        // NOTE: We can't check closure args _now_
                        return;
                    }
                };

                // NOTE: Not very reliable, but almost all instructions _now_ require their arguments be non-negative
                if (cur->tag != instr::CONST)
                {
                    for (int32_t i = 0; i < cur->get_args_length(); i++)
                    {
                        assert_with_ip(cur->args[i] >= 0, cur_id, "Argument should be positive");
                    }
                }

                switch (cur->tag)
                {
                case instr::BEGIN:
                case instr::CBEGIN:
                    cur_stack_size = 0;
                    break;
                case instr::JMP:
                    cur = code.get_by_id(check_jump(cur->args[0]));
                    // Skip default next iter
                    continue;
                case instr::END:
                case instr::RET:
                case instr::FAIL:
                    cur = nullptr;
                    continue;
                case instr::CALL:
                    stack.push_back({
                        .cur_id = check_call(cur->args[0]),
                        .cur_stack_size = cur_stack_size,
                        .cur_borders = borders[find_borders(cur->args[0])],
                    });
                    break;
                case instr::CJMPZ:
                case instr::CJMPNZ:
                    stack.push_back({
                        .cur_id = check_jump(cur->args[0]),
                        .cur_stack_size = cur_stack_size,
                        .cur_borders = cur_borders,
                    });
                    break;
                case instr::CLOSURE:
                    stack.push_back({
                        .cur_id = check_call(cur->args[0]),
                        .cur_stack_size = cur_stack_size,
                        .cur_borders = borders[find_borders(cur->args[0])],
                    });
                    for (int32_t i = 0; i < cur->args[1]; i++)
                    {
                        check_access(cur->cargs[i].tag, cur->cargs[i].arg);
                    }
                    break;
                case instr::LDG:
                case instr::STG:
                    check_access(Instruction::CArg::CArgType::G, cur->args[0]);
                    break;
                case instr::LDL:
                case instr::STL:
                    check_access(Instruction::CArg::CArgType::L, cur->args[0]);
                    break;
                case instr::LDA:
                case instr::STA_:
                    check_access(Instruction::CArg::CArgType::A, cur->args[0]);
                    break;
                case instr::LDC:
                case instr::STC:
                    check_access(Instruction::CArg::CArgType::C, cur->args[0]);
                    break;
                case instr::SEXP:
                case instr::TAG:
                    assert_with_ip(cur->args[0] >= 0 && cur->args[0] < res.header.st_length, cur_id, "String index outside of range");
                    break;
                default:
                    break;
                }
                cur = code.get_next(cur);
            }
        }
    }
};

struct Interpreter2
{
    // Approx ~4 MiB
    const size_t STACK_MAX_SIZE = 1024 * 1024;
    const size_t CALL_STACK_MAX_SIZE = 2048;

    Result result;
    std::vector<aint> stack;
    std::vector<SFrame> frames;

    size_t ip;
    size_t base;
    size_t args;
    size_t locals;
    size_t captured;
    bool is_closure;

    int32_t get_code_size()
    {
        return result.code_size;
    }

    int32_t read_i32()
    {
        assert(get_code_size() >= ip + sizeof(int32_t), "Unexpected file end while reading instruction arg");

        int32_t res;
        std::memcpy(&res, &result.code[ip], sizeof(int32_t));
        ip += sizeof(int32_t);
        return res;
    }

    aint pop()
    {
        __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_bottom) - 1;
        return *reinterpret_cast<aint *>(__gc_stack_bottom);
    }

    void push(aint v)
    {
        *reinterpret_cast<aint *>(__gc_stack_bottom) = v;
        __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_bottom) + 1;
    }

    int interpret()
    {
        auto code = result.code;

        frames.push_back(SFrame{
            .prev_ip = 0,
            .prev_base = 0,
            .prev_args = 0,
            .prev_locals = 0,
            .prev_captured = 0,
            .is_closure = false,
        });

        while (true)
        {
            assert(ip < get_code_size() && ip >= 0, "Tried to read instruction outside of bytecode");
            char instr = code[ip];
            ip++;
            switch (instr)
            {
            case instr::ADD:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) + UNBOX(rhs)));
                break;
            }
            case instr::SUB:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) - UNBOX(rhs)));
                break;
            }
            case instr::MUL:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX(UNBOX(lhs) * UNBOX(rhs)));
                break;
            }
            case instr::DIV:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                assert_with_ip(UNBOX(rhs) != 0, ip, "Division by zero");
                push(BOX(UNBOX(lhs) / UNBOX(rhs)));
                break;
            }
            case instr::REM:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                assert_with_ip(UNBOX(rhs) != 0, ip, "Remainder zero");
                push(BOX(UNBOX(lhs) % UNBOX(rhs)));
                break;
            }
            case instr::LSS:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) < UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::LEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) <= UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::GRE:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) > UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::GEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) >= UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::EQU:
            {
                auto rhs = pop();
                auto lhs = pop();
                push(BOX(lhs == rhs ? 1 : 0));
                break;
            }
            case instr::NEQ:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != UNBOX(rhs)) ? 1 : 0));
                break;
            }
            case instr::AND:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != 0 && UNBOX(rhs) != 0) ? 1 : 0));
                break;
            }
            case instr::OR:
            {
                auto rhs = pop();
                auto lhs = pop();
                assert_with_ip(UNBOXED(rhs) && UNBOXED(lhs), ip, "Arguments not integers");
                push(BOX((UNBOX(lhs) != 0 || UNBOX(rhs) != 0) ? 1 : 0));
                break;
            }
            case instr::CONST:
            {
                auto v = read_i32();
                push(BOX(v));
                break;
            }
            case instr::STRING:
            {
                auto v = read_i32();
                std::string_view sv = &result.st[v];
                void *v_ = get_object_content_ptr(alloc_string(sv.length()));
                push(reinterpret_cast<aint>(v_));
                strcpy(TO_DATA(v_)->contents, sv.data());
                break;
            }
            case instr::SEXP:
            {
                int32_t s = read_i32();
                int32_t n = read_i32();
                char *tag = &result.st[s];
                auto *v = get_object_content_ptr(alloc_sexp(n));

                TO_SEXP(v)->tag = UNBOX(LtagHash(tag));

                for (int32_t i = n - 1; i >= 0; i--)
                {
                    auto vv = pop();
                    auto sexp_ = TO_SEXP(v);
                    reinterpret_cast<auint *>(sexp_->contents)[i] = vv;
                }

                push(reinterpret_cast<aint>(v));
                break;
            }
            case instr::STI:
            {
                not_implemented(ip, "STI");
            }
            case instr::STA:
            {
                auto v = pop();
                auto idx_v = pop();
                auto agg = pop();

                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");
                assert_with_ip(UNBOXED(idx_v), ip, "Index not integer");

                auto idx = UNBOX(idx_v);
                auto agg_ = TO_DATA(agg);
                aint len = static_cast<aint>(LEN(TO_DATA(agg)->data_header));
                assert_with_ip(idx >= 0 && idx < len, ip, "Index outside of range");
                switch (tag)
                {
                case ARRAY:
                    reinterpret_cast<aint *>(agg_->contents)[idx] = v;
                    break;
                case STRING:
                    assert_with_ip(UNBOXED(v) && v >= 0 && v <= 0xff, ip, "Can't assign value to string");
                    agg_->contents[idx] = UNBOX(v);
                    break;
                case SEXP:
                    TO_SEXP(agg_)->contents[idx] = v;
                    break;
                default:
                    unreachable(ip);
                }
                push(v);
                break;
            }
            case instr::JMP:
            {
                int32_t offset = read_i32();
                ip = offset;
                break;
            }
            case instr::END:
            case instr::RET:
            {
                SFrame f = frames.back();
                aint v = pop();
                __gc_stack_bottom = reinterpret_cast<aint *>(__gc_stack_top) + base - args - (is_closure ? 1 : 0);

                push(v);

                if (f.prev_ip == 0)
                {
                    return 0;
                }

                ip = f.prev_ip;
                base = f.prev_base;
                args = f.prev_args;
                locals = f.prev_locals;
                captured = f.prev_captured;
                is_closure = f.is_closure;
                frames.pop_back();

                break;
            }
            case instr::DROP:
            {
                pop();
                break;
            }
            case instr::DUP:
            {
                aint v = pop();
                push(v);
                push(v);

                break;
            }
            case instr::SWAP:
            {
                aint top = pop();
                aint second = pop();

                push(top);
                push(second);

                break;
            }
            case instr::ELEM:
            {
                aint idx_v = pop();
                aint agg = pop();

                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");
                assert_with_ip(UNBOXED(idx_v), ip, "Index not integer");

                auto idx = UNBOX(idx_v);
                auto agg_ = TO_DATA(agg);
                aint len = static_cast<aint>(LEN(TO_DATA(agg)->data_header));
                if (idx < 0 || idx >= len)
                {
                    std::cout << "Index outside of range\n";
                    exit(1);
                }
                switch (tag)
                {
                case ARRAY:
                    push(reinterpret_cast<aint *>(agg_->contents)[idx]);
                    break;
                case STRING:
                    push(BOX(agg_->contents[idx]));
                    break;
                case SEXP:
                {
                    auto sexp_ = TO_SEXP(agg);
                    push(reinterpret_cast<auint *>(sexp_->contents)[idx]);
                    break;
                }
                default:
                    unreachable(ip);
                }

                break;
            }
            case instr::LDG:
            {
                auto g = read_i32();
                push(stack[g]);

                break;
            }
            case instr::LDL:
            {
                auto l = read_i32();
                push(stack[base + l]);
                break;
            }
            case instr::LDA:
            {
                auto a = read_i32();
                push(stack[base - args + a]);

                break;
            }
            case instr::LDC:
            {
                auto c = read_i32();
                auto cc = stack[base - args - 1];
                assert_with_ip(c >= 0 && c < captured, ip, "Tried to get invalid captured");
                auto cc_ = (aint *)cc;
                push(cc_[c + 1]);
                break;
            }

            case instr::LDGR:
            {
                not_implemented(ip, "LDGR");
            }
            case instr::LDLR:
            {
                not_implemented(ip, "LDLR");
            }
            case instr::LDAR:
            {
                not_implemented(ip, "LDAR");
            }
            case instr::LDCR:
            {
                not_implemented(ip, "LDCR");
            }
            case instr::STG:
            {
                auto g = read_i32();
                auto v = pop();

                stack[g] = v;
                push(v);

                break;
            }
            case instr::STL:
            {
                auto l = read_i32();
                auto v = pop();

                stack[base + l] = v;
                push(v);

                break;
            }
            case instr::STA_:
            {
                auto a = read_i32();
                auto v = pop();

                stack[base - args + a] = v;
                push(v);

                break;
            }
            case instr::STC:
            {
                auto c = read_i32();
                auto cc = stack[base - args - 1];
                assert_with_ip(c >= 0 && c < captured, ip, "Tried to get invalid captured");
                auto cc_ = (aint *)cc;
                auto v = pop();

                cc_[c + 1] = v;
                push(v);

                break;
            }
            case instr::CJMPZ:
            {
                auto l = read_i32();
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                if (UNBOX(v) == 0)
                {
                    ip = l;
                }

                break;
            }
            case instr::CJMPNZ:
            {
                auto l = read_i32();
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                if (UNBOX(v) != 0)
                {
                    ip = l;
                }

                break;
            }
            case instr::BEGIN:
            case instr::CBEGIN:
            {
                read_i32();
                auto n = read_i32();
                auto m = n >> 16;
                auto locs = n & 0xFFFF;

                locals = locs;

                assert_with_ip(base + locs + m <= STACK_MAX_SIZE, ip, "Stack overflow");

                for (int32_t i = 0; i < locs; i++)
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::CLOSURE:
            {
                auto l = read_i32();
                auto n = read_i32();
                auto *closure = get_object_content_ptr(alloc_closure(n + 1));
                push(reinterpret_cast<aint>(closure));
                static_cast<aint *>(closure)[0] = l;

                for (int32_t i = 0; i < n; ++i)
                {
                    auto kind = static_cast<char>(code[this->ip]);
                    ip++;
                    auto m = read_i32();

                    switch (kind)
                    {
                    case 0:
                        static_cast<aint *>(closure)[i + 1] = stack[m];
                        break;

                    case 1:
                        static_cast<aint *>(closure)[i + 1] = stack[base + m];
                        break;

                    case 2:
                        static_cast<aint *>(closure)[i + 1] = stack[base - args + m];
                        break;

                    case 3:
                        static_cast<aint *>(closure)[i + 1] = ((aint *)stack[base - args - 1])[m + 1];
                        break;

                    default:
                        std::cout << "Not implemented\n";
                        exit(1);
                    }
                }
                break;
            }
            case instr::CALLC:
            {
                auto n = read_i32();
                auto closure = *(reinterpret_cast<aint *>(__gc_stack_bottom) - n - 1);

                assert_with_ip(!UNBOXED(closure) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(closure))) == CLOSURE,
                               ip, "Try to call not closure");

                assert_with_ip(frames.size() < CALL_STACK_MAX_SIZE, ip, "Cant call closure: call stack overflow");

                frames.push_back(SFrame{
                    .prev_ip = ip,
                    .prev_base = base,
                    .prev_args = args,
                    .prev_locals = locals,
                    .prev_captured = captured,
                    .is_closure = is_closure,
                });

                ip = reinterpret_cast<aint *>(closure)[0];
                is_closure = true;
                base = reinterpret_cast<aint *>(__gc_stack_bottom) - reinterpret_cast<aint *>(__gc_stack_top);
                args = n;
                locals = 0;
                captured = LEN(TO_DATA(closure)->data_header) - 1;

                break;
            }
            case instr::CALL:
            {
                auto l = read_i32();
                auto n = read_i32();

                assert_with_ip(frames.size() < CALL_STACK_MAX_SIZE, ip, "Cant call function: call stack overflow");

                frames.push_back(SFrame{
                    .prev_ip = ip,
                    .prev_base = base,
                    .prev_args = args,
                    .prev_locals = locals,
                    .prev_captured = captured,
                    .is_closure = is_closure,
                });

                ip = l;
                is_closure = false;
                base = reinterpret_cast<aint *>(__gc_stack_bottom) - reinterpret_cast<aint *>(__gc_stack_top);
                args = n;
                captured = 0;
                locals = 0;

                break;
            }
            case instr::TAG:
            {
                auto s = read_i32();
                auto n = read_i32();
                auto v = pop();

                char *exp = &result.st[s];
                aint exp_ = UNBOX(LtagHash(exp));

                if (!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == SEXP)
                {
                    auto sexp_ = TO_SEXP(v);
                    auto tag = sexp_->tag;

                    push((LEN(sexp_->data_header) == n && exp_ == tag) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::ARRAY:
            {
                auto n = read_i32();
                auto v = pop();

                if (!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == ARRAY)
                {
                    auto data_ = TO_DATA(v);
                    push((LEN(data_->data_header) == n) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::FAIL:
            {
                auto ln = read_i32();
                auto col = read_i32();
                pop();

                std::cout << "Match failure at " << ln << ":" << col << "\n";
                exit(1);
            }
            case instr::LINE:
            {
                read_i32();
                break;
            }
            case instr::PATT_eq:
            {
                auto rhs = pop();
                auto lhs = pop();

                if (!UNBOXED(lhs) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(lhs))) == STRING &&
                    !UNBOXED(rhs) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(rhs))) == STRING)
                {
                    push((strcmp(TO_DATA(lhs)->contents, TO_DATA(rhs)->contents) == 0) ? BOX(1) : BOX(0));
                }
                else
                {
                    push(BOX(0));
                }

                break;
            }
            case instr::PATT_is_string:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == STRING)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_array:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == ARRAY)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_sexp:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == SEXP)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_ref:
            {
                auto v = pop();

                push((!UNBOXED(v))
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_val:
            {
                auto v = pop();

                push((UNBOXED(v))
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::PATT_is_fun:
            {
                auto v = pop();

                push((!UNBOXED(v) && get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(v))) == CLOSURE)
                         ? BOX(1)
                         : BOX(0));
                break;
            }
            case instr::CALL_Lread:
            {
                aint v = 0;
                std::cout << " > " << std::flush;
                std::cin >> v;

                push(BOX(v));

                break;
            }
            case instr::CALL_Lwrite:
            {
                auto v = pop();

                assert_with_ip(UNBOXED(v), ip, "Value is not integer");

                std::cout << UNBOX(v) << "\n";

                push(BOX(0));

                break;
            }
            case instr::CALL_Llength:
            {
                auto agg = pop();
                assert_with_ip(!UNBOXED(agg), ip, "Not aggregate");
                auto tag = get_type_header_ptr(get_obj_header_ptr(reinterpret_cast<void *>(agg)));
                assert_with_ip(tag == ARRAY || tag == STRING || tag == SEXP, ip, "Not aggregate");

                push(BOX(LEN(TO_DATA(agg)->data_header)));
                break;
            }
            case instr::CALL_Lstring:
            {
                auto v = pop();

                push(reinterpret_cast<aint>(Lstring(&v)));

                break;
            }
            case instr::CALL_Barray:
            {
                auto n = read_i32();
                auto *v = get_object_content_ptr(alloc_array(n));

                for (int32_t i = n - 1; i >= 0; i--)
                {
                    static_cast<aint *>(v)[i] = pop();
                }

                push(reinterpret_cast<aint>(v));
                break;
            }

            default:
                assert(false, "Unsupported instruction");
                break;
            }
        }
        return 0;
    }

    Interpreter2(Result result_)
    {
        // Globals + dummy main arguments
        const size_t base_ = result_.header.globals_length + 2;

        result = result_;
        stack.resize(STACK_MAX_SIZE, BOX(0));
        ip = 0;
        base = base_;
        args = 2;
        is_closure = false;

        frames.reserve(CALL_STACK_MAX_SIZE);

        __gc_stack_top = stack.data();
        __gc_stack_bottom = stack.data() + base_;

        __init();
    }

    ~Interpreter2()
    {
        __shutdown();
    }
};

std::string to_hex_string(const std::vector<char> &vec)
{
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (i > 0)
            oss << ' ';
        oss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(vec[i]));
    }
    return oss.str();
}

void print_occurency(Code code, std::tuple<int32_t, int32_t> occ, int32_t size)
{
    std::cout << std::get<1>(occ) << " ";
    auto cur = code.get_by_id(std::get<0>(occ));
    for (int32_t i = 0; i < size - 1; i++)
    {
        std::cout << *cur << "; ";
        cur = code.get_next(cur);
    }
    std::cout << *cur << "\n";
}

namespace mode
{
    enum Mode
    {
        VALIDATE,
        DUMP,
        ANALYSE,
        RUN,
        VERIFY_RUN,
    };
}

int main(int argc, char **argv)
{
    assert(argc >= 2, "No input file");

    std::string fname;
    std::string flag;

    mode::Mode mode = mode::RUN;

    if (argc >= 3)
    {
        flag = argv[1];
        fname = argv[2];

        if (flag == "-c")
        {
            mode = mode::VALIDATE;
        }
        else if (flag == "-d")
        {
            mode = mode::DUMP;
        }
        else if (flag == "-a")
        {
            mode = mode::ANALYSE;
        }
        else if (flag == "-v")
        {
            mode = mode::VERIFY_RUN;
        }
    }
    else
    {
        fname = argv[1];
    }

    std::vector<char> bytes = read_file(fname);

    Result result = parse_and_validate(std::move(bytes));

    switch (mode)
    {
    case mode::VALIDATE:
    {
        std::cout << "Parsed filed succesfully\n";
        exit(0);
    }
    case mode::DUMP:
    {
        dump_bytecode(result.code, result.code_size);
        exit(0);
    }
    case mode::ANALYSE:
    {
        Code code = Code(result.code, result.code_size);
        Analyser a = Analyser(result);
        a.analyse();

        std::cout << "Instructions sorted by occurencies:\n";

        for (int32_t i = 0, j = 0; i < a.occurencies.size() || j < a.double_occurencies.size();)
        {
            if (i >= a.occurencies.size())
            {
                auto occ = a.double_occurencies[j++];
                print_occurency(code, occ, 2);
                continue;
            }
            if (j >= a.double_occurencies.size())
            {
                auto occ = a.occurencies[i++];
                print_occurency(code, occ, 1);
                continue;
            }
            auto occ1 = a.occurencies[i];
            auto occ2 = a.double_occurencies[j];
            if (std::get<1>(occ1) > std::get<1>(occ2))
            {
                print_occurency(code, occ1, 1);
                i++;
            }
            else
            {
                print_occurency(code, occ2, 2);
                j++;
            }
        }

        exit(0);
    }
    case mode::RUN:
    {
        Interpreter it = Interpreter(result);
        exit(it.interpret());
    }
    case mode::VERIFY_RUN:
    {
        auto verifier = Verifier(result);
        verifier.verify();
        Interpreter2 it = Interpreter2(result);
        exit(it.interpret());
    }
    default:
    {
        std::cout << "Unsupported mode\n";
        exit(1);
    }
    }
}