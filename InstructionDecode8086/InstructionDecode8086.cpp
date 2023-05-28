// InstructionDecode8086.cpp

#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>

struct instruction
{
    uint8_t opcode = 0;
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

constexpr uint8_t mov_op = 0b10'0010;

std::unordered_map<uint8_t, const char*> opcodes
{
    { mov_op, "mov" }
};

int main()
{
    // open binary file
    std::ifstream input_file{ R"(C:\Users\Wesley\Desktop\listing_0038_many_register_mov)" };

    // read instructions from file
    std::vector<instruction> instructions;

    uint8_t b;
    while (input_file >> b)
    {
        instruction inst;

        inst.w = b & 1;
        b >>= 1;
        inst.d = b & 1;
        b >>= 1;
        inst.opcode = b;

        if (input_file >> b)
        {
            if (inst.opcode != mov_op)
                continue; // only supporting mov for now 

            inst.rm = b & 0b0111;
            b >>= 3;
            inst.reg = b & 0b0111;
            b >>= 3;
            inst.mod = b;

            instructions.push_back(inst);
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
