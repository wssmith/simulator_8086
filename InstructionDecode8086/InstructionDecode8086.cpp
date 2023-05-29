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

namespace
{
    enum class opcode : uint16_t
    {
        none,
        mov_normal,
        mov_immediate_to_register_or_memory,
        mov_immediate_to_register,
        mov_memory_to_accumulator,
        mov_accumulator_to_memory,
        mov_to_segment_register,
        mov_from_segment_register
    };

    struct instruction
    {
        opcode opcode = opcode::none;
        uint8_t mod = 0;
        uint8_t reg = 0;
        uint8_t rm = 0;
        uint8_t sr = 0;
        uint8_t disp_lo = 0;
        uint8_t disp_hi = 0;
        uint8_t data_lo = 0;
        uint8_t data_hi = 0;
        uint8_t addr_lo = 0;
        uint8_t addr_hi = 0;
        bool d = false;
        bool w = false;
    };

    constexpr std::array<const char*, 16> registers =
    {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
    };

    constexpr std::array<const char*, 8> effective_addresses =
    {
        "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"
    };

    constexpr const char* mov_name = "mov";

    std::unordered_map<opcode, const char*> opcodes
    {
        { opcode::mov_normal, mov_name },
        { opcode::mov_immediate_to_register_or_memory, mov_name },
        { opcode::mov_immediate_to_register, mov_name },
        { opcode::mov_memory_to_accumulator, mov_name },
        { opcode::mov_accumulator_to_memory, mov_name },
        { opcode::mov_to_segment_register, mov_name },
        { opcode::mov_from_segment_register, mov_name },
    };

    std::vector<uint8_t> read_binary_file(const std::string& path)
    {
        std::ifstream input_file{ path, std::ios::in | std::ios::binary };

        std::vector<uint8_t> data;
        std::for_each(std::istreambuf_iterator<char>(input_file),
                      std::istreambuf_iterator<char>(),
                      [&data](char c)
                      {
                          data.push_back(c);
                      });

        return data;
    }

    opcode read_opcode(uint8_t b)
    {
        switch (b >> 4)
        {
            case 0b1000:
            {
                switch (const uint8_t second_half = b & 0b1111)
                {
                    case 0b1110:
                        return opcode::mov_to_segment_register;
                    case 0b1100:
                        return opcode::mov_from_segment_register;
                    default:
                        if ((second_half & 0b1100) == 0b1000)
                            return opcode::mov_normal;
                        break;
                }
                break;
            }
            case 0b1100:
                return opcode::mov_immediate_to_register_or_memory;
            case 0b1011:
                return opcode::mov_immediate_to_register;
            case 0b1010:
                switch (b & 0b1110)
                {
                    case 0b0000:
                        return opcode::mov_memory_to_accumulator;
                    case 0b0010:
                        return opcode::mov_accumulator_to_memory;
                }
                break;
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

    using data_iterator = std::vector<uint8_t>::const_iterator;

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

        if (inst.w)
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

            case opcode::mov_immediate_to_register_or_memory:
            {
                inst.w = b & 1;

                if (!read_and_advance(data_iter, data_end, b))
                    return {};

                inst.rm = b & 0b0111;
                b >>= 6;
                inst.mod = b;

                if (!read_displacement(data_iter, data_end, inst))
                    return {};
                if (!read_data(data_iter, data_end, inst))
                    return {};

                break;
            }

            case opcode::mov_immediate_to_register:
            {
                inst.reg = b & 0b111;
                b >>= 3;
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

                inst.addr_lo = b;
                if (inst.w)
                {
                    if (!read_and_advance(data_iter, data_end, b))
                        return {};

                    inst.addr_hi = b;
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

            case opcode::none:
                break;
        }

        return inst;
    }

    std::string get_instruction_data(const instruction& inst)
    {
        if (inst.w)
            return std::to_string(static_cast<int16_t>(inst.data_lo + (inst.data_hi << 8)));
        else
            return std::to_string(static_cast<int8_t>(inst.data_lo));
    }

    std::string get_instruction_address(const instruction& inst)
    {
        if (inst.w)
            return "[" + std::to_string(static_cast<int16_t>(inst.addr_lo + (inst.addr_hi << 8))) + "]";
        else
            return "[" + std::to_string(static_cast<int8_t>(inst.addr_lo)) + "]";
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
        std::string source;
        std::string destination;

        switch (inst.opcode)
        {
            case opcode::mov_normal:
            {
                if (inst.mod == 0b11) // register mode
                {
                    source = registers[(inst.d ? inst.rm : inst.reg) + 8 * inst.w];
                    destination = registers[(inst.d ? inst.reg : inst.rm) + 8 * inst.w];
                }
                else // memory mode
                {
                    std::string address = get_instruction_memory(inst);
                    source = inst.d ? address : registers[inst.reg + 8 * inst.w];
                    destination = inst.d ? registers[inst.reg + 8 * inst.w] : address;
                }
                break;
            }

            case opcode::mov_immediate_to_register_or_memory:
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

            case opcode::none:
                break;
        }

        return { opcodes[inst.opcode], source, destination };
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
            auto [opcode, source, destination] = decode_instruction(inst);

            std::cout << opcode << ' ' << destination << ", " << source << '\n';
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
