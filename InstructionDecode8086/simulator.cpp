#include "simulator.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>

#include "control_flags.hpp"
#include "decoder.hpp"
#include "overloaded.hpp"

namespace
{
    std::unordered_map<control_flags, char> flag_names
    {
        { control_flags::carry, 'C' },
        { control_flags::parity, 'P' },
        { control_flags::aux_carry, 'A' },
        { control_flags::zero, 'Z' },
        { control_flags::sign, 'S' },
        { control_flags::trap, 'T' },
        { control_flags::interrupt, 'I' },
        { control_flags::direction, 'D' },
        { control_flags::overflow, 'O' },
    };

    control_flags compute_flags(int32_t value, bool wide_value)
    {
        control_flags new_flags = control_flags::none;

        if ((std::popcount(static_cast<uint8_t>(value & 0xFF)) & 1) == 0)
            new_flags |= control_flags::parity;

        if (value == 0)
            new_flags |= control_flags::zero;

        if ((value & 0x8000) != 0)
            new_flags |= control_flags::sign;

        const int min_value = wide_value ? std::numeric_limits<int16_t>::min() : std::numeric_limits<int8_t>::min();
        const int max_value = wide_value ? std::numeric_limits<int16_t>::max() : std::numeric_limits<int8_t>::max();

        if (value > max_value || value < min_value)
            new_flags |= control_flags::overflow;

        return new_flags;
    }
}

std::string get_flag_string(control_flags flags)
{
    std::string flag_string;

    uint16_t flag_value = 1;
    while ((flag_value & 0xFFF) != 0)
    {
        auto flag_to_test = static_cast<control_flags>(flag_value);

        if (flag_names.contains(flag_to_test) && has_any_flag(flag_to_test, flags))
            flag_string += flag_names[flag_to_test];

        flag_value <<= 1;
    }

    return flag_string;
}

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

    simulation_step step;

    if (const register_access* destination = std::get_if<register_access>(&inst.operands[0]))  // NOLINT(readability-container-data-pointer)
    {
        const uint16_t old_value = registers[destination->index];
        const uint16_t op_value = std::visit(matcher, inst.operands[1]);
        const auto signed_old_value = static_cast<int16_t>(old_value);
        const auto signed_op_value = static_cast<int16_t>(op_value);
        const bool wide_value = (destination->count == 2);

        const auto old_flags = static_cast<control_flags>(registers[flags_register_index]);

        uint16_t new_value = old_value;
        control_flags new_flags = old_flags;

        switch (inst.op)
        {
            case operation_type::op_mov:
            {
                if (destination->count == 1)
                {
                    if (destination->offset == 0)
                        new_value = (old_value & 0xFF) + (op_value << 8);
                    else
                        new_value = (old_value & 0xFF00) + op_value;
                }
                else
                {
                    new_value = op_value;
                }
                break;
            }

            case operation_type::op_add:
            {
                int32_t result = 0;
                if (destination->count == 1 && destination->offset == 0)
                    result = signed_old_value + (signed_op_value << 8);
                else
                    result = signed_old_value + signed_op_value;

                new_flags = compute_flags(result, wide_value);

                new_value = static_cast<uint16_t>(result);
                break;
            }

            case operation_type::op_sub:
            case operation_type::op_cmp:
            {
                int32_t result = 0;
                if (destination->count == 1 && destination->offset == 0)
                    result = signed_old_value - (signed_op_value << 8);
                else
                    result = signed_old_value - signed_op_value;

                new_flags = compute_flags(result, wide_value);

                if (inst.op == operation_type::op_sub)
                    new_value = static_cast<uint16_t>(result);

                break;
            }
        }

        // write to registers
        registers[destination->index] = new_value;

        // update flags
        registers[flags_register_index] = static_cast<uint16_t>(new_flags);

        step = simulation_step
        {
            .destination = *destination,
            .old_value = old_value,
            .new_value = new_value,
            .old_flags = old_flags,
            .new_flags = new_flags
        };
    }

    registers[instruction_pointer_index] += inst.size;

    return step;
}
