#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "flag_utils.hpp"
#include "decoder.hpp"
#include "overloaded.hpp"
#include "instruction.hpp"
#include "simulator.hpp"

namespace
{
    using namespace std::string_literals;

    struct sim86_arguments
    {
        const char* input_path = nullptr;
        bool execute_mode{};
        bool dump_memory{};
    };

    std::vector<uint8_t> read_binary_file(const std::string& path)
    {
        std::vector<uint8_t> data;

        std::ifstream input_file{ path, std::ios::in | std::ios::binary };

        if (!input_file.is_open())
            throw std::exception{ "Cannot open binary file." };

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

                if (has_any_flag(inst.flags, instruction_flags::wide))
                    return std::to_string(static_cast<uint16_t>(immediate_op.value));
                else
                    return std::to_string(static_cast<uint8_t>(immediate_op.value));

            },
            [](std::monostate) { return ""s; }
        };

        const std::string first_operand = std::visit(matcher, inst.operands[0]);
        const std::string second_operand = std::visit(matcher, inst.operands[1]);

        std::string asm_line = mnemonic;
        if (first_operand.length() != 0)
            asm_line += " " + first_operand;
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

    std::string print_register_contents()
    {
        std::ostringstream builder;

        for (size_t i = 0; i < registers.size(); ++i)
        {
            const uint16_t reg_value = registers[i];
            if (reg_value == 0)
                continue;

            const auto index = static_cast<register_index>(i);
            char const* register_name = get_register_name({ .index = index, .offset = 0, .count = 2 });

            if (i == flags_index)
            {
                const auto flags = control_flags{ registers[flags_index] };

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

    void save_memory_dump(const char* filename)
    {
        std::ofstream memory_dump;
        memory_dump.open(filename, std::ios::out | std::ios::binary);

        for (const uint8_t b : memory)
            memory_dump << b;

        memory_dump.close();
    }
}

int main(int argc, char* argv[])
{
    // read command line arguments
    constexpr int min_expected_args = 2;
    constexpr const char* usage_message = "Usage: InstructionDecode8086 [-exec] [-dump] input_file";

    if (argc < min_expected_args)
    {
        std::cout << usage_message << '\n';
        return EXIT_FAILURE;
    }

    constexpr int not_found = -1;
    int invalid_option_index = not_found;
    const std::unordered_set<std::string> valid_options = { "-exec", "-dump" };

    std::unordered_set<std::string> options;
    for (int i = 1; i < (argc - 1); ++i)
    {
        std::string option = argv[i];
        std::transform(option.begin(), option.end(), option.begin(), [](char c) { return std::tolower(c); });

        if (valid_options.contains(option))
        {
            options.emplace(option);
        }
        else
        {
            invalid_option_index = i;
            break;
        }
    }

    sim86_arguments app_args;
    if (invalid_option_index == not_found)
    {
        app_args = sim86_arguments
        {
            .input_path = argv[argc - 1],
            .execute_mode = options.contains("-exec"),
            .dump_memory = options.contains("-dump")
        };
    }
    else
    {
        std::cout << "Unrecognized argument '" << argv[invalid_option_index] << "'.\n\n" << usage_message << '\n';
        return EXIT_FAILURE;
    }

    std::string input_filename = std::filesystem::path(app_args.input_path).filename().string();
    const char* action = app_args.execute_mode ? "execution" : "decoding";
    std::cout << "--- " << input_filename << " " << action << " --- \n\n";

    try
    {
        // read binary instructions into a buffer, then copy its contents to the code segment in memory via a view
        std::span<uint8_t> data;
        {
            constexpr int segment_size = 64 * 1024;
            constexpr auto cs_location = 0;
            //constexpr auto cs_location = 64 * 4 * 63;
            //constexpr auto cs_location = memory.size() - segment_size;

            registers[code_segment_index] = cs_location >> 4;

            std::vector<uint8_t> data_buffer = read_binary_file(app_args.input_path);

            if (data_buffer.size() > segment_size)
                throw std::exception{ "Instructions must fit within a single memory segment." };

            constexpr uint8_t* code_segment = memory.data() + cs_location;
            data = std::span{ code_segment, data_buffer.size() };
            std::copy(data_buffer.cbegin(), data_buffer.cend(), data.begin());
        }
        
        // read instructions
        auto data_iter = data.begin();
        const auto data_end = data.end();

        uint32_t current_address = 0;

        // decode instruction
        while (data_iter < data_end)
        {
            instruction inst = decode_instruction(data_iter, data_end, current_address);
            current_address += inst.size;

            // print instruction
            constexpr int column_width = 22;
            std::string asm_line = print_instruction(inst);
            std::cout << std::left << std::setw(column_width) << std::fixed << std::setfill(' ');
            std::cout << asm_line;

            // execute instruction?
            if (app_args.execute_mode)
            {
                simulation_step step = simulate_instruction(inst);

                const int32_t actual_ip_change = step.new_ip - step.old_ip;
                const int32_t delta = (actual_ip_change - static_cast<int32_t>(inst.size));
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
            const std::string register_contents = print_register_contents();
            std::cout << "\nFinal registers:\n" << register_contents;

            if (app_args.dump_memory)
            {
                // save memory to a file
                constexpr auto dump_filename = "dump.data";
                save_memory_dump(dump_filename);
                std::cout << "\nSaved memory to '" << dump_filename << "'.\n";
            }
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
