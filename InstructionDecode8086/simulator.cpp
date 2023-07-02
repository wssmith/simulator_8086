#include "simulator.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>

#include "control_flags.hpp"
#include "flag_utils.hpp"
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


    struct simulator_numeric_limits
    {
        int32_t min_signed{};
        int32_t max_signed{};
        int32_t min_unsigned{};
        int32_t max_unsigned{};
    };

    enum class simulator_numeric_width : uint8_t { byte, word, nibble };

    simulator_numeric_limits get_numeric_limits(simulator_numeric_width width)
    {
        switch (width)
        {
            case simulator_numeric_width::byte:
            default:
                return simulator_numeric_limits
                {
                    .min_signed = std::numeric_limits<int8_t>::min(),
                    .max_signed = std::numeric_limits<int8_t>::max(),
                    .min_unsigned = std::numeric_limits<uint8_t>::min(),
                    .max_unsigned = std::numeric_limits<uint8_t>::max()
                };

            case simulator_numeric_width::word:
                return simulator_numeric_limits
                {
                    .min_signed = std::numeric_limits<int16_t>::min(),
                    .max_signed = std::numeric_limits<int16_t>::max(),
                    .min_unsigned = std::numeric_limits<uint16_t>::min(),
                    .max_unsigned = std::numeric_limits<uint16_t>::max()
                };

            case simulator_numeric_width::nibble:
                return simulator_numeric_limits
                {
                    .min_signed = -128,
                    .max_signed = 127,
                    .min_unsigned = 0,
                    .max_unsigned = 255
                };
        }
    }

    control_flags compute_flags(int32_t result, bool wide_value)
    {
        control_flags new_flags = control_flags::none;

        if ((std::popcount(static_cast<uint8_t>(result & 0xFF)) & 1) == 0)
            new_flags |= control_flags::parity;

        if (result == 0)
            new_flags |= control_flags::zero;

        if ((result & 0x8000) != 0)
            new_flags |= control_flags::sign;

        const auto width = wide_value ? simulator_numeric_width::word : simulator_numeric_width::byte;
        const auto limits = get_numeric_limits(width);
        const auto aux_limits = get_numeric_limits(simulator_numeric_width::nibble);

        if (result > limits.max_signed || result < limits.min_signed)
            new_flags |= control_flags::overflow;

        if (result > limits.max_unsigned || result < limits.min_unsigned)
            new_flags |= control_flags::carry;

        if (result > aux_limits.max_unsigned || result < aux_limits.min_unsigned)
            new_flags |= control_flags::aux_carry;

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
    // update instruction pointer
    const uint16_t old_ip = registers[instruction_pointer_index];
    uint16_t new_ip = old_ip + static_cast<uint16_t>(inst.size);

    const auto old_flags = static_cast<control_flags>(registers[flags_register_index]);
    control_flags new_flags = old_flags;

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
        uint16_t new_value = old_value;

        const uint16_t op_value = std::visit(matcher, inst.operands[1]);
        const auto signed_old_value = static_cast<int16_t>(old_value);
        const auto signed_op_value = static_cast<int16_t>(op_value);
        const bool wide_value = (destination->count == 2);

        switch (inst.op)
        {
            case operation_type::mov:
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

            case operation_type::add:
            {
                const int32_t operand = (destination->count == 1 && destination->offset == 0) ? signed_op_value << 8 : signed_op_value;
                const int32_t result = signed_old_value + operand;

                new_flags = compute_flags(result, wide_value);

                new_value = static_cast<uint16_t>(result);
                break;
            }

            case operation_type::sub:
            case operation_type::cmp:
            {
                const int32_t operand = (destination->count == 1 && destination->offset == 0) ? signed_op_value << 8 : signed_op_value;
                const int32_t result = signed_old_value - operand;

                new_flags = compute_flags(result, wide_value);

                if (inst.op == operation_type::sub)
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
            .new_flags = new_flags,
            .old_ip = old_ip,
            .new_ip = new_ip
        };
    }
    else if (const immediate* displacement = std::get_if<immediate>(&inst.operands[0]))  // NOLINT(readability-container-data-pointer)
    {
        auto destination = register_access
        {
            .index = instruction_pointer_index,
            .offset = 0,
            .count = 2
        };

        uint16_t old_value = 0;
        uint16_t new_value = 0;
        bool do_jump = false;

        switch (inst.op)
        {
            case operation_type::je:
                do_jump = has_any_flag(old_flags, control_flags::zero);
                break;
            case operation_type::jne:
                do_jump = !has_any_flag(old_flags, control_flags::zero);
                break;

            case operation_type::jl:
                do_jump = has_any_flag(old_flags, control_flags::sign) ^ has_any_flag(old_flags, control_flags::overflow);
                break;
            case operation_type::jnl:
                do_jump = !(has_any_flag(old_flags, control_flags::sign) ^ has_any_flag(old_flags, control_flags::overflow));
                break;

            case operation_type::jle:
                do_jump = (has_any_flag(old_flags, control_flags::sign) ^ has_any_flag(old_flags, control_flags::overflow)) || has_any_flag(old_flags, control_flags::zero);
                break;
            case operation_type::jg:
                do_jump = !(has_any_flag(old_flags, control_flags::sign) ^ has_any_flag(old_flags, control_flags::overflow)) || !has_any_flag(old_flags, control_flags::zero);
                break;

            case operation_type::jb:
                do_jump = has_any_flag(old_flags, control_flags::carry);
                break;
            case operation_type::jnb:
                do_jump = !has_any_flag(old_flags, control_flags::carry);
                break;

            case operation_type::jbe:
                do_jump = has_any_flag(old_flags, control_flags::zero | control_flags::carry);
                break;
            case operation_type::ja:
                do_jump = !has_any_flag(old_flags, control_flags::zero | control_flags::carry);
                break;

            case operation_type::jp:
                do_jump = has_any_flag(old_flags, control_flags::parity);
                break;
            case operation_type::jnp:
                do_jump = !has_any_flag(old_flags, control_flags::parity);
                break;

            case operation_type::jo:
                do_jump = has_any_flag(old_flags, control_flags::overflow);
                break;
            case operation_type::jno:
                do_jump = !has_any_flag(old_flags, control_flags::overflow);
                break;

            case operation_type::js:
                do_jump = has_any_flag(old_flags, control_flags::sign);
                break;
            case operation_type::jns:
                do_jump = !has_any_flag(old_flags, control_flags::sign);
                break;

            case operation_type::loopz:
            case operation_type::loopnz:
            case operation_type::loop:
            case operation_type::jcxz:
            {
                destination = register_access
                {
                    .index = counter_register_index,
                    .offset = 0,
                    .count = 2
                };

                old_value = registers[counter_register_index];

                if (inst.op == operation_type::jcxz)
                {
                    new_value = old_value;
                }
                else
                {
                    new_value = old_value - 1;
                    registers[counter_register_index] = new_value;
                }

                do_jump = false;
                switch (inst.op)
                {
                    case operation_type::loopz:
                        do_jump = new_value == 0 && has_any_flag(old_flags, control_flags::zero);
                        break;
                    case operation_type::loopnz:
                        do_jump = new_value != 0 && !has_any_flag(old_flags, control_flags::zero);
                        break;
                    case operation_type::loop:
                        do_jump = new_value == 0;
                        break;
                    case operation_type::jcxz:
                        do_jump = registers[counter_register_index] == 0;
                        break;
                }
                break;
            }
        }

        if (do_jump)
            new_ip += displacement->value;

        step = simulation_step
        {
            .destination = destination,
            .old_value = old_value,
            .new_value = new_value,
            .old_flags = old_flags,
            .new_flags = new_flags,
            .old_ip = old_ip,
            .new_ip = new_ip
        };
    }

    // update instruction pointer
    registers[instruction_pointer_index] = new_ip;

    return step;
}
