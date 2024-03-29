﻿#include "simulator.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <variant>

#include "flag_utils.hpp"
#include "overloaded.hpp"
#include "instruction.hpp"
#include "register_access.hpp"

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
        { control_flags::overflow, 'O' }
    };

    struct simulator_numeric_limits
    {
        int32_t min_signed{};
        int32_t max_signed{};
        int32_t min_unsigned{};
        int32_t max_unsigned{};
    };

    enum class simulator_numeric_width : uint8_t
    {
        byte,
        word,
        nibble
    };

    constexpr simulator_numeric_limits get_numeric_limits(simulator_numeric_width width)
    {
        switch (width)
        {
            case simulator_numeric_width::byte:
                return
                {
                    .min_signed = std::numeric_limits<int8_t>::min(),
                    .max_signed = std::numeric_limits<int8_t>::max(),
                    .min_unsigned = std::numeric_limits<uint8_t>::min(),
                    .max_unsigned = std::numeric_limits<uint8_t>::max()
                };

            case simulator_numeric_width::word:
                return
                {
                    .min_signed = std::numeric_limits<int16_t>::min(),
                    .max_signed = std::numeric_limits<int16_t>::max(),
                    .min_unsigned = std::numeric_limits<uint16_t>::min(),
                    .max_unsigned = std::numeric_limits<uint16_t>::max()
                };

            case simulator_numeric_width::nibble:
                return
                {
                    .min_signed = -8,
                    .max_signed = 7,
                    .min_unsigned = 0,
                    .max_unsigned = 15
                };

            default:
                throw std::exception{ "Unexpected numeric width type." };
        }
    }

    control_flags compute_flags(int32_t existing, int32_t operand, int32_t result, bool wide_value, bool is_addition)
    {
        control_flags new_flags = control_flags::none;

        if ((std::popcount(static_cast<uint8_t>(result & 0xFF)) & 1) == 0)
            new_flags |= control_flags::parity;

        if (result == 0)
            new_flags |= control_flags::zero;

        if ((result & 0x8000) != 0)
            new_flags |= control_flags::sign;

        constexpr auto byte_limits = get_numeric_limits(simulator_numeric_width::byte);
        constexpr auto word_limits = get_numeric_limits(simulator_numeric_width::word);
        const auto limits = wide_value ? word_limits : byte_limits;

        if (result > limits.max_signed || result < limits.min_signed)
            new_flags |= control_flags::overflow;

        const auto existing_unsigned = static_cast<uint16_t>(existing);
        const auto operand_unsigned = static_cast<uint16_t>(operand);
        const int32_t result_unsigned = is_addition ? (existing_unsigned + operand_unsigned) : (existing_unsigned - operand_unsigned);

        if (result_unsigned > limits.max_unsigned || result_unsigned < limits.min_unsigned)
            new_flags |= control_flags::carry;

        constexpr auto aux_limits = get_numeric_limits(simulator_numeric_width::nibble);

        const int32_t existing_nibble = existing & 0xF;
        const int32_t operand_nibble = operand & 0xF;
        const int32_t result_nibble = is_addition ? (existing_nibble + operand_nibble) : (existing_nibble - operand_nibble);

        if (result_nibble > aux_limits.max_unsigned || result_nibble < aux_limits.min_unsigned)
            new_flags |= control_flags::aux_carry;

        return new_flags;
    }

    uint32_t get_address(const instruction_operand& destination_op, const register_array& registers)
    {
        uint32_t address{};

        if (const auto* da = std::get_if<direct_address>(&destination_op))
        {
            address = da->address;
        }
        else if (const auto* eae = std::get_if<effective_address_expression>(&destination_op))
        {
            const uint32_t term1_index = eae->term1.reg.index;
            address = registers[term1_index] + eae->displacement;

            if (eae->term2.has_value())
            {
                const uint32_t term2_index = eae->term2->reg.index;
                address += registers[term2_index];
            }
        }
        else
        {
            throw std::exception{ "Instruction operand type does not represent an address." };
        }

        return address;
    }

    void store_value(uint16_t value, uint32_t address, const instruction_flags& flags, memory_array& memory)
    {
        memory[address] = value & 0xFF;

        if (has_any_flag(flags, instruction_flags::wide))
            memory[address + 1] = (value >> 8) & 0xFF;
    }
}

std::string get_flag_string(control_flags flags)
{
    std::string flag_string;

    uint16_t flag_value = 1;
    while ((flag_value & 0xFFF) != 0)
    {
        if (auto flag = control_flags{ flag_value }; flag_names.contains(flag) && has_all_flags(flags, flag))
            flag_string += flag_names[flag];

        flag_value <<= 1;
    }

    return flag_string;
}

simulation_step simulate_instruction(const instruction& inst, register_array& registers, memory_array& memory)
{
    auto source_matcher = overloaded
    {
        [&registers, &memory](const effective_address_expression& eae) -> uint16_t
        {
            int32_t address = registers[eae.term1.reg.index] + eae.displacement;

            if (eae.term2.has_value())
                address += registers[eae.term2->reg.index];

            return memory[address];
        },
        [&memory](direct_address address) -> uint16_t { return memory[address.address]; },
        [&registers](register_access operand) -> uint16_t
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
        [](std::monostate) { return uint16_t{ 0 }; }
    };

    const instruction_operand destination_op = inst.operands[0];
    const uint16_t op_value = std::visit(source_matcher, inst.operands[1]);

    simulation_step step =
    {
        .old_flags = control_flags{ registers[flags_index] },
        .new_flags = control_flags{ registers[flags_index] },
        .old_ip = registers[instruction_pointer_index],
        .new_ip = static_cast<uint16_t>(registers[instruction_pointer_index] + inst.size)
    };

    if (const register_access* reg_destination = std::get_if<register_access>(&destination_op))
    {
        step.destination = *reg_destination;
        step.old_value = registers[reg_destination->index];
        step.new_value = registers[reg_destination->index];

        const auto old_value_signed = static_cast<int16_t>(step.old_value);
        const auto op_value_signed = static_cast<int16_t>(op_value);
        const bool wide_value = (reg_destination->count == 2);

        switch (inst.op)
        {
            case operation_type::mov:
            {
                if (reg_destination->count == 1)
                {
                    if (reg_destination->offset == 0)
                        step.new_value = (step.old_value & 0xFF) + (op_value << 8 & 0xFF00);
                    else
                        step.new_value = (step.old_value & 0xFF00) + op_value;
                }
                else
                {
                    step.new_value = op_value;
                }
                break;
            }

            case operation_type::add:
            case operation_type::sub:
            case operation_type::cmp:
            {
                const int32_t operand = (reg_destination->count == 1 && reg_destination->offset == 0) ? op_value_signed << 8 : op_value_signed;

                const bool is_addition = (inst.op == operation_type::add);
                const int32_t result = is_addition ? old_value_signed + operand : old_value_signed - operand;

                step.new_flags = compute_flags(old_value_signed, operand, result, wide_value, is_addition);

                if (inst.op != operation_type::cmp)
                    step.new_value = static_cast<uint16_t>(result);

                break;
            }

            default:
                throw std::exception{ "Opcode does not support a register as the first operand." };
        }

        // write to registers
        registers[reg_destination->index] = step.new_value;

        // update flags
        registers[flags_index] = static_cast<uint16_t>(step.new_flags);
    }
    else if (const immediate* displacement = std::get_if<immediate>(&destination_op))
    {
        step.destination = register_access
        {
            .index = instruction_pointer_index,
            .offset = 0,
            .count = 2
        };

        bool do_jump = false;
        switch (inst.op)
        {
            case operation_type::je:
                do_jump = has_any_flag(step.old_flags, control_flags::zero);
                break;
            case operation_type::jne:
                do_jump = !has_any_flag(step.old_flags, control_flags::zero);
                break;

            case operation_type::jl:
                do_jump = has_any_flag(step.old_flags, control_flags::sign) ^ has_any_flag(step.old_flags, control_flags::overflow);
                break;
            case operation_type::jnl:
                do_jump = !(has_any_flag(step.old_flags, control_flags::sign) ^ has_any_flag(step.old_flags, control_flags::overflow));
                break;

            case operation_type::jle:
                do_jump = (has_any_flag(step.old_flags, control_flags::sign) ^ has_any_flag(step.old_flags, control_flags::overflow)) || has_any_flag(step.old_flags, control_flags::zero);
                break;
            case operation_type::jg:
                do_jump = !(has_any_flag(step.old_flags, control_flags::sign) ^ has_any_flag(step.old_flags, control_flags::overflow)) || !has_any_flag(step.old_flags, control_flags::zero);
                break;

            case operation_type::jb:
                do_jump = has_any_flag(step.old_flags, control_flags::carry);
                break;
            case operation_type::jnb:
                do_jump = !has_any_flag(step.old_flags, control_flags::carry);
                break;

            case operation_type::jbe:
                do_jump = has_any_flag(step.old_flags, control_flags::zero | control_flags::carry);
                break;
            case operation_type::ja:
                do_jump = !has_any_flag(step.old_flags, control_flags::zero | control_flags::carry);
                break;

            case operation_type::jp:
                do_jump = has_any_flag(step.old_flags, control_flags::parity);
                break;
            case operation_type::jnp:
                do_jump = !has_any_flag(step.old_flags, control_flags::parity);
                break;

            case operation_type::jo:
                do_jump = has_any_flag(step.old_flags, control_flags::overflow);
                break;
            case operation_type::jno:
                do_jump = !has_any_flag(step.old_flags, control_flags::overflow);
                break;

            case operation_type::js:
                do_jump = has_any_flag(step.old_flags, control_flags::sign);
                break;
            case operation_type::jns:
                do_jump = !has_any_flag(step.old_flags, control_flags::sign);
                break;

            case operation_type::loopz:
            case operation_type::loopnz:
            case operation_type::loop:
            case operation_type::jcxz:
            {
                step.destination = register_access
                {
                    .index = counter_register_index,
                    .offset = 0,
                    .count = 2
                };

                step.old_value = registers[counter_register_index];

                if (inst.op == operation_type::jcxz)
                {
                    step.new_value = step.old_value;
                }
                else
                {
                    step.new_value = step.old_value - 1;
                    registers[counter_register_index] = step.new_value;
                }

                switch (inst.op)
                {
                    case operation_type::loopz:
                        do_jump = step.new_value == 0 && has_any_flag(step.old_flags, control_flags::zero);
                        break;
                    case operation_type::loopnz:
                        do_jump = step.new_value != 0 && !has_any_flag(step.old_flags, control_flags::zero);
                        break;
                    case operation_type::loop:
                        do_jump = step.new_value != 0;
                        break;
                    case operation_type::jcxz:
                        do_jump = registers[counter_register_index] == 0;
                        break;
                    default:
                        throw std::exception{ "Unexpected loop-related opcode." };
                }
                break;
            }

            case operation_type::jmp:
            {
                do_jump = true;
                break;
            }

            default:
                throw std::exception{ "Opcode does not support an immediate as the first operand." };
        }

        if (do_jump)
            step.new_ip += displacement->value;
    }
    else if (std::holds_alternative<direct_address>(destination_op) || std::holds_alternative<effective_address_expression>(destination_op))
    {
        const uint32_t address = get_address(destination_op, registers);

        switch (inst.op)
        {
            case operation_type::mov:
            {
                store_value(op_value, address, inst.flags, memory);
                break;
            }

            case operation_type::add:
            {
                uint16_t existing_value = memory[address];

                if (has_any_flag(inst.flags, instruction_flags::wide))
                    existing_value += (memory[address + 1] << 8) & 0xFF00;

                const uint16_t new_value = existing_value + op_value;
                
                store_value(new_value, address, inst.flags, memory);
                break;
            }

            case operation_type::jmp:
            {
                const uint16_t displacement_value = memory[address] + ((memory[address + 1] << 8) & 0xFF00);
                step.new_ip = displacement_value;
                break;
            }
            
            default:
                throw std::exception{ "Opcode does not support an address as the first operand." };
        }
    }
    else if (!std::holds_alternative<std::monostate>(destination_op))
    {
        throw std::exception{ "The instruction's first operand had an unexpected type." };
    }

    registers[instruction_pointer_index] = step.new_ip;

    return step;
}
