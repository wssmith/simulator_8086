#ifndef WS_CONTROLFLAGS_HPP
#define WS_CONTROLFLAGS_HPP

#include <cstdint>

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

constexpr control_flags operator|(control_flags f1, control_flags f2) noexcept
{
    return static_cast<control_flags>(static_cast<uint16_t>(f1) | static_cast<uint16_t>(f2));
}

constexpr control_flags operator&(control_flags f1, control_flags f2) noexcept
{
    return static_cast<control_flags>(static_cast<uint16_t>(f1) & static_cast<uint16_t>(f2));
}

constexpr control_flags operator^(control_flags f1, control_flags f2) noexcept
{
    return static_cast<control_flags>(static_cast<uint16_t>(f1) ^ static_cast<uint16_t>(f2));
}

constexpr control_flags operator~(control_flags f) noexcept
{
    return static_cast<control_flags>(~static_cast<uint16_t>(f));
}

constexpr control_flags& operator|=(control_flags& f1, control_flags f2) noexcept
{
    f1 = f1 | f2;
    return f1;
}

constexpr control_flags& operator&=(control_flags& f1, control_flags f2) noexcept
{
    f1 = f1 & f2;
    return f1;
}

constexpr control_flags& operator^=(control_flags& f1, control_flags f2) noexcept
{
    f1 = f1 ^ f2;
    return f1;
}

constexpr bool has_any_flag(control_flags this_flag, control_flags flags) noexcept
{
    return (this_flag & flags) != control_flags::none;
}

constexpr bool has_all_flags(control_flags this_flag, control_flags flags) noexcept
{
    return (this_flag & flags) == flags;
}

#endif
