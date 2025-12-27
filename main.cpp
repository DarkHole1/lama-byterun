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
        std::cout << "[ip=" << ip << "] " << msg << "\n";
        exit(1);
    }
}

[[noreturn]] void unknown_instruction(int32_t ip, long name)
{
    std::cout << "[ip=" << ip << "] " << "Unknown instruction: " << name;
    exit(1);
}

[[noreturn]] void not_implemented(int32_t ip, std::string name)
{
    std::cout << "[ip=" << ip << "] " << "Instruction not implemented: " << name;
    exit(1);
}

[[noreturn]] void unreachable(int32_t ip)
{
    std::cout << "[ip=" << ip << "] " << "Unreachable code executed";
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
            L = 1,
            G = 2,
            A = 3,
            C = 4
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

                locals = n;

                for (int32_t i = 0; i < n; i++)
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

struct Analyser
{
    Result result;
    // Bitvectors
    std::vector<bool> reachable, visited, boundary;

    std::vector<std::tuple<uint32_t, uint32_t>> occurencies;
    std::vector<std::tuple<uint32_t, uint32_t>> double_occurencies;

    Analyser(Result res)
    {
        result = res;
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
        for (int i = 0; i < result.header.pubs_length; i++)
        {
            assert(result.pubs[i].b >= 0 && result.pubs[i].b < result.code_size, "Public symbol points outside of code");
            reachable[i] = true;
        }

        bool has_reachable;
        do
        {
            has_reachable = false;
            for (int32_t i = 0; i < result.code_size;)
            {
                if (!reachable[i] || visited[i])
                {
                    i++;
                    continue;
                }
                has_reachable = true;
                visited[i] = true;
                Instruction *cur = reinterpret_cast<Instruction *>(result.code + i);

                if (i + cur->size() > result.code_size)
                {
                    goto LOOP_END;
                }

                switch (cur->tag)
                {
                case instr::JMP:
                    boundary[i] = true;
                    assert_with_ip(cur->args[0] >= 0 && cur->args[0] < result.code_size, i, "Tried to jump outside of code");
                    reachable[cur->args[0]] = true;
                    i = cur->args[0];
                    // Skip default next iter
                    continue;
                case instr::END:
                case instr::RET:
                    boundary[i] = true;
                    goto LOOP_END;
                case instr::CJMPZ:
                case instr::CJMPNZ:
                case instr::CALL:
                    boundary[i] = true;
                    assert_with_ip(cur->args[0] >= 0 && cur->args[0] < result.code_size, i, "Tried to jump outside of code");
                    reachable[cur->args[0]] = true;
                    break;
                case instr::BEGIN:
                case instr::CBEGIN:
                    if (i > 0)
                    {
                        boundary[i - 1] = true;
                    }
                    break;
                case instr::CLOSURE:
                    assert_with_ip(cur->args[0] >= 0 && cur->args[0] < result.code_size, i, "Tried to create closure outside of code");
                    reachable[cur->args[0]] = true;
                    break;
                case instr::CALLC:
                    boundary[i] = true;
                    break;
                }
                i += cur->size();
                if (i < result.code_size)
                {
                    reachable[i] = true;
                }
            }
        LOOP_END:
        } while (has_reachable);
    }

    void add_instr(Instruction *_inst)
    {
        for (int32_t i = 0; i < occurencies.size(); i++)
        {
            auto _inst2 = reinterpret_cast<Instruction *>(result.code + std::get<0>(occurencies[i]));
            auto size1 = _inst->size();
            auto size2 = _inst2->size();
            if (size1 != size2)
            {
                continue;
            }
            if (std::memcmp(_inst, _inst2, size1) != 0)
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
            auto _inst2 = reinterpret_cast<Instruction *>(result.code + std::get<0>(double_occurencies[i]));
            auto size1 = _inst->size();
            auto size2 = _inst2->size();
            if (size1 != size2)
            {
                continue;
            }
            auto _inst3 = reinterpret_cast<Instruction *>(reinterpret_cast<char *>(_inst) + size1);
            auto _inst4 = reinterpret_cast<Instruction *>(reinterpret_cast<char *>(_inst2) + size1);
            auto size3 = _inst3->size();
            auto size4 = _inst4->size();
            if (size3 != size4)
            {
                continue;
                ;
            }
            if (std::memcmp(_inst, _inst2, size1 + size3) != 0)
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
        for (int32_t i = 0; i < result.code_size;)
        {
            Instruction *cur = reinterpret_cast<Instruction *>(result.code + i);
            if (i + cur->size() > result.code_size)
            {
                return;
            }
            if (!reachable[i])
            {
                i += 1;
                prev = nullptr;
                continue;
            }

            add_instr(cur);
            if (prev != nullptr)
            {
                add_double_instr(prev);
            }

            if (boundary[i])
            {
                prev = nullptr;
            }
            else
            {
                prev = cur;
            }
            i += cur->size();
        }
    }

    void analyse()
    {
        mark_instructions();
        count_occurencies();
        sort_occurencies();
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

void print_occurency(char *code, std::tuple<int32_t, int32_t> occ, int32_t size)
{
    std::cout << std::get<1>(occ) << " ";
    auto cur = reinterpret_cast<Instruction *>(code + std::get<0>(occ));
    for (int32_t i = 0; i < size - 1; i++)
    {
        std::cout << *cur << "; ";
        cur = reinterpret_cast<Instruction *>(reinterpret_cast<char *>(cur) + cur->size());
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
        Analyser a = Analyser(result);
        a.analyse();

        std::cout << "Instructions sorted by occurencies:\n";

        for (int32_t i = 0, j = 0; i < a.occurencies.size() && j < a.double_occurencies.size();)
        {
            if (i >= a.occurencies.size())
            {
                auto occ = a.double_occurencies[j++];
                print_occurency(result.code, occ, 2);
                continue;
            }
            if (j >= a.double_occurencies.size())
            {
                auto occ = a.occurencies[i++];
                print_occurency(result.code, occ, 1);
                continue;
            }
            auto occ1 = a.occurencies[i];
            auto occ2 = a.double_occurencies[j];
            if (std::get<1>(occ1) > std::get<1>(occ2))
            {
                print_occurency(result.code, occ1, 1);
                i++;
            }
            else
            {
                print_occurency(result.code, occ2, 2);
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
    default:
    {
        std::cout << "Unsupported mode\n";
        exit(1);
    }
    }
}