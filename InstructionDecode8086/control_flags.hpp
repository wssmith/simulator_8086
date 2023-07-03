#ifndef WS_CONTROLFLAGS_HPP
#define WS_CONTROLFLAGS_HPP

#include <cstdint>

#include "flag_utils.hpp"

enum class control_flags : uint16_t
{
    none = 0,
    carry = 1 << 0,
    parity = 1 << 2,
    aux_carry = 1 << 4,
    zero = 1 << 6,
    sign = 1 << 7,
    trap = 1 << 8,
    interrupt = 1 << 9,
    direction = 1 << 10,
    overflow = 1 << 11
};

FLAG_OPERATIONS(control_flags)

#endif
