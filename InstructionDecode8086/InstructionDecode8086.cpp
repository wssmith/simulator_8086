// InstructionDecode8086.cpp

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "instruction.hpp"
#include "opcode.hpp"

namespace
{
    using data_iterator = std::vector<uint8_t>::const_iterator;

    constexpr std::array registers =
    {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
    };

    constexpr std::array effective_addresses =
    {
        "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"
    };

    constexpr const char* mov_name = "mov";
    constexpr const char* add_name = "add";
    constexpr const char* sub_name = "sub";
    constexpr const char* cmp_name = "cmp";

    std::unordered_map<opcode, const char*> opcodes
    {
        { opcode::mov_normal, mov_name },
        { opcode::mov_immediate_to_register_or_memory, mov_name },
        { opcode::mov_immediate_to_register, mov_name },
        { opcode::mov_memory_to_accumulator, mov_name },
        { opcode::mov_accumulator_to_memory, mov_name },
        { opcode::mov_to_segment_register, mov_name },
        { opcode::mov_from_segment_register, mov_name },

        { opcode::add_normal, add_name },
        { opcode::add_immediate_to_register_or_memory, add_name },
        { opcode::add_immediate_to_accumulator, add_name },

        { opcode::sub_normal, sub_name },
        { opcode::sub_immediate_from_register_or_memory, sub_name },
        { opcode::sub_immediate_from_accumulator, sub_name },

        { opcode::cmp_normal, cmp_name },
        { opcode::cmp_immediate_with_register_or_memory, cmp_name },
        { opcode::cmp_immediate_with_accumulator, cmp_name },

        { opcode::je, "je" },
        { opcode::jl, "jl" },
        { opcode::jle, "jle" },
        { opcode::jb, "jb" },
        { opcode::jbe, "jbe" },
        { opcode::jp, "jp" },
        { opcode::jo, "jo" },
        { opcode::js, "js" },
        { opcode::jne, "jne" },
        { opcode::jnl, "jnl" },
        { opcode::jg, "jg" },
        { opcode::jnb, "jnb" },
        { opcode::ja, "ja" },
        { opcode::jnp, "jnp" },
        { opcode::jno, "jno" },
        { opcode::jns, "jns" },
        { opcode::loop, "loop" },
        { opcode::loopz, "loopz" },
        { opcode::loopnz, "loopnz" },
        { opcode::jcxz, "jcxz" }
    };

    std::vector<std::unordered_map<uint8_t, opcode>> opcode_maps
    {
        {
            { 0b1000'1110, opcode::mov_to_segment_register },
            { 0b1000'1100, opcode::mov_from_segment_register },

            { 0b0111'0100 , opcode::je },
            { 0b0111'1100 , opcode::jl },
            { 0b0111'1110 , opcode::jle },
            { 0b0111'0010 , opcode::jb },
            { 0b0111'0110 , opcode::jbe },
            { 0b0111'1010 , opcode::jp },
            { 0b0111'0000 , opcode::jo },
            { 0b0111'1000 , opcode::js },
            { 0b0111'0101 , opcode::jne },
            { 0b0111'1101 , opcode::jnl },
            { 0b0111'1111 , opcode::jg },
            { 0b0111'0011 , opcode::jnb },
            { 0b0111'0111 , opcode::ja },
            { 0b0111'1011 , opcode::jnp },
            { 0b0111'0001 , opcode::jno },
            { 0b0111'1001 , opcode::jns },
            { 0b1110'0010 , opcode::loop },
            { 0b1110'0001 , opcode::loopz },
            { 0b1110'0000 , opcode::loopnz },
            { 0b1110'0011 , opcode::jcxz },
        },
        {
            { 0b1100'011, opcode::mov_immediate_to_register_or_memory },
            { 0b1010'000, opcode::mov_memory_to_accumulator },
            { 0b1010'001, opcode::mov_accumulator_to_memory },

            { 0b0000'010, opcode::add_immediate_to_accumulator },
            { 0b0010'110, opcode::sub_immediate_from_accumulator },
            { 0b0011'110, opcode::cmp_immediate_with_accumulator }
        },
        {
            { 0b1000'10, opcode::mov_normal },

            { 0b0000'00, opcode::add_normal },
            { 0b0010'10, opcode::sub_normal },
            { 0b0011'10, opcode::cmp_normal },
            { 0b1000'00, opcode::arithmetic_immediate }
        },
        {},
        {
            { 0b1011, opcode::mov_immediate_to_register }
        }
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

    opcode read_opcode(uint8_t b)
    {
        for (size_t i = 0; i < opcode_maps.size(); ++i)
        {
            std::unordered_map<uint8_t, opcode>& opcode_map = opcode_maps[i];

            if (uint8_t key = b >> i; opcode_map.contains(key))
                return opcode_map[key];
        }

        return opcode::none;
    }

    int8_t get_displacement_bytes(uint8_t mod, uint8_t rm)
    {
        switch (mod)
        {
            case 0b00: // memory mode, no displacement unless direct address
                return (rm == 0b110) * 2;
            case 0b01: // memory mode, 8-bit displacement
                return 1;
            case 0b10: // memory mode, 16-bit displacement
                return 2;
            case 0b11: // register mode, no displacement
            default:
                return 0;
        }
    }

    bool read_and_advance(data_iterator& iter, const data_iterator& iter_end, uint8_t& b)
    {
        if (iter == iter_end)
            return false;

        b = *iter++;

        return true;
    }

    bool read_displacement(data_iterator& iter, const data_iterator& iter_end, instruction& inst)
    {
        const int8_t displacement_bytes = get_displacement_bytes(inst.mod, inst.rm);

        if (displacement_bytes > 0)
        {
            if (!read_and_advance(iter, iter_end, inst.disp_lo))
                return false;

            if (displacement_bytes > 1)
            {
                if (!read_and_advance(iter, iter_end, inst.disp_hi))
                    return false;
            }
        }

        return true;
    }

    bool read_data(data_iterator& iter, const data_iterator& iter_end, instruction& inst)
    {
        if (!read_and_advance(iter, iter_end, inst.data_lo))
            return false;

        if (inst.w && !inst.s)
        {
            if (!read_and_advance(iter, iter_end, inst.data_hi))
                return false;
        }

        return true;
    }

    std::optional<instruction> read_instruction(data_iterator& data_iter, const data_iterator& data_end)
    {
        instruction inst{};

        uint8_t b = 0;
        if (!read_and_advance(data_iter, data_end, b))
            return {};

        inst.opcode = read_opcode(b);

        switch (inst.opcode)
        {
            case opcode::mov_normal:
            case opcode::add_normal:
            case opcode::sub_normal:
            case opcode::cmp_normal:
            {
                inst.w = b & 1;
                b >>= 1;
                inst.d = b & 1;

                if (!read_and_advance(data_iter, data_end, b))
                    return {};

                inst.rm = b & 0b0111;
                b >>= 3;
                inst.reg = b & 0b0111;
                b >>= 3;
                inst.mod = b;

                if (!read_displacement(data_iter, data_end, inst))
                    return {};

                break;
            }

            case opcode::arithmetic_immediate:
                inst.s = (b >> 1) & 1;
                [[fallthrough]];

            case opcode::mov_immediate_to_register_or_memory:
            {
                inst.w = b & 1;

                if (!read_and_advance(data_iter, data_end, b))
                    return {};

                inst.rm = b & 0b0111;
                b >>= 3;
                uint8_t op = b & 0b0111;
                b >>= 3;
                inst.mod = b;

                if (inst.opcode == opcode::arithmetic_immediate)
                {
                    inst.opcode = [&op]
                    {
                        switch (op)
                        {
                            case 0b000: return opcode::add_immediate_to_register_or_memory;
                            case 0b101: return opcode::sub_immediate_from_register_or_memory;
                            case 0b111: return opcode::cmp_immediate_with_register_or_memory;
                            default:    return opcode::none;
                        }
                    }();
                }

                if (!read_displacement(data_iter, data_end, inst))
                    return {};
                if (!read_data(data_iter, data_end, inst))
                    return {};

                break;
            }

            case opcode::mov_immediate_to_register:
                inst.reg = b & 0b111;
                b >>= 3;
                [[fallthrough]];

            case opcode::add_immediate_to_accumulator:
            case opcode::sub_immediate_from_accumulator:
            case opcode::cmp_immediate_with_accumulator:
            {
                inst.w = b & 1;

                if (!read_data(data_iter, data_end, inst))
                    return {};

                break;
            }

            case opcode::mov_memory_to_accumulator:
            case opcode::mov_accumulator_to_memory:
            {
                inst.w = b & 1;

                if (!read_and_advance(data_iter, data_end, b))
                    return {};

                inst.data_lo = b;
                if (inst.w)
                {
                    if (!read_and_advance(data_iter, data_end, b))
                        return {};

                    inst.data_hi = b;
                }
                break;
            }

            case opcode::mov_to_segment_register:
            case opcode::mov_from_segment_register:
            {
                if (!read_and_advance(data_iter, data_end, b)) 
                    return {};

                inst.rm = b & 0b0111;
                b >>= 3;
                inst.sr = b & 0b011;
                b >>= 3;
                inst.mod = b;

                if (!read_displacement(data_iter, data_end, inst))
                    return {};

                break;
            }

            case opcode::je:
            case opcode::jl:     
            case opcode::jle:
            case opcode::jb: 
            case opcode::jbe:
            case opcode::jp: 
            case opcode::jo: 
            case opcode::js: 
            case opcode::jne:
            case opcode::jnl:
            case opcode::jg: 
            case opcode::jnb:
            case opcode::ja: 
            case opcode::jnp:
            case opcode::jno:
            case opcode::jns:
            case opcode::loop:
            case opcode::loopz: 
            case opcode::loopnz:
            case opcode::jcxz:
            {
                if (!read_and_advance(data_iter, data_end, inst.data_lo))
                    return {};

                break;
            }

            case opcode::none:
                break;
        }

        return inst;
    }

    std::string get_instruction_data(const instruction& inst)
    {
        if (inst.w && !inst.s)
            return std::to_string(static_cast<int16_t>(inst.data_lo + (inst.data_hi << 8)));
        else
            return std::to_string(static_cast<int8_t>(inst.data_lo));
    }

    std::string get_instruction_address(const instruction& inst)
    {
        return "[" + get_instruction_data(inst) + "]";
    }

    int16_t get_instruction_displacement(const instruction& inst, int8_t bytes)
    {
        switch (bytes)
        {
            case 0:
            default:
                return 0;
            case 1:
                return static_cast<int8_t>(inst.disp_lo);
            case 2:
                return static_cast<int16_t>((inst.disp_hi << 8) + inst.disp_lo);
        }
    }

    std::string get_instruction_memory(const instruction& inst)
    {
        std::string address;

        int8_t displacement_bytes = 0;
        bool directAddress = false;
        switch (inst.mod)
        {
            case 0b00: // direct address, 16-bit displacement
                displacement_bytes = 2;
                directAddress = (inst.rm == 0b110);
                break;

            case 0b01: // memory mode, 8-bit displacement
                displacement_bytes = 1;
                break;

            case 0b10: // memory mode, 16-bit displacement
                displacement_bytes = 2;
                break;
        }

        const auto displacement = get_instruction_displacement(inst, displacement_bytes);

        if (directAddress)
        {
            if (displacement >= 0)
                address = std::to_string(displacement);
            else if (displacement < 0)
                address = std::to_string(-displacement);
        }
        else
        {
            address = effective_addresses[inst.rm];
            if (displacement > 0)
                address += " + " + std::to_string(displacement);
            else if (displacement < 0)
                address += " - " + std::to_string(-displacement);
        }

        return "[" + address + "]";
    }

    using instruction_parts = std::tuple<std::string, std::string, std::string>;

    instruction_parts decode_instruction(instruction& inst)
    {
        std::string destination;
        std::string source;

        switch (inst.opcode)
        {
            case opcode::mov_normal:
            case opcode::add_normal:
            case opcode::sub_normal:
            case opcode::cmp_normal:
            {
                if (inst.mod == 0b11) // register mode
                {
                    destination = registers[(inst.d ? inst.reg : inst.rm) + 8 * inst.w];
                    source = registers[(inst.d ? inst.rm : inst.reg) + 8 * inst.w];
                }
                else // memory mode
                {
                    std::string address = get_instruction_memory(inst);
                    destination = inst.d ? registers[inst.reg + 8 * inst.w] : address;
                    source = inst.d ? address : registers[inst.reg + 8 * inst.w];
                }
                break;
            }

            case opcode::mov_immediate_to_register_or_memory:
            case opcode::add_immediate_to_register_or_memory:
            case opcode::sub_immediate_from_register_or_memory:
            case opcode::cmp_immediate_with_register_or_memory:
            {
                if (inst.mod == 0b11) // register mode
                {
                    destination = registers[inst.rm + 8 * inst.w];
                    source = get_instruction_data(inst);
                }
                else // memory mode
                {
                    destination = get_instruction_memory(inst);
                    source = (inst.w ? "word " : "byte ") + get_instruction_data(inst);
                }
                break;
            }

            case opcode::mov_immediate_to_register:
            case opcode::add_immediate_to_accumulator:
            case opcode::sub_immediate_from_accumulator:
            case opcode::cmp_immediate_with_accumulator:
            {
                destination = registers[inst.reg + 8 * inst.w];
                source = get_instruction_data(inst);
                break;
            }

            case opcode::mov_memory_to_accumulator:
            {
                destination = registers[inst.w ? 8 : 0];
                source = get_instruction_address(inst);
                break;
            }

            case opcode::mov_accumulator_to_memory:
            {
                destination = get_instruction_address(inst);
                source = registers[inst.w ? 8 : 0];
                break;
            }

            case opcode::mov_to_segment_register:
            case opcode::mov_from_segment_register:
                // todo
                break;

            case opcode::je:
            case opcode::jl:
            case opcode::jle:
            case opcode::jb:
            case opcode::jbe:
            case opcode::jp:
            case opcode::jo:
            case opcode::js:
            case opcode::jne:
            case opcode::jnl:
            case opcode::jg:
            case opcode::jnb:
            case opcode::ja:
            case opcode::jnp:
            case opcode::jno:
            case opcode::jns:
            case opcode::loop:
            case opcode::loopz:
            case opcode::loopnz:
            case opcode::jcxz:
            {
                destination = std::to_string(static_cast<int8_t>(inst.data_lo));
                break;
            }

            case opcode::none:
                break;
        }

        return { opcodes[inst.opcode], destination, source };
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
        std::vector<instruction> instructions;
        auto data_iter = data.cbegin();
        const auto data_end = data.cend();

        while (data_iter != data_end)
        {
            std::optional<instruction> result = read_instruction(data_iter, data_end);

            if (result.has_value())
                instructions.push_back(result.value());
        }

        // decode instructions and print assembly
        for (instruction& inst : instructions)
        {
            auto [opcode, destination, source] = decode_instruction(inst);

            std::cout << opcode << ' ' << destination;
            if (source.length() > 0)
                std::cout << ", " << source;
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
