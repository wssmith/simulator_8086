#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "decoder.hpp"
#include "overloaded.hpp"

namespace
{
    std::vector<uint8_t> read_binary_file(const std::string& path)
    {
        std::ifstream input_file{ path, std::ios::in | std::ios::binary };

        std::vector<uint8_t> data;
        std::for_each(std::istreambuf_iterator(input_file),
            std::istreambuf_iterator<char>(),
            [&data](char c)
            {
                data.push_back(c);
            });

        return data;
    }
}

int main(int argc, char* argv[])
{
    constexpr int expected_args = 2;
    if (argc != expected_args)
    {
        std::cout << "Usage: InstructionDecode8086 input_file\n";
        return 1;
    }

    try
    {
        // read binary file
        const char* input_path = argv[1];
        const std::vector<uint8_t> data = read_binary_file(input_path);

        // read instructions
        auto data_iter = data.cbegin();
        const auto data_end = data.cend();

        while (data_iter != data_end)
        {
            std::optional<instruction> inst_result = decode_instruction(data_iter, data_end);
            if (!inst_result.has_value())
                continue;

            instruction inst = inst_result.value();

            // print instructions
            const char* mnemonic = get_mneumonic(inst.op);

            auto match_operand = overloaded
            {
                [](const effective_address_expression& address_op) { return std::string(get_register_name(address_op.terms[0].reg)); },
                [](const register_access& register_op) { return std::string(get_register_name(register_op)); },
                [](immediate immediate_op) { return std::to_string(immediate_op.value); },
                [](std::monostate) { return std::string(""); },
            };

            std::string first_operand = std::visit(match_operand, inst.operands[0]);
            std::string second_operand = std::visit(match_operand, inst.operands[1]);

            std::cout << mnemonic << ' ' << first_operand;
            if (second_operand.length() > 0)
                std::cout << ", " << second_operand;
            std::cout << '\n';
        }
    }
    catch (std::exception& ex)
    {
        std::cout << "ERROR!! " << ex.what() << '\n';
    }
    catch (...)
    {
        std::cout << "UNKNOWN ERROR!!\n";
    }
}