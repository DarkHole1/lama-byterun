#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include "commons.h"

struct Analyser
{
    Result result;
    Code code;
    // Bitvectors
    std::vector<bool> visited, boundary;

    std::vector<std::tuple<uint32_t, uint32_t>> occurencies;
    std::vector<std::tuple<uint32_t, uint32_t>> double_occurencies;

    Analyser(Result res) : result(res), code(Code(res.code, res.code_size))
    {
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
            if (visited[result.pubs[i].b])
            {
                continue;
            }
            stack.push_back(i);
            visited[result.pubs[i].b] = true;
            boundary[result.pubs[i].b] = true;
        }

        for (; stack.size() != 0;)
        {
            auto cur = code.get_by_id(stack.back());
            stack.pop_back();
            while (cur != nullptr)
            {
                auto cur_id = code.to_id(cur);

                switch (cur->tag)
                {
                case instr::JMP:
                    boundary[cur_id] = true;
                    boundary[cur->args[0]] = true;
                    cur = code.get_by_id(cur->args[0]);
                    goto CHECK_NEXT;
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
                {
                    assert(cur->args[0] >= 0 && cur->args[0] < code.code_size, "Tried to create closure outside of code");
                    auto jmp = cur->args[0];
                    if (!visited[jmp])
                    {
                        stack.push_back(jmp);
                        visited[jmp] = true;
                    }
                    break;
                }
                default:
                    break;
                }

                cur = code.get_next(cur);

            CHECK_NEXT:
                if (cur != nullptr)
                {
                    cur_id = code.to_id(cur);
                    if (visited[cur_id])
                    {
                        break;
                    }
                    visited[cur_id] = true;
                }
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
        for (auto cur = code.get_by_id(0); cur != nullptr;)
        {
            auto cur_id = code.to_id(cur);
            // std::cout << (reachable[cur_id] ? "* " : "  ") << *cur << "\n";
            if (!visited[cur_id])
            {
                prev = nullptr;
                cur = code.get_next_inc(cur);
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
            cur = code.get_next(cur);
        }
    }

    void analyse()
    {
        mark_instructions();
        count_occurencies();
        sort_occurencies();
    }
};

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

int main(int argc, char **argv)
{
    assert(argc >= 2, "No input file");

    std::string fname = argv[1];

    std::vector<char> bytes = read_file(fname);

    Result result = parse_and_validate(std::move(bytes));

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