#include "cycle_estimator.hpp"

#include <map>
#include <tuple>
#include <variant>

#include "instruction.hpp"
#include "overloaded.hpp"

namespace
{
    enum class operand_type : int8_t
    {
        none,
        accumulator,
        register_access,
        memory,
        immediate
    };

    struct cycle_info
    {
        int32_t base_count{};
        bool use_ea{};
        int8_t ea_index{};
    };

    using cycle_map = std::map<std::tuple<operation_type, operand_type, operand_type>, cycle_info>;

    cycle_map cycle_table
    {
        { { operation_type::mov, operand_type::memory, operand_type::accumulator }, { .base_count = 10 } },
        { { operation_type::mov, operand_type::accumulator, operand_type::memory }, { .base_count = 10 } },

        { { operation_type::mov, operand_type::register_access, operand_type::register_access }, { .base_count = 2 } },
        { { operation_type::mov, operand_type::accumulator, operand_type::accumulator }, { .base_count = 2 } },
        { { operation_type::mov, operand_type::accumulator, operand_type::register_access }, { .base_count = 2 } },
        { { operation_type::mov, operand_type::register_access, operand_type::accumulator }, { .base_count = 2 } },

        { { operation_type::mov, operand_type::register_access, operand_type::memory }, { .base_count = 8, .use_ea = true, .ea_index = 1 } },
        { { operation_type::mov, operand_type::memory, operand_type::register_access }, { .base_count = 9, .use_ea = true, .ea_index = 0 } },

        { { operation_type::mov, operand_type::register_access, operand_type::immediate }, { .base_count = 4 } },
        { { operation_type::mov, operand_type::accumulator, operand_type::immediate }, { .base_count = 4 } },

        { { operation_type::mov, operand_type::memory, operand_type::immediate }, { .base_count = 10, .use_ea = true, .ea_index = 0 } },

        { { operation_type::add, operand_type::register_access, operand_type::register_access }, { .base_count = 3 } },
        { { operation_type::add, operand_type::accumulator, operand_type::accumulator }, { .base_count = 3 } },
        { { operation_type::add, operand_type::accumulator, operand_type::register_access }, { .base_count = 3 } },
        { { operation_type::add, operand_type::register_access, operand_type::accumulator }, { .base_count = 3 } },

        { { operation_type::add, operand_type::register_access, operand_type::memory }, { .base_count = 9, .use_ea = true, .ea_index = 1 } },
        { { operation_type::add, operand_type::accumulator, operand_type::memory }, { .base_count = 9, .use_ea = true, .ea_index = 1 } },

        { { operation_type::add, operand_type::memory, operand_type::register_access }, { .base_count = 16, .use_ea = true, .ea_index = 0 } },
        { { operation_type::add, operand_type::memory, operand_type::accumulator }, { .base_count = 16, .use_ea = true, .ea_index = 0 } },

        { { operation_type::add, operand_type::register_access, operand_type::immediate }, { .base_count = 4 } },
        { { operation_type::add, operand_type::accumulator, operand_type::immediate }, { .base_count = 4 } },

        { { operation_type::add, operand_type::memory, operand_type::immediate }, { .base_count = 17, .use_ea = true, .ea_index = 0 } }
    };

    // bx, bp, si, di, disp
    using ea_map = std::map<std::tuple<bool, bool, bool, bool, bool>, int8_t>;

    ea_map ea_table
    {
        // displacement only
        { { false, false, false, false, true }, 6 }, // disp

        // base or index only
        { { true, false, false, false, false }, 5 }, // bx
        { { false, true, false, false, false }, 5 }, // bp
        { { false, false, true, false, false }, 5 }, // si
        { { false, false, false, true, false }, 5 }, // di

        // displacement + base or index
        { { true, false, false, false, true }, 9 }, // bx + disp
        { { false, true, false, false, true }, 9 }, // bp + disp
        { { false, false, true, false, true }, 9 }, // si + disp
        { { false, false, false, true, true }, 9 }, // di + disp

        // base + index
        { { true, false, true, false, false }, 7 }, // bx + si
        { { true, false, false, true, false }, 8 }, // bx + di
        { { false, true, true, false, false }, 8 }, // bp + si
        { { false, true, false, true, false }, 7 }, // bp + di

        // displacement + base + index
        { { true, false, true, false, true }, 11 }, // bx + si + disp
        { { true, false, false, true, true }, 12 }, // bx + di + disp
        { { false, true, true, false, true }, 12 }, // bp + si + disp
        { { false, true, false, true, true }, 11 }, // bp + di + disp
    };

    operand_type get_operand_type(const instruction_operand& operand)
    {
        auto matcher = overloaded
        {
            [](const effective_address_expression&) { return operand_type::memory; },
            [](direct_address) { return operand_type::memory; },
            [](register_access reg_access)
            {
                const register_types reg_type = get_register_types(reg_access);

                if (has_any_flag(reg_type, register_types::ax | register_types::ah | register_types::al))
                    return operand_type::accumulator;

                return operand_type::register_access;
            },
            [](immediate) { return operand_type::immediate; },
            [](std::monostate) { return operand_type::none; }
        };

        return std::visit(matcher, operand);
    }
}

cycle_estimate estimate_cycles(const instruction& inst)
{
    operation_type opcode = inst.op;
    operand_type first_operand_type = get_operand_type(inst.operands[0]);
    operand_type second_operand_type = get_operand_type(inst.operands[1]);

    const std::tuple cycle_key = { opcode, first_operand_type, second_operand_type };

    if (!cycle_table.contains(cycle_key))
        throw std::exception{ "Unexpected instruction for cycle estimation." };

    const auto [base_cycles, use_ea, ea_index] = cycle_table[cycle_key];

    int8_t ea_cycles = 0;
    if (use_ea)
    {
        instruction_operand address_operand = inst.operands[ea_index];

        auto matcher = overloaded
        {
            [](const effective_address_expression& eae)
            {
                const register_types term1_reg_type = get_register_types(eae.term1.reg);
                const register_types term2_reg_type = eae.term2.has_value() ? get_register_types(eae.term2->reg) : register_types::none;
                const register_types eae_reg_types = term1_reg_type | term2_reg_type;

                bool bx = has_any_flag(eae_reg_types, register_types::bx);
                bool bp = has_any_flag(eae_reg_types, register_types::bp);
                bool si = has_any_flag(eae_reg_types, register_types::si);
                bool di = has_any_flag(eae_reg_types, register_types::di);
                const bool disp = (eae.displacement != 0);

                const std::tuple ea_key = { bx, bp, si, di, disp };

                if (!ea_table.contains(ea_key))
                    throw std::exception{ "Unexpected effective address expression for cycle estimation." };

                return ea_table[ea_key];
            },
            [](direct_address)
            {
                return ea_table[{ false, false, false, false, true }];
            },
            [](register_access) { return int8_t{ 0 }; },
            [](immediate) { return int8_t{ 0 }; },
            [](std::monostate) { return int8_t{ 0 }; }
        };

        ea_cycles = std::visit(matcher, address_operand);
    }

    return cycle_estimate
    {
        .base = base_cycles,
        .ea = ea_cycles
    };
}
