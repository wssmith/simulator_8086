#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "control_flags.hpp"
#include "decoder.hpp"
#include "overloaded.hpp"
#include "simulator.hpp"

namespace
{
    struct sim86_arguments
    {
        const char* input_path = nullptr;
        bool execute_mode{};
    };

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
                    const bool is_word = (inst.flags & static_cast<uint8_t>(instruction_flags::wide)) != 0;
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
                    const bool is_word = (inst.flags & static_cast<uint8_t>(instruction_flags::wide)) != 0;
                    direct_address_text += (is_word ? "word " : "byte ");
                }

                direct_address_text += "[" + std::to_string(direct_address_op.address) + "]";

                return direct_address_text;
            },
            [](const register_access& register_op) { return std::string(get_register_name(register_op)); },
            [](immediate immediate_op) { return std::to_string(immediate_op.value); },
            [](std::monostate) { return std::string(); }
        };

        const std::string first_operand = std::visit(match_operand, inst.operands[0]);
        const std::string second_operand = std::visit(match_operand, inst.operands[1]);

        std::string asm_line = std::string(mnemonic) + " " + first_operand;
        if (second_operand.length() != 0)
            asm_line += ", " + second_operand;

        return asm_line;
    }

    std::string print_simulation_step(const simulation_step& step)
    {
        std::ostringstream builder;

        if (step.new_value != step.old_value)
        {
            const char* destination_register = get_register_name(step.destination);
            builder << destination_register << ":0x" << std::hex << step.old_value << "->0x" << step.new_value << std::dec << " ";
        }

        builder << "ip:0x" << std::hex << step.old_ip << "->0x" << step.new_ip << std::dec << " ";

        if (step.new_flags != step.old_flags)
        {
            builder << "flags:" << get_flag_string(step.old_flags) << "->" << get_flag_string(step.new_flags);
        }

        return builder.str();
    }

    std::string print_register_contents(std::array<uint16_t, register_count> registers)
    {
        std::ostringstream builder;

        for (size_t i = 0; i < registers.size(); ++i)
        {
            const uint16_t reg_value = registers[i];
            if (reg_value == 0)
                continue;

            char const* register_name = get_register_name(register_access{ .index = i, .offset = 0, .count = 2 });

            if (i == flags_register_index)
            {
                const auto flags = static_cast<control_flags>(registers[flags_register_index]);

                builder << "   " << register_name << ": " << get_flag_string(flags) << '\n';
            }
            else
            {
                builder << "      " << register_name << ": 0x" << std::hex << std::setfill('0') << std::setw(4) << reg_value << std::dec << std::setw(0) << " (" << reg_value << ")\n";
            }
        }

        return builder.str();
    }
}

int main(int argc, char* argv[])
{
    // read command line arguments
    constexpr int min_expected_args = 2;
    constexpr const char* usage_message = "Usage: InstructionDecode8086 [-exec] input_file";

    if (argc < min_expected_args)
    {
        std::cout << usage_message << '\n';
        return 1;
    }

    sim86_arguments app_args{};
    if (argc == min_expected_args)
    {
        app_args.input_path = argv[1];
    }
    else
    {
        if (std::strcmp(argv[1], "-exec") == 0)
        {
            app_args.execute_mode = true;
            app_args.input_path = argv[2];
        }
        else
        {
            std::cout << "Unrecognized argument '" << argv[1] << "'.\n\n" << usage_message << '\n';
            return 1;
        }
    }

    try
    {
        // read binary file
        const std::vector<uint8_t> data = read_binary_file(app_args.input_path);

        // read instructions
        auto data_iter = data.cbegin();
        const auto data_end = data.cend();

        std::vector<instruction> instruction_list;
        uint32_t current_address = 0;

        std::array<uint16_t, register_count> registers{ };

        // decode instruction
        while (data_iter != data_end)
        {
            std::optional<instruction> inst_result = decode_instruction(data_iter, data_end);
            if (!inst_result.has_value())
                continue;

            instruction_list.push_back(inst_result.value());

            instruction& inst = instruction_list.back();
            inst.address = current_address;
            current_address += inst.size;

            // print instruction
            std::string asm_line = print_instruction(inst);
            std::cout << asm_line;

            // execute instruction?
            if (app_args.execute_mode)
            {
                simulation_step step = simulate_instruction(inst, registers);
                std::string sim_line = print_simulation_step(step);

                std::cout << " ; " << sim_line;
            }

            std::cout << '\n';
        }

        if (app_args.execute_mode)
        {
            // print final contents of registers
            const std::string register_contents = print_register_contents(registers);

            std::cout << "\nFinal registers:\n" << register_contents;
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
