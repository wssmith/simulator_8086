#ifndef WS_SIMULATOR_HPP
#define WS_SIMULATOR_HPP

#include <array>
#include <cstdint>
#include <string>

#include "decoder.hpp"

enum class control_flags : uint16_t;

struct instruction;

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

inline constexpr int code_segment_index = 8;
inline constexpr int instruction_pointer_index = 12;
inline constexpr int flags_register_index = 13;
inline constexpr int register_count = 14;

std::string get_flag_string(control_flags flags);

simulation_step simulate_instruction(const instruction& inst, std::array<uint16_t, register_count>& registers);

#endif
