#ifndef WS_SIMULATOR_HPP
#define WS_SIMULATOR_HPP

#include <array>
#include <cstdint>

#include "decoder.hpp"

struct instruction;

struct simulation_step
{
    register_access destination{};
    uint16_t old_value{};
    uint16_t new_value{};
};

inline constexpr size_t register_count = 13;

simulation_step simulate_instruction(const instruction& inst, std::array<uint16_t, register_count>& registers);

#endif
