#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include "runtime/gc.h"
#include "commons.h"

extern "C" void *Lstring(aint *args);
extern "C" aint LtagHash(char *c);
extern "C" char *de_hash(aint n);

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

struct Verifier
{
    Result res;
    Code code;

    Verifier(Result res_) : res(res_), code(res.code, res.code_size) {}

    void verify()
    {
        struct State
        {
            int32_t cur_id;
            int32_t cur_stack_size;
            Instruction *cur_header;
        };

        std::vector<int32_t> stack_sizes;
        stack_sizes.resize(code.code_size, -1);

        Instruction *cur_header;
        int32_t cur_stack_size = 0;

        std::string entry_point = "main";
        Instruction *cur = nullptr;
        for (int32_t i = 0; i < res.header.pubs_length; i++) {
            if (entry_point == res.st + res.pubs[i].a) {
                cur = code.get_by_id(res.pubs[i].b);
                break;
            }
        }
        assert(cur != nullptr, "Can't find entry point");

        std::vector<State> stack;

        for (; stack.size() != 0;)
        {
            auto cur = code.get_by_id(stack.back().cur_id);
            cur_stack_size = stack.back().cur_stack_size;
            cur_header = stack.back().cur_header;
            stack.pop_back();
            while (cur != nullptr)
            {
                auto cur_id = code.to_id(cur);
                assert_with_ip(stack_sizes[cur_id] < 0 || cur_stack_size == stack_sizes[cur_id] || cur->tag == instr::END || cur->tag == instr::RET, cur_id, "Stack sizes don't match");
                stack_sizes[cur_id] = cur_stack_size;

                assert_with_ip(cur_stack_size >= cur->get_popped(), cur_id, "Insufficient stack size for operation");
                cur_stack_size += cur->get_diff();

                auto m = cur_header->args[1] >> 16;
                auto locs = cur_header->args[1] & 0xFFFF;
                cur_header->args[1] = locs | (std::max(m, cur_stack_size) << 16);

                auto check_next_jump = [&](int32_t l)
                {
                    assert_with_ip(l >= 0 && l <= code.code_size, cur_id, "Tried to jump outside of function block");
                    if (stack_sizes[l] >= 0)
                    {
                        auto then = code.get_by_id(l);
                        assert_with_ip(stack_sizes[l] == cur_stack_size || then->tag == instr::END || then->tag == instr::RET, cur_id, "Stack sizes don't match");
                    }
                    else
                    {
                        stack_sizes[l] = cur_stack_size;
                        cur = code.get_by_id(l);
                    }
                };

                auto check_push_jump = [&](int32_t l)
                {
                    assert_with_ip(l >= 0 && l <= code.code_size, cur_id, "Tried to jump outside of function block");
                    if (stack_sizes[l] >= 0)
                    {
                        auto then = code.get_by_id(l);
                        assert_with_ip(stack_sizes[l] == cur_stack_size || then->tag == instr::END || then->tag == instr::RET, cur_id, "Stack sizes don't match");
                    }
                    else
                    {
                        stack_sizes[l] = cur_stack_size;
                        stack.push_back({
                            .cur_id = l,
                            .cur_stack_size = cur_stack_size,
                            .cur_header = cur_header,
                        });
                    }
                };

                auto check_push_call = [&](int32_t l)
                {
                    assert_with_ip(l >= 0 && l < code.code_size, cur_id, "Tried to call function outside of code");
                    auto header = code.get_by_id(l);
                    assert_with_ip(header->tag == instr::BEGIN || header->tag == instr::CBEGIN, cur_id, "Tried to call not a function");
                    if (stack_sizes[l] < 0) {
                        stack_sizes[l] = 0;
                        stack.push_back({
                            .cur_id = l,
                            .cur_stack_size = cur_stack_size,
                            .cur_header = header,
                        });
                    }
                };

                auto check_access = [&](Instruction::CArg::CArgType typ, int32_t a)
                {
                    switch (typ)
                    {
                    case Instruction::CArg::G:
                        assert_with_ip(a >= 0 && a < res.header.globals_length, cur_id, "Trying to access invalid global");
                        return;
                    case Instruction::CArg::L:
                        assert_with_ip(a >= 0 && a < locs, cur_id, "Trying to access invalid local");
                        return;
                    case Instruction::CArg::A:
                        assert_with_ip(a >= 0 && a < cur_header->args[0], cur_id, "Trying to access invalid argument");
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
                    check_next_jump(cur->args[0]);
                    // Skip default next iter
                    continue;
                case instr::END:
                case instr::RET:
                case instr::FAIL:
                    cur = nullptr;
                    continue;
                case instr::CALL:
                    check_push_call(cur->args[0]);
                    break;
                case instr::CJMPZ:
                case instr::CJMPNZ:
                    check_push_jump(cur->args[0]);
                    break;
                case instr::CLOSURE:
                    check_push_call(cur->args[0]);
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

                if (cur != nullptr) {
                    cur_id = code.to_id(cur);
                    assert_with_ip(stack_sizes[cur_id] < 0 || cur_stack_size == stack_sizes[cur_id] || cur->tag == instr::END || cur->tag == instr::RET, cur_id, "Stack sizes don't match");
                    if (stack_sizes[cur_id] >= 0)
                    {
                        break;
                    }
                }
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