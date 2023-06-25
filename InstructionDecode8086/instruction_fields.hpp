#ifndef WS_INSTRUCTIONFIELDS_HPP
#define WS_INSTRUCTIONFIELDS_HPP

#include <cstdint>

enum class opcode : uint16_t;

struct instruction_fields
{
    uint16_t size{};
    opcode opcode{};
    uint8_t mod{};
    uint8_t reg{};
    uint8_t rm{};
    uint8_t sr{};
    uint8_t disp_lo{};
    uint8_t disp_hi{};
    uint8_t data_lo{};
    uint8_t data_hi{};
    bool d{};
    bool w{};
    bool s{};
};

#endif
