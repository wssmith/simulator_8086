#include "simulator.hpp"

#include <cstdint>
#include <variant>

#include "decoder.hpp"
#include "overloaded.hpp"

simulation_step simulate_instruction(const instruction& inst, std::array<uint16_t, register_count>& registers)
{
    auto matcher = overloaded
    {
        [](const effective_address_expression&) -> uint16_t { return 0; },
        [](direct_address) -> uint16_t { return 0; },
        [&registers](const register_access& operand) -> uint16_t
        {
            if (operand.count == 1)
            {
                if (operand.offset == 0)
                    return (registers[operand.index] & 0xFF00) >> 8;
                else
                    return (registers[operand.index] & 0x00FF);
            }

            return registers[operand.index];
        },
        [](immediate operand) { return static_cast<uint16_t>(operand.value); },
        [](std::monostate) -> uint16_t { return 0; }
    };

    constexpr size_t ip_index = 12;
    ++registers[ip_index];

    simulation_step step;

    if (const register_access* destination = std::get_if<register_access>(&inst.operands[0]))  // NOLINT(readability-container-data-pointer)
    {
        const uint16_t old_value = registers[destination->index];
        const uint16_t op_value = std::visit(matcher, inst.operands[1]);

        uint16_t new_value = op_value;
        if (destination->count == 1)
        {
            if (destination->offset == 0)
                new_value = (old_value & 0x00FF) + (op_value << 8);
            else
                new_value = (old_value & 0xFF00) + op_value;
        }

        registers[destination->index] = new_value;

        step = simulation_step
        {
            .destination = *destination,
            .old_value = old_value,
            .new_value = new_value
        };
    }

    return step;
}
