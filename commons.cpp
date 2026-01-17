#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <stdint.h>
#include "commons.h"

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

namespace instr
{
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

size_t Instruction::get_args_length() const
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

bool Instruction::is_hex_arg(size_t arg) const
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

bool Instruction::is_closure() const
{
    switch (tag)
    {
    case instr::CLOSURE:
        return true;

    default:
        return false;
    }
}

const char *Instruction::get_tag_name() const
{
    return instr::name(tag);
}

size_t Instruction::size() const
{
    size_t closure_args = 0;
    if (is_closure())
    {
        closure_args = args[1] * (sizeof(char) + sizeof(int32_t));
    }
    return sizeof(char) + sizeof(int32_t) * get_args_length() + closure_args;
}

bool Instruction::operator==(const Instruction &other) const
{
    return memcmp(this, &other, size()) == 0;
}

int32_t Instruction::get_popped()
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

int32_t Instruction::get_pushed()
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

int32_t Instruction::get_diff()
{
    return get_pushed() - get_popped();
}

Instruction *Code::get_by_id(int32_t id) const
{
    assert(id >= 0 && id < code_size, "Tried to get instruction outside of code");
    return reinterpret_cast<Instruction *>(code + id);
}

int32_t Code::to_id(Instruction *_ins) const
{
    auto res = reinterpret_cast<char *>(_ins) - code;
    assert(res >= 0 && res < code_size, "Tried to get id outside of code");
    return res;
}

Instruction *Code::get_next(Instruction *_ins) const
{
    auto id = to_id(_ins);
    if (id + _ins->size() >= code_size)
    {
        return nullptr;
    }
    return get_by_id(id + _ins->size());
}

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
