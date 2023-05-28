// InstructionDecode8086.cpp

#include <cstdint>
#include <iostream>
#include <fstream>
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
        bool d = false;
        bool w = false;
    };

    constexpr uint8_t mod_regreg = 0b11;

    constexpr char registers[16][3] =
    {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
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
            inst.w = b & 1;
            b >>= 1;
            inst.d = b & 1;
            b >>= 1;
            inst.opcode = opcode;

            if (input_file >> b)
            {
                inst.rm = b & 0b0111;
                b >>= 3;
                inst.reg = b & 0b0111;
                b >>= 3;
                inst.mod = b;

                instructions.push_back(inst);
            }
            break;

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
    }

    // decode instruction and print asm
    for (auto& inst : instructions)
    {
        if (inst.mod != mod_regreg)
            continue; // only supporting reg-to-reg

        const uint8_t source = inst.d ? inst.rm : inst.reg;
        const uint8_t destination = inst.d ? inst.reg : inst.rm;

        const char* source_name = registers[source + 8 * inst.w];
        const char* destination_name = registers[destination + 8 * inst.w];

        const char* opcode = opcodes[inst.opcode];

        std::cout << opcode << ' ' << destination_name << ", " << source_name << '\n';
    }
}
