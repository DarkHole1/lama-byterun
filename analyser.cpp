#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include "commons.h"

struct Analyser
{
    Result result;
    Code code;
    // Bitvectors
    std::vector<bool> visited, boundary;

    std::vector<int32_t> occurencies;
    std::vector<int32_t> double_occurencies;
    std::vector<std::tuple<int32_t, int32_t>> occurencies_u;
    std::vector<std::tuple<int32_t, int32_t>> double_occurencies_u;

    Analyser(Result res) : result(res), code(Code(res.code, res.code_size))
    {
        visited.resize(res.code_size, false);
        boundary.resize(res.code_size, false);
    }

    void sort_occurencies()
    {
        std::sort(occurencies.begin(), occurencies.end(),
                  [this](const auto &a, const auto &b)
                  {
                      return code.get_by_id(b)->cmp(code.get_by_id(a)) < 0;
                  });

        std::sort(double_occurencies.begin(), double_occurencies.end(),
                  [this](const auto &a, const auto &b)
                  {
                      auto a_ = code.get_by_id(a);
                      auto b_ = code.get_by_id(b);
                      auto res = b_->cmp(a_);
                      if (res == 0)
                      {
                          return code.get_next(b_)->cmp(code.get_next(a_)) < 0;
                      }
                      return res < 0;
                  });

        int32_t uniq1 = 1;
        int32_t uniq2 = 1;

        for (int32_t i = 1; i < occurencies.size(); i++)
        {
            if (code.get_by_id(occurencies[i - 1])->cmp(code.get_by_id(occurencies[i])) != 0)
            {
                uniq1 += 1;
            }
        }

        for (int32_t i = 1; i < double_occurencies.size(); i++)
        {
            auto a_ = code.get_by_id(double_occurencies[i - 1]);
            auto b_ = code.get_by_id(double_occurencies[i]);
            if (a_->cmp(b_) != 0 || code.get_next(a_)->cmp(code.get_next(b_)) != 0)
            {
                uniq2 += 1;
            }
        }

        occurencies_u.reserve(uniq1);
        double_occurencies_u.reserve(uniq2);

        if (occurencies.size() > 0)
        {
            occurencies_u.emplace_back(occurencies[0], 1);

            for (int32_t i = 1; i < occurencies.size(); i++)
            {
                if (code.get_by_id(occurencies[i - 1])->cmp(code.get_by_id(occurencies[i])) != 0)
                {
                    occurencies_u.emplace_back(occurencies[i], 1);
                }
                else
                {
                    auto c = occurencies_u[occurencies_u.size() - 1];
                    occurencies_u[occurencies_u.size() - 1] = std::tuple(std::get<0>(c), std::get<1>(c) + 1);
                }
            }
        }

        if (double_occurencies.size() > 0)
        {
            double_occurencies_u.emplace_back(double_occurencies[0], 1);

            for (int32_t i = 1; i < double_occurencies.size(); i++)
            {
                auto a_ = code.get_by_id(double_occurencies[i - 1]);
                auto b_ = code.get_by_id(double_occurencies[i]);
                if (a_->cmp(b_) != 0 || code.get_next(a_)->cmp(code.get_next(b_)) != 0)
                {
                    double_occurencies_u.emplace_back(double_occurencies[i], 1);
                }
                else
                {
                    auto c = double_occurencies_u[double_occurencies_u.size() - 1];
                    double_occurencies_u[double_occurencies_u.size() - 1] = std::tuple(std::get<0>(c), std::get<1>(c) + 1);
                }
            }
        }

        std::sort(occurencies_u.begin(), occurencies_u.end(),
                  [](const auto &a, const auto &b)
                  {
                      return std::get<1>(a) > std::get<1>(b);
                  });

        std::sort(double_occurencies_u.begin(), double_occurencies_u.end(),
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
                {
                    assert(cur->args[0] >= 0 && cur->args[0] < code.code_size, "Tried to call outside of code");
                    boundary[cur_id] = true;
                    auto jmp = cur->args[0];
                    if (!visited[jmp])
                    {
                        stack.push_back(jmp);
                        visited[jmp] = true;
                    }
                    auto next = code.get_next(cur);
                    if (next != nullptr)
                    {
                        boundary[code.to_id(next)] = true;
                    }
                    break;
                }
                case instr::CJMPZ:
                case instr::CJMPNZ:
                {
                    assert(cur->args[0] >= 0 && cur->args[0] < code.code_size, "Tried to jump outside of code");
                    auto jmp = cur->args[0];
                    if (!visited[jmp])
                    {
                        stack.push_back(jmp);
                        visited[jmp] = true;
                    }
                    break;
                }
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
        occurencies.push_back(code.to_id(_inst));
    }

    void add_double_instr(Instruction *_inst)
    {
        double_occurencies.push_back(code.to_id(_inst));
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

            if (cur->get_args_length() > 0)
            {
                add_instr(cur);
            }
            if (prev != nullptr && prev->get_args_length() + cur->get_args_length() > 0)
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

    for (int32_t i = 0, j = 0; i < a.occurencies_u.size() || j < a.double_occurencies_u.size();)
    {
        if (i >= a.occurencies_u.size())
        {
            auto occ = a.double_occurencies_u[j++];
            print_occurency(code, occ, 2);
            continue;
        }
        if (j >= a.double_occurencies_u.size())
        {
            auto occ = a.occurencies_u[i++];
            print_occurency(code, occ, 1);
            continue;
        }
        auto occ1 = a.occurencies_u[i];
        auto occ2 = a.double_occurencies_u[j];
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