#ifndef WS_SIMULATOR_HPP
#define WS_SIMULATOR_HPP

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>

#include "flag_utils.hpp"
#include "register_access.hpp"

struct instruction;

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

FLAG_OPERATIONS(control_flags);

struct simulation_step
{
    register_access destination{};
    uint16_t old_value{};
    uint16_t new_value{};
    control_flags old_flags{};
    control_flags new_flags{};
    uint16_t old_ip{};
    uint16_t new_ip{};
};

inline constexpr int counter_register_index = 2;
inline constexpr int code_segment_index = 8;
inline constexpr int instruction_pointer_index = 12;
inline constexpr int flags_index = 13;
inline constexpr int register_count = 14;

using register_array = std::array<uint16_t, register_count>;
inline register_array registers = {};

inline constexpr int memory_size = 1024 * 1024;
inline std::array<uint8_t, memory_size> memory = {};

std::string get_flag_string(control_flags flags);

simulation_step simulate_instruction(const instruction& inst);

#endif
