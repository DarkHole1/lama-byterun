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
}

void dump_bytecode(char *code, int32_t code_size)
{
    for (int32_t ip = 0; ip < code_size; ip++)
    {
        auto read_i32 = [&]()
        {
            int32_t arg;
            assert_with_ip(ip + 1 + sizeof(int32_t) < code_size, ip, "Unexpected end of file reading argument");
            std::memcpy(&arg, &code[ip + 1], sizeof(arg));
            ip += sizeof(arg);
            return arg;
        };

        switch (code[ip])
        {
        case instr::ADD:
        {
            std::cout << std::hex << ip << std::dec << " " << "ADD\n";
            break;
        }
        case instr::SUB:
        {
            std::cout << std::hex << ip << std::dec << " " << "SUB\n";
            break;
        }
        case instr::MUL:
        {
            std::cout << std::hex << ip << std::dec << " " << "MUL\n";
            break;
        }
        case instr::DIV:
        {
            std::cout << std::hex << ip << std::dec << " " << "DIV\n";
            break;
        }
        case instr::REM:
        {
            std::cout << std::hex << ip << std::dec << " " << "REM\n";
            break;
        }
        case instr::LSS:
        {
            std::cout << std::hex << ip << std::dec << " " << "LSS\n";
            break;
        }
        case instr::LEQ:
        {
            std::cout << std::hex << ip << std::dec << " " << "LEQ\n";
            break;
        }
        case instr::GRE:
        {
            std::cout << std::hex << ip << std::dec << " " << "GRE\n";
            break;
        }
        case instr::GEQ:
        {
            std::cout << std::hex << ip << std::dec << " " << "GEQ\n";
            break;
        }
        case instr::EQU:
        {
            std::cout << std::hex << ip << std::dec << " " << "EQU\n";
            break;
        }
        case instr::NEQ:
        {
            std::cout << std::hex << ip << std::dec << " " << "NEQ\n";
            break;
        }
        case instr::AND:
        {
            std::cout << std::hex << ip << std::dec << " " << "AND\n";
            break;
        }
        case instr::OR:
        {
            std::cout << std::hex << ip << std::dec << " " << "OR\n";
            break;
        }
        case instr::CONST:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CONST " << arg << "\n";
            break;
        }
        case instr::STRING:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "STRING " << arg << "\n";
            break;
        }
        case instr::SEXP:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "SEXP " << arg1 << " " << arg2 << "\n";
            break;
        }
        case instr::STI:
        {
            std::cout << std::hex << ip << std::dec << " " << "STI\n";
            break;
        }
        case instr::STA:
        {
            std::cout << std::hex << ip << std::dec << " " << "STA\n";
            break;
        }
        case instr::JMP:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "JMP " << arg << "\n";
            break;
        }
        case instr::END:
        {
            std::cout << std::hex << ip << std::dec << " " << "END\n";
            break;
        }
        case instr::RET:
        {
            std::cout << std::hex << ip << std::dec << " " << "RET\n";
            break;
        }
        case instr::DROP:
        {
            std::cout << std::hex << ip << std::dec << " " << "DROP\n";
            break;
        }
        case instr::DUP:
        {
            std::cout << std::hex << ip << std::dec << " " << "DUP\n";
            break;
        }
        case instr::SWAP:
        {
            std::cout << std::hex << ip << std::dec << " " << "SWAP\n";
            break;
        }
        case instr::ELEM:
        {
            std::cout << std::hex << ip << std::dec << " " << "ELEM\n";
            break;
        }
        case instr::LDG:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDG " << arg << "\n";
            break;
        }
        case instr::LDL:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDL " << arg << "\n";
            break;
        }
        case instr::LDA:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDA " << arg << "\n";
            break;
        }
        case instr::LDC:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDC " << arg << "\n";
            break;
        }
        case instr::LDGR:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDGR " << arg << "\n";
            break;
        }
        case instr::LDLR:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDLR " << arg << "\n";
            break;
        }
        case instr::LDAR:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDAR " << arg << "\n";
            break;
        }
        case instr::LDCR:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LDCR " << arg << "\n";
            break;
        }
        case instr::STG:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "STG " << arg << "\n";
            break;
        }
        case instr::STL:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "STL " << arg << "\n";
            break;
        }
        case instr::STA_:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "STA_ " << arg << "\n";
            break;
        }
        case instr::STC:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "STC " << arg << "\n";
            break;
        }
        case instr::CJMPZ:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CJMPZ " << arg << "\n";
            break;
        }
        case instr::CJMPNZ:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CJMPNZ " << arg << "\n";
            break;
        }
        case instr::BEGIN:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "BEGIN " << arg1 << " " << arg2 << "\n";
            break;
        }
        case instr::CBEGIN:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CBEGIN " << arg1 << " " << arg2 << "\n";
            break;
        }
        case instr::CLOSURE:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();

            std::cout << std::hex << _ip << std::dec << " " << "CLOSURE " << arg1 << " " << arg2;

            for (int32_t i = 0; i < arg2; i++)
            {
                char t;
                int32_t m;
                std::memcpy(&t, &code[ip + 1], sizeof(char));
                std::memcpy(&m, &code[ip + 2], sizeof(int32_t));
                ip += 5;

                switch (t)
                {
                case 1:
                    std::cout << " G(";
                    break;
                case 2:
                    std::cout << " L(";
                    break;
                case 3:
                    std::cout << " A(";
                    break;
                case 4:
                    std::cout << " C(";
                    break;
                }
                std::cout << m << ")";
            }

            std::cout << "\n";
            break;
        }
        case instr::CALLC:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CALLC " << std::hex << arg << std::dec << "\n";
            break;
        }
        case instr::CALL:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CALL " << std::hex << arg1 << std::dec << " " << arg2 << "\n";
            break;
        }
        case instr::TAG:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "TAG " << arg1 << arg2 << "\n";
            break;
        }
        case instr::ARRAY:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "ARRAY " << arg << "\n";
            break;
        }
        case instr::FAIL:
        {
            const size_t _ip = ip;
            int32_t arg1 = read_i32(), arg2 = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "FAIL " << arg1 << " " << arg2 << "\n";
            break;
        }
        case instr::LINE:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "LINE " << arg << "\n";
            break;
        }
        case instr::PATT_eq:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_eq\n";
            break;
        }
        case instr::PATT_is_string:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_string\n";
            break;
        }
        case instr::PATT_is_array:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_array\n";
            break;
        }
        case instr::PATT_is_sexp:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_sexp\n";
            break;
        }
        case instr::PATT_is_ref:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_ref\n";
            break;
        }
        case instr::PATT_is_val:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_val\n";
            break;
        }
        case instr::PATT_is_fun:
        {
            std::cout << std::hex << ip << std::dec << " " << "PATT_is_fun\n";
            break;
        }
        case instr::CALL_Lread:
        {
            std::cout << std::hex << ip << std::dec << " " << "CALL_Lread\n";
            break;
        }
        case instr::CALL_Lwrite:
        {
            std::cout << std::hex << ip << std::dec << " " << "CALL_Lwrite\n";
            break;
        }
        case instr::CALL_Llength:
        {
            std::cout << std::hex << ip << std::dec << " " << "CALL_Llength\n";
            break;
        }
        case instr::CALL_Lstring:
        {
            std::cout << std::hex << ip << std::dec << " " << "CALL_Lstring\n";
            break;
        }
        case instr::CALL_Barray:
        {
            const size_t _ip = ip;
            int32_t arg = read_i32();
            std::cout << std::hex << _ip << std::dec << " " << "CALL_Barray " << arg << "\n";
            break;
        }
        default:
        {
            std::cout << std::hex << ip << std::dec << " " << "UNK\n";
            break;
        }
        }
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

long get_instr_size(instr::Instr instr)
{
    switch (instr)
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
        return 1;
    }
    case instr::CONST:
    case instr::STRING:
    {
        return 5;
    }
    case instr::SEXP:
    {
        return 9;
    }
    case instr::STI:
    case instr::STA:
    {
        return 1;
    }
    case instr::JMP:
    {
        return 5;
    }
    case instr::END:
    case instr::RET:
    case instr::DROP:
    case instr::DUP:
    case instr::SWAP:
    case instr::ELEM:
    {
        return 1;
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
        return 5;
    }
    case instr::BEGIN:
    case instr::CBEGIN:
    {
        return 9;
    }
    case instr::CLOSURE:
    {
        return -1;
    }
    case instr::CALLC:
    {
        return 5;
    }
    case instr::CALL:
    case instr::TAG:
    {
        return 9;
    }
    case instr::ARRAY:
    {
        return 5;
    }
    case instr::FAIL:
    {
        return 9;
    }
    case instr::LINE:
    {
        return 5;
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
        return 1;
    }
    case instr::CALL_Barray:
    {
        return 5;
    }
    default:
    {
        return -1;
    }
    }
}

struct Block
{
    long offset_start;
    long offset_end;
    std::vector<char> code;
};

struct Analyser
{
    Result result;
    std::vector<Block> blocks;
    std::vector<std::tuple<std::vector<char>, long>> occurencies;

    Analyser(Result res)
    {
        std::vector<char> code_;
        code_.insert(code_.end(), res.code, res.code + res.code_size);
        result = res;
        blocks.push_back(Block{
            .offset_start = 0,
            .offset_end = res.code_size,
            .code = code_,
        });
    }

    void split_at(long l)
    {
        for (size_t i = 0; i < blocks.size(); i++)
        {
            if (blocks[i].offset_start == l)
            {
                return;
            }
            if (blocks[i].offset_start < l && blocks[i].offset_end > l)
            {
                blocks.push_back({
                    .offset_start = l,
                    .offset_end = blocks[i].offset_end,
                    .code = std::vector<char>(blocks[i].code.begin() + (l - blocks[i].offset_start), blocks[i].code.end()),
                });
                blocks[i].code.resize(l - blocks[i].offset_start);
                blocks[i].offset_end = l;
                return;
            }
        }
    }

    void split_blocks()
    {
        auto code = result.code;

        for (long ip = 0; ip < result.code_size; ip++)
        {
            switch (code[ip])
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
                break;
            }
            case instr::CONST:
            {
                ip += 4;
                break;
            }
            case instr::STRING:
            {
                ip += 4;
                break;
            }
            case instr::SEXP:
            {
                ip += 8;
                break;
            }
            case instr::STI:
            case instr::STA:
            {
                break;
            }
            case instr::JMP:
            {
                int32_t arg;
                std::memcpy(&arg, &code[ip + 1], sizeof(arg));
                ip += sizeof(arg);
                split_at(arg);
                break;
            }
            case instr::END:
            {
                break;
            }
            case instr::RET:
            {
                break;
            }
            case instr::DROP:
            case instr::DUP:
            case instr::SWAP:
            case instr::ELEM:
            {
                break;
            }
            case instr::LDG:
            {
                ip += 4;
                break;
            }
            case instr::LDL:
            {
                ip += 4;
                break;
            }
            case instr::LDA:
            {
                ip += 4;
                break;
            }
            case instr::LDC:
            {
                ip += 4;
                break;
            }
            case instr::LDGR:
            {
                ip += 4;
                break;
            }
            case instr::LDLR:
            {
                ip += 4;
                break;
            }
            case instr::LDAR:
            {
                ip += 4;
                break;
            }
            case instr::LDCR:
            {
                ip += 4;
                break;
            }
            case instr::STG:
            {
                ip += 4;
                break;
            }
            case instr::STL:
            {
                ip += 4;
                break;
            }
            case instr::STA_:
            {
                ip += 4;
                break;
            }
            case instr::STC:
            {
                ip += 4;
                break;
            }
            case instr::CJMPZ:
            {
                int32_t arg;
                std::memcpy(&arg, &code[ip + 1], sizeof(arg));
                ip += sizeof(arg);
                split_at(arg);
                break;
            }
            case instr::CJMPNZ:
            {
                int32_t arg;
                std::memcpy(&arg, &code[ip + 1], sizeof(arg));
                ip += sizeof(arg);
                split_at(arg);
                break;
            }
            case instr::BEGIN:
            {
                ip += 8;
                split_at(ip + 1);
                break;
            }
            case instr::CBEGIN:
            {
                ip += 8;
                split_at(ip + 1);
                break;
            }
            case instr::CLOSURE:
            {
                const size_t _ip = ip;
                int32_t arg1, arg2;
                std::memcpy(&arg1, &code[ip + 1], sizeof(arg1));
                std::memcpy(&arg2, &code[ip + 1 + sizeof(arg1)], sizeof(arg2));
                ip += sizeof(arg1) + sizeof(arg2);
                ip += 5 * arg2;

                split_at(arg1);
                break;
            }
            case instr::CALLC:
            {
                ip += 4;
                break;
            }
            case instr::CALL:
            {
                const size_t _ip = ip;
                int32_t arg1, arg2;
                std::memcpy(&arg1, &code[ip + 1], sizeof(arg1));
                std::memcpy(&arg2, &code[ip + 1 + sizeof(arg1)], sizeof(arg2));
                ip += sizeof(arg1) + sizeof(arg2);
                split_at(arg1);
                break;
            }
            case instr::TAG:
            {
                ip += 8;
                break;
            }
            case instr::ARRAY:
            {
                ip += 4;
                break;
            }
            case instr::FAIL:
            {
                ip += 8;
                break;
            }
            case instr::LINE:
            {
                ip += 4;
                break;
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
                break;
            }
            case instr::CALL_Barray:
            {
                ip += 4;
                break;
            }
            default:
            {
                unknown_instruction(ip, static_cast<long>(code[ip]));
            }
            }
        }
    }

    void add_instr(std::vector<char> instr)
    {
        for (long i = 0; i < occurencies.size(); i++)
        {
            if (std::get<0>(occurencies[i]) == instr)
            {
                occurencies[i] = std::tuple(std::get<0>(occurencies[i]), std::get<1>(occurencies[i]) + 1);
                return;
            }
        }

        occurencies.push_back(std::tuple(instr, 1));
    }

    void process_block(Block b)
    {
        auto code = b.code;
        bool has_prev = false;
        std::vector<char> prev;
        long instr_size = 0;
        for (long i = 0; i < code.size(); i += instr_size)
        {
            instr_size = get_instr_size((instr::Instr)code[i]);
            if (instr_size <= 0)
            {
                switch (code[i])
                {
                case instr::CLOSURE:
                    int32_t arg1, arg2;
                    std::memcpy(&arg1, &code[i + 1], sizeof(arg1));
                    std::memcpy(&arg2, &code[i + 1 + sizeof(arg1)], sizeof(arg2));
                    instr_size = 9 + 5 * arg2;
                    break;

                default:
                    std::cout << "Unknown instruction " << static_cast<long>(code[i]) << "\n";
                    exit(1);
                }
            }

            std::vector<char> cur = std::vector<char>(code.begin() + i, code.begin() + i + instr_size);
            add_instr(cur);

            if (has_prev)
            {
                std::vector<char> dcur;
                dcur.insert(dcur.end(), prev.begin(), prev.end());
                dcur.insert(dcur.end(), cur.begin(), cur.end());

                add_instr(dcur);
            }

            prev = cur;
            has_prev = true;
        }
    }

    void sort_occurencies()
    {
        std::sort(occurencies.begin(), occurencies.end(),
                  [](const auto &a, const auto &b)
                  {
                      return std::get<1>(a) > std::get<1>(b);
                  });
    }

    std::vector<std::tuple<std::vector<char>, long>> analyse()
    {
        split_blocks();

        for (long i = 0; i < blocks.size(); i++)
        {
            process_block(blocks[i]);
        }

        sort_occurencies();

        return occurencies;
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
        auto idioms = a.analyse();

        std::cout << "Instructions sorted by occurencies:\n";

        for (int i = 0; i < idioms.size(); i++)
        {
            std::cout << std::get<1>(idioms[i]) << "\t" << to_hex_string(std::get<0>(idioms[i])) << "\n";
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