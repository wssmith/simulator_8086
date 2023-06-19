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

    std::string print_instruction(const instruction& inst)
    {
        const char* mnemonic = get_mneumonic(inst.op);

        auto match_operand = overloaded
        {
            [&inst](const effective_address_expression& address_op)
            {
                std::string address_text;

                if (std::holds_alternative<immediate>(inst.operands[1]))
                {
                    const bool is_word = (inst.flags & static_cast<uint8_t>(instruction_flag::inst_wide)) != 0;
                    address_text += (is_word ? "word " : "byte ");
                }

                address_text += "[" + std::string(get_register_name(address_op.term1.reg));

                if (address_op.term2.has_value())
                    address_text += " + " + std::string(get_register_name(address_op.term2.value().reg));

                if (address_op.displacement > 0)
                    address_text += " + " + std::to_string(address_op.displacement);
                else if (address_op.displacement < 0)
                    address_text += " - " + std::to_string(-address_op.displacement);

                address_text += "]";
                return address_text;
            },
            [&inst](direct_address direct_address_op)
            {
                std::string direct_address_text;

                if (std::holds_alternative<immediate>(inst.operands[1]))
                {
                    const bool is_word = (inst.flags & static_cast<uint8_t>(instruction_flag::inst_wide)) != 0;
                    direct_address_text += (is_word ? "word " : "byte ");
                }

                direct_address_text += "[" + std::to_string(direct_address_op.address) + "]";

                return direct_address_text;
            },
            [](const register_access& register_op) { return std::string(get_register_name(register_op)); },
            [](immediate immediate_op) { return std::to_string(immediate_op.value); },
            [](std::monostate) { return std::string(); },
        };

        const std::string first_operand = std::visit(match_operand, inst.operands[0]);
        const std::string second_operand = std::visit(match_operand, inst.operands[1]);

        std::string asm_line = std::string(mnemonic) + " " + first_operand;
        if (second_operand.length() != 0)
            asm_line += ", " + second_operand;

        return asm_line;
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

    // todo: keep track of address

    try
    {
        // read binary file
        const char* input_path = argv[1];
        const std::vector<uint8_t> data = read_binary_file(input_path);

        // read instructions
        auto data_iter = data.cbegin();
        const auto data_end = data.cend();

        uint32_t current_address = 0;

        while (data_iter != data_end)
        {
            std::optional<instruction> inst_result = decode_instruction(data_iter, data_end);
            if (!inst_result.has_value())
                continue;

            instruction inst = inst_result.value();
            inst.address = current_address;
            current_address += inst.size;

            // print instructions
            std::string asm_line = print_instruction(inst);
            std::cout << asm_line << '\n';
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
