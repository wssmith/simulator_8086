// InstructionDecode8086.cpp

#include <array>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
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
        uint8_t disp_lo = 0;
        uint8_t disp_hi = 0;
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

    opcode determine_opcode(uint8_t b)
    {
        switch (b >> 4)
        {
        case 0b1000:
        {
            const uint8_t second_half = b & 0b1111;
            switch (second_half)
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
}

int main()
{
    // open binary file
    std::ifstream input_file{ R"(C:\Users\Wesley\Desktop\listing_0038_many_register_mov)" };

    // read instructions from file
    std::vector<instruction> instructions;

    uint8_t b;
    while (input_file >> b)
    {
        instruction inst{};

        const opcode opcode = determine_opcode(b);

        switch (opcode)
        {
        case opcode::mov_normal:
        {
            inst.w = b & 1;
            b >>= 1;
            inst.d = b & 1;
            b >>= 1;
            inst.opcode = opcode;

            input_file >> b;

            inst.rm = b & 0b0111;
            b >>= 3;
            inst.reg = b & 0b0111;
            b >>= 3;
            inst.mod = b;

            int displacement_bytes = 0;
            switch (inst.mod)
            {
            case 0b00: // memory mode, no displacement unless direct address
                displacement_bytes = (inst.rm == 0b110) * 2;
                break;
            case 0b01: // memory mode, 8-bit displacement
                displacement_bytes = 1;
                break;
            case 0b10: // memory mode, 16-bit displacement
                displacement_bytes = 2;
                break;
            case 0b11: // register mode, no displacement
                break;
            }

            // read displacement
            if (displacement_bytes > 0)
            {
                input_file >> inst.disp_lo;

                if (displacement_bytes > 1)
                    input_file >> inst.disp_hi;
            }

            break;
        }

        case opcode::mov_immediate_to_register_or_memory:
            break;

        case opcode::mov_immediate_to_register:
            break;

        case opcode::mov_memory_to_accumulator:
        case opcode::mov_accumulator_to_memory:
            break;

        case opcode::mov_to_segment_register:
        case opcode::mov_from_segment_register:
            break;

        case opcode::none:
            break;
        }

        instructions.push_back(inst);
    }

    // decode instruction and print asm
    for (auto& inst : instructions)
    {
        const char* opcode = opcodes[inst.opcode];

        std::string source_name;
        std::string destination_name;

        switch (inst.opcode)
        {
        case opcode::mov_normal:
            {
                if (inst.mod == 0b11)
                {
                    // register mode
                    if (inst.d)
                    {
                        source_name = registers[inst.rm + 8 * inst.w];
                        destination_name = registers[inst.reg + 8 * inst.w];
                    }
                    else
                    {
                        source_name = registers[inst.reg + 8 * inst.w];
                        destination_name = registers[inst.rm + 8 * inst.w];
                    }
                }
                else
                {
                    std::string address = effective_addresses[inst.rm];

                    // memory mode, 8-bit displacement
                    if (inst.mod == 0b01)
                        address += " + " + std::to_string(inst.disp_lo);

                    // memory mode, 16-bit displacement
                    if (inst.mod == 0b10)
                        address += " + " + std::to_string((inst.disp_hi << 8) + inst.disp_lo);

                    if (inst.mod == 0b00 && inst.rm == 0b110)
                    {
                        // direct address
                        // todo
                    }

                    if (inst.d)
                    {
                        source_name = "[" + address + "]";
                        destination_name = registers[inst.reg + 8 * inst.w];
                    }
                    else
                    {
                        source_name = registers[inst.reg + 8 * inst.w];
                        destination_name = "[" + address + "]";
                    }
                }
            }
        case opcode::mov_immediate_to_register_or_memory: break;
        case opcode::mov_immediate_to_register: break;
        case opcode::mov_memory_to_accumulator: break;
        case opcode::mov_accumulator_to_memory: break;
        case opcode::mov_to_segment_register: break;
        case opcode::mov_from_segment_register: break;
        case opcode::none: break;
        }

        std::cout << opcode << ' ' << destination_name << ", " << source_name << '\n';
    }
}
