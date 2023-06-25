#include "simulator.hpp"

#include <cstdint>
#include <variant>

#include "control_flags.hpp"
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

    simulation_step step;

    if (const register_access* destination = std::get_if<register_access>(&inst.operands[0]))  // NOLINT(readability-container-data-pointer)
    {
        const uint16_t old_value = registers[destination->index];
        const uint16_t op_value = std::visit(matcher, inst.operands[1]);

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
                        new_value = (old_value & 0x00FF) + (op_value << 8);
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
                if (destination->count == 1 && destination->offset == 0)
                {
                    new_value = old_value + (op_value << 8);
                }
                else
                {
                    new_value = old_value + op_value;
                }
                break;
            }

            case operation_type::op_sub:
            case operation_type::op_cmp:
            {
                uint16_t result = 0;
                if (destination->count == 1 && destination->offset == 0)
                {
                    result = old_value - (op_value << 8);
                }
                else
                {
                    result = old_value - op_value;
                }

                if (result == 0)
                    new_flags |= control_flags::zero;
                else
                    new_flags &= ~control_flags::zero;

                if ((result & 0b1000'0000'0000'0000) != 0)
                    new_flags |= control_flags::sign;
                else
                    new_flags &= ~control_flags::sign;

                if (inst.op == operation_type::op_sub)
                    new_value = result;

                break;
            }
        }

        // write to registers
        registers[destination->index] = new_value;

        if (destination->index == code_segment_index && new_value != old_value)
        {
            // reset instruction pointer if the code segment changed
            registers[instruction_pointer_index] = 0;
        }
        else
        {
            ++registers[instruction_pointer_index];
        }

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

    return step;
}
