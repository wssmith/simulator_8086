#ifndef WS_INSTRUCTION_HPP
#define WS_INSTRUCTION_HPP

#include <cstdint>

enum class opcode : uint16_t;

struct instruction
{
    opcode opcode{};
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
    bool s = false;
};

#endif