#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "flag_utils.hpp"
#include "decoder.hpp"
#include "overloaded.hpp"
#include "instruction.hpp"
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

        if (!input_file.is_open())
            throw std::exception("Cannot open binary file.");

        std::vector<uint8_t> data;
        std::for_each(std::istreambuf_iterator(input_file),
            std::istreambuf_iterator<char>(),
            [&data](char c)
            {
                data.push_back(c);
            });

        return data;
    }

    std::string print_width(const instruction& inst)
    {
        return has_any_flag(inst.flags, instruction_flags::wide) ? "word" : "byte";
    }

    std::string print_instruction(const instruction& inst)
    {
        using std::string_literals::operator ""s;

        const char* mnemonic = get_mneumonic(inst.op);

        auto matcher = overloaded
        {
            [&inst](const effective_address_expression& address_op)
            {
                std::string address_text;

                if (std::holds_alternative<immediate>(inst.operands[1]))
                    address_text += print_width(inst) + " ";

                address_text += "["s + get_register_name(address_op.term1.reg);

                if (address_op.term2.has_value())
                    address_text += " + "s + get_register_name(address_op.term2.value().reg);

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
                    direct_address_text += print_width(inst) + " ";

                direct_address_text += "[" + std::to_string(direct_address_op.address) + "]";

                return direct_address_text;
            },
            [](const register_access& register_op) -> std::string { return get_register_name(register_op); },
            [&inst](immediate immediate_op)
            {
                if (has_any_flag(immediate_op.flags, immediate_flags::relative_jump_displacement))
                {
                    const int32_t value = immediate_op.value + static_cast<int32_t>(inst.size);
                    return "$" + (value >= 0 ? "+" + std::to_string(value) : "-" + std::to_string(-value));
                }

                return std::to_string(immediate_op.value);
            },
            [](std::monostate) { return ""s; }
        };

        const std::string first_operand = std::visit(matcher, inst.operands[0]);
        const std::string second_operand = std::visit(matcher, inst.operands[1]);

        std::string asm_line = mnemonic + " "s + first_operand;
        if (second_operand.length() != 0)
            asm_line += ", " + second_operand;

        return asm_line;
    }

    void print_state_transition(std::ostringstream& stream, const char* destination_register, int width, uint16_t old_value, uint16_t new_value)
    {
        std::ostringstream builder;
        builder << destination_register << ":0x" << std::hex << old_value << "->0x" << new_value << std::dec;

        stream << std::left << std::setw(width) << std::fixed << std::setfill(' ');
        stream << builder.str();
    }

    void print_flags_transition(std::ostringstream& stream, const char* flag_register, int width, const simulation_step& step)
    {
        using std::string_literals::operator ""s;

        stream << std::left << std::setw(width) << std::fixed << std::setfill(' ');
        stream << flag_register + ":"s + get_flag_string(step.old_flags) + "->" + get_flag_string(step.new_flags);
    }

    void print_literal(std::ostringstream& stream, const char* literal, int width)
    {
        stream << std::left << std::setw(width) << std::fixed << std::setfill(' ') << literal;
    }

    void print_empty_column(std::ostringstream& stream, int width)
    {
        print_literal(stream, "", width);
    }
    
    std::string print_simulation_step(const simulation_step& step)
    {
        constexpr int column_width = 20;
        std::ostringstream builder;

        if (step.new_value != step.old_value)
        {
            const char* destination_register = get_register_name(step.destination);
            print_state_transition(builder, destination_register, column_width, step.old_value, step.new_value);
        }
        else
        {
            print_empty_column(builder, column_width);
        }

        const auto ip_name = get_register_name({ .index = instruction_pointer_index, .offset = 0, .count = 2 });
        print_state_transition(builder, ip_name, column_width, step.old_ip, step.new_ip);

        if (step.new_flags != step.old_flags)
        {
            const auto flags_name = get_register_name({ .index = flags_index, .offset = 0, .count = 2 });
            print_flags_transition(builder, flags_name, 10, step);
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

            char const* register_name = get_register_name({ .index = i, .offset = 0, .count = 2 });

            if (i == flags_index)
            {
                const auto flags = static_cast<control_flags>(registers[flags_index]);

                builder << std::right << std::setw(8) << std::fixed << std::setfill(' ');
                builder << register_name << ": " << get_flag_string(flags) << '\n';
            }
            else
            {
                builder << std::right << std::setw(8) << std::fixed << std::setfill(' ');
                builder << register_name << ": 0x" << std::hex << std::setfill('0') << std::setw(4) << reg_value << std::dec << std::setw(0) << " (" << reg_value << ")\n";
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
        return EXIT_FAILURE;
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
            return EXIT_FAILURE;
        }
    }

    std::string input_filename = std::filesystem::path(app_args.input_path).filename().string();
    const char* action = app_args.execute_mode ? "execution" : "decoding";
    std::cout << "--- " << input_filename << " " << action << " --- \n\n";

    try
    {
        // read binary file
        const std::vector<uint8_t> data = read_binary_file(app_args.input_path);

        // read instructions
        auto data_iter = data.cbegin();
        const auto data_end = data.cend();

        std::vector<instruction> instruction_list;
        uint32_t current_address = 0;

        std::array<uint16_t, register_count> registers = {};

        // decode instruction
        while (data_iter < data_end)
        {
            std::optional<instruction> inst_result = decode_instruction(data_iter, data_end, current_address);
            if (!inst_result.has_value())
                continue;

            instruction_list.push_back(inst_result.value());

            const instruction& inst = instruction_list.back();
            current_address += inst.size;

            // print instruction
            constexpr int column_width = 20;
            std::string asm_line = print_instruction(inst);
            std::cout << std::left << std::setw(column_width) << std::fixed << std::setfill(' ');
            std::cout << asm_line;

            // execute instruction?
            if (app_args.execute_mode)
            {
                simulation_step step = simulate_instruction(inst, registers);

                int32_t actual_ip_change = step.new_ip - step.old_ip;
                int32_t delta = (actual_ip_change - static_cast<int32_t>(inst.size));
                if (delta != 0)
                    std::advance(data_iter, delta);

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

    return EXIT_SUCCESS;
}
