#include "decoder.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>

#include "instruction_fields.hpp"
#include "opcode.hpp"

namespace
{
    constexpr std::array registers =
    {
        "al", "cl", "dl", "bl",
        "ah", "ch", "dh", "bh",
        "ax", "cx", "dx", "bx",
        "sp", "bp", "si", "di",
        "es", "cs", "ss", "ds",
        "ip"
    };

    std::unordered_map<operation_type, const char*> mnemonics
    {
        { operation_type::op_mov, "mov" },
        { operation_type::op_add, "add" },
        { operation_type::op_sub, "sub" },
        { operation_type::op_cmp, "cmp" },
        { operation_type::op_je, "je" },
        { operation_type::op_jl, "jl" },
        { operation_type::op_jle, "jle" },
        { operation_type::op_jb, "jb" },
        { operation_type::op_jbe, "jbe" },
        { operation_type::op_jp, "jp" },
        { operation_type::op_jo, "jo" },
        { operation_type::op_js, "js" },
        { operation_type::op_jne, "jne" },
        { operation_type::op_jnl, "jnl" },
        { operation_type::op_jg, "jg" },
        { operation_type::op_jnb, "jnb" },
        { operation_type::op_ja, "ja" },
        { operation_type::op_jnp, "jnp" },
        { operation_type::op_jno, "jno" },
        { operation_type::op_jns, "jns" },
        { operation_type::op_loop, "loop" },
        { operation_type::op_loopz, "loopz" },
        { operation_type::op_loopnz, "loopnz" },
        { operation_type::op_jcxz, "jcxz" }
    };

    constexpr std::array<std::pair<register_access, std::optional<register_access>>, 8> effective_addresses =
    {
        std::pair{ register_access{11, 0, 1}, register_access{14, 0, 1} }, // bx + si
        std::pair{ register_access{11, 0, 1}, register_access{15, 0, 1} }, // bx + di
        std::pair{ register_access{13, 0, 1}, register_access{14, 0, 1} }, // bp + si
        std::pair{ register_access{13, 0, 1}, register_access{15, 0, 1} }, // bp + di
        std::pair{ register_access{14, 0, 1}, std::optional<register_access>{} }, // si
        std::pair{ register_access{15, 0, 2}, std::optional<register_access>{} }, // di
        std::pair{ register_access{13, 0, 2}, std::optional<register_access>{} }, // bp
        std::pair{ register_access{11, 0, 2}, std::optional<register_access>{} }  // bx
    };

    constexpr const char* mov_name = "mov";
    constexpr const char* add_name = "add";
    constexpr const char* sub_name = "sub";
    constexpr const char* cmp_name = "cmp";

    std::unordered_map<opcode, const char*> opcodes
    {
        { opcode::mov_normal, mov_name },
        { opcode::mov_immediate_to_register_or_memory, mov_name },
        { opcode::mov_immediate_to_register, mov_name },
        { opcode::mov_memory_to_accumulator, mov_name },
        { opcode::mov_accumulator_to_memory, mov_name },
        { opcode::mov_to_segment_register, mov_name },
        { opcode::mov_from_segment_register, mov_name },

        { opcode::add_normal, add_name },
        { opcode::add_immediate_to_register_or_memory, add_name },
        { opcode::add_immediate_to_accumulator, add_name },

        { opcode::sub_normal, sub_name },
        { opcode::sub_immediate_from_register_or_memory, sub_name },
        { opcode::sub_immediate_from_accumulator, sub_name },

        { opcode::cmp_normal, cmp_name },
        { opcode::cmp_immediate_with_register_or_memory, cmp_name },
        { opcode::cmp_immediate_with_accumulator, cmp_name },

        { opcode::je, "je" },
        { opcode::jl, "jl" },
        { opcode::jle, "jle" },
        { opcode::jb, "jb" },
        { opcode::jbe, "jbe" },
        { opcode::jp, "jp" },
        { opcode::jo, "jo" },
        { opcode::js, "js" },
        { opcode::jne, "jne" },
        { opcode::jnl, "jnl" },
        { opcode::jg, "jg" },
        { opcode::jnb, "jnb" },
        { opcode::ja, "ja" },
        { opcode::jnp, "jnp" },
        { opcode::jno, "jno" },
        { opcode::jns, "jns" },
        { opcode::loop, "loop" },
        { opcode::loopz, "loopz" },
        { opcode::loopnz, "loopnz" },
        { opcode::jcxz, "jcxz" }
    };

    std::unordered_map<opcode, operation_type> opcode_translation
    {
        { opcode::mov_normal, operation_type::op_mov },
        { opcode::mov_immediate_to_register_or_memory, operation_type::op_mov },
        { opcode::mov_immediate_to_register, operation_type::op_mov },
        { opcode::mov_memory_to_accumulator, operation_type::op_mov },
        { opcode::mov_accumulator_to_memory, operation_type::op_mov },
        { opcode::mov_to_segment_register, operation_type::op_mov },
        { opcode::mov_from_segment_register, operation_type::op_mov },

        { opcode::add_normal, operation_type::op_add },
        { opcode::add_immediate_to_register_or_memory, operation_type::op_add },
        { opcode::add_immediate_to_accumulator, operation_type::op_add },

        { opcode::sub_normal, operation_type::op_sub },
        { opcode::sub_immediate_from_register_or_memory, operation_type::op_sub },
        { opcode::sub_immediate_from_accumulator, operation_type::op_sub },

        { opcode::cmp_normal, operation_type::op_cmp },
        { opcode::cmp_immediate_with_register_or_memory, operation_type::op_cmp },
        { opcode::cmp_immediate_with_accumulator, operation_type::op_cmp },

        { opcode::je, operation_type::op_je },
        { opcode::jl, operation_type::op_jl },
        { opcode::jle, operation_type::op_jle },
        { opcode::jb, operation_type::op_jb },
        { opcode::jbe, operation_type::op_jbe },
        { opcode::jp, operation_type::op_jp },
        { opcode::jo, operation_type::op_jo },
        { opcode::js, operation_type::op_js },
        { opcode::jne, operation_type::op_jne },
        { opcode::jnl, operation_type::op_jnl },
        { opcode::jg, operation_type::op_jg },
        { opcode::jnb, operation_type::op_jnb },
        { opcode::ja, operation_type::op_ja },
        { opcode::jnp, operation_type::op_jnp },
        { opcode::jno, operation_type::op_jno },
        { opcode::jns, operation_type::op_jns },
        { opcode::loop, operation_type::op_loop },
        { opcode::loopz, operation_type::op_loopz },
        { opcode::loopnz, operation_type::op_loopnz },
        { opcode::jcxz, operation_type::op_jcxz }
    };

    std::vector<std::unordered_map<uint8_t, opcode>> opcode_maps
    {
        {
            { 0b1000'1110, opcode::mov_to_segment_register },
            { 0b1000'1100, opcode::mov_from_segment_register },

            { 0b0111'0100 , opcode::je },
            { 0b0111'1100 , opcode::jl },
            { 0b0111'1110 , opcode::jle },
            { 0b0111'0010 , opcode::jb },
            { 0b0111'0110 , opcode::jbe },
            { 0b0111'1010 , opcode::jp },
            { 0b0111'0000 , opcode::jo },
            { 0b0111'1000 , opcode::js },
            { 0b0111'0101 , opcode::jne },
            { 0b0111'1101 , opcode::jnl },
            { 0b0111'1111 , opcode::jg },
            { 0b0111'0011 , opcode::jnb },
            { 0b0111'0111 , opcode::ja },
            { 0b0111'1011 , opcode::jnp },
            { 0b0111'0001 , opcode::jno },
            { 0b0111'1001 , opcode::jns },
            { 0b1110'0010 , opcode::loop },
            { 0b1110'0001 , opcode::loopz },
            { 0b1110'0000 , opcode::loopnz },
            { 0b1110'0011 , opcode::jcxz },
        },
        {
            { 0b1100'011, opcode::mov_immediate_to_register_or_memory },
            { 0b1010'000, opcode::mov_memory_to_accumulator },
            { 0b1010'001, opcode::mov_accumulator_to_memory },

            { 0b0000'010, opcode::add_immediate_to_accumulator },
            { 0b0010'110, opcode::sub_immediate_from_accumulator },
            { 0b0011'110, opcode::cmp_immediate_with_accumulator }
        },
        {
            { 0b1000'10, opcode::mov_normal },

            { 0b0000'00, opcode::add_normal },
            { 0b0010'10, opcode::sub_normal },
            { 0b0011'10, opcode::cmp_normal },
            { 0b1000'00, opcode::arithmetic_immediate }
        },
        {},
        {
            { 0b1011, opcode::mov_immediate_to_register }
        }
    };

    opcode read_opcode(uint8_t b)
    {
        for (size_t i = 0; i < opcode_maps.size(); ++i)
        {
            std::unordered_map<uint8_t, opcode>& opcode_map = opcode_maps[i];

            if (uint8_t key = b >> i; opcode_map.contains(key))
                return opcode_map[key];
        }

        return opcode::none;
    }

    int8_t get_displacement_bytes(uint8_t mod, uint8_t rm)
    {
        switch (mod)
        {
        case 0b00: // memory mode, no displacement unless direct address
            return (rm == 0b110) * 2;
        case 0b01: // memory mode, 8-bit displacement
            return 1;
        case 0b10: // memory mode, 16-bit displacement
            return 2;
        case 0b11: // register mode, no displacement
        default:
            return 0;
        }
    }

    bool read_and_advance(data_iterator& iter, const data_iterator& iter_end, uint8_t& b)
    {
        if (iter == iter_end)
            return false;

        b = *iter++;

        return true;
    }

    bool read_displacement(data_iterator& iter, const data_iterator& iter_end, instruction_fields& inst)
    {
        const int8_t displacement_bytes = get_displacement_bytes(inst.mod, inst.rm);

        if (displacement_bytes > 0)
        {
            if (!read_and_advance(iter, iter_end, inst.disp_lo))
                return false;

            if (displacement_bytes > 1)
            {
                if (!read_and_advance(iter, iter_end, inst.disp_hi))
                    return false;
            }
        }

        return true;
    }

    bool read_data(data_iterator& iter, const data_iterator& iter_end, instruction_fields& inst)
    {
        if (!read_and_advance(iter, iter_end, inst.data_lo))
            return false;

        if (inst.w && !inst.s)
        {
            if (!read_and_advance(iter, iter_end, inst.data_hi))
                return false;
        }

        return true;
    }

    int16_t get_instruction_data(const instruction_fields& inst)
    {
        if (inst.w && !inst.s)
            return static_cast<int16_t>(inst.data_lo + (inst.data_hi << 8));
        else
            return static_cast<int8_t>(inst.data_lo);
    }

    std::string get_instruction_data_string(const instruction_fields& inst)
    {
        return std::to_string(get_instruction_data(inst));
    }

    std::string get_instruction_address_string(const instruction_fields& inst)
    {
        return "[" + get_instruction_data_string(inst) + "]";
    }

    int16_t get_instruction_address(const instruction_fields& inst)
    {
        return get_instruction_data(inst);
    }

    int16_t get_instruction_displacement(const instruction_fields& inst, int8_t bytes)
    {
        switch (bytes)
        {
        case 0:
        default:
            return 0;
        case 1:
            return static_cast<int8_t>(inst.disp_lo);
        case 2:
            return static_cast<int16_t>((inst.disp_hi << 8) + inst.disp_lo);
        }
    }

    effective_address_expression get_instruction_memory(const instruction_fields& inst)
    {
        int8_t displacement_bytes = 0;
        bool directAddress = false;
        switch (inst.mod)
        {
        case 0b00: // direct address, 16-bit displacement
            displacement_bytes = 2;
            directAddress = (inst.rm == 0b110);
            break;

        case 0b01: // memory mode, 8-bit displacement
            displacement_bytes = 1;
            break;

        case 0b10: // memory mode, 16-bit displacement
            displacement_bytes = 2;
            break;
        }

        const auto displacement = get_instruction_displacement(inst, displacement_bytes);

        effective_address_expression effective_address{};
        if (directAddress)
        {
            effective_address.explicit_segment = displacement;
        }
        else
        {
            effective_address.displacement = displacement;

            auto [term1, term2] = effective_addresses[inst.rm];

            effective_address.term1 = effective_address_term{ term1, 0 };
            if (term2.has_value())
                effective_address.term2 = effective_address_term{ term2.value(), 0 };
        }

        return effective_address;
    }
}

char const* get_register_name(const register_access& reg_access)
{
    return registers[reg_access.index + reg_access.offset];
}

char const* get_mneumonic(operation_type type)
{
    return mnemonics[type];
}

std::optional<instruction_fields> read_fields(data_iterator& data_iter, const data_iterator& data_end)
{
    instruction_fields inst{};

    uint8_t b = 0;
    if (!read_and_advance(data_iter, data_end, b))
        return {};

    inst.opcode = read_opcode(b);

    switch (inst.opcode)
    {
    case opcode::mov_normal:
    case opcode::add_normal:
    case opcode::sub_normal:
    case opcode::cmp_normal:
    {
        inst.w = b & 1;
        b >>= 1;
        inst.d = b & 1;

        if (!read_and_advance(data_iter, data_end, b))
            return {};

        inst.rm = b & 0b111;
        b >>= 3;
        inst.reg = b & 0b111;
        b >>= 3;
        inst.mod = b;

        if (!read_displacement(data_iter, data_end, inst))
            return {};

        break;
    }

    case opcode::arithmetic_immediate:
        inst.s = (b >> 1) & 1;
        [[fallthrough]];

    case opcode::mov_immediate_to_register_or_memory:
    {
        inst.w = b & 1;

        if (!read_and_advance(data_iter, data_end, b))
            return {};

        inst.rm = b & 0b111;
        b >>= 3;
        uint8_t op = b & 0b111;
        b >>= 3;
        inst.mod = b;

        if (inst.opcode == opcode::arithmetic_immediate)
        {
            inst.opcode = [&op]
            {
                switch (op)
                {
                case 0b000: return opcode::add_immediate_to_register_or_memory;
                case 0b101: return opcode::sub_immediate_from_register_or_memory;
                case 0b111: return opcode::cmp_immediate_with_register_or_memory;
                default:    return opcode::none;
                }
            }();
        }

        if (!read_displacement(data_iter, data_end, inst))
            return {};
        if (!read_data(data_iter, data_end, inst))
            return {};

        break;
    }

    case opcode::mov_immediate_to_register:
        inst.reg = b & 0b111;
        b >>= 3;
        [[fallthrough]];

    case opcode::add_immediate_to_accumulator:
    case opcode::sub_immediate_from_accumulator:
    case opcode::cmp_immediate_with_accumulator:
    case opcode::mov_memory_to_accumulator:
    case opcode::mov_accumulator_to_memory:
    {
        inst.w = b & 1;

        if (!read_data(data_iter, data_end, inst))
            return {};

        break;
    }

    case opcode::mov_to_segment_register:
    case opcode::mov_from_segment_register:
    {
        if (!read_and_advance(data_iter, data_end, b))
            return {};

        inst.rm = b & 0b111;
        b >>= 3;
        inst.sr = b & 0b11;
        b >>= 3;
        inst.mod = b;

        if (!read_displacement(data_iter, data_end, inst))
            return {};

        break;
    }

    case opcode::je:
    case opcode::jl:
    case opcode::jle:
    case opcode::jb:
    case opcode::jbe:
    case opcode::jp:
    case opcode::jo:
    case opcode::js:
    case opcode::jne:
    case opcode::jnl:
    case opcode::jg:
    case opcode::jnb:
    case opcode::ja:
    case opcode::jnp:
    case opcode::jno:
    case opcode::jns:
    case opcode::loop:
    case opcode::loopz:
    case opcode::loopnz:
    case opcode::jcxz:
    {
        if (!read_and_advance(data_iter, data_end, inst.data_lo))
            return {};

        break;
    }

    default:
    case opcode::none:
        return {};
    }

    return inst;
}

std::optional<instruction> decode_instruction(const instruction_fields& inst)
{
    instruction inst_ex;
    inst_ex.op = opcode_translation[inst.opcode];

    const bool wide_data = (inst.w && !inst.s);
    const uint32_t inst_size = wide_data ? 4 : 3;
    const uint32_t data_size = wide_data ? 2 : 1;

    switch (inst.opcode)
    {
    case opcode::mov_normal:
    case opcode::add_normal:
    case opcode::sub_normal:
    case opcode::cmp_normal:
    {
        if (inst.mod == 0b11) // register mode
        {
            inst_ex.size = 2;
            inst_ex.operands[0] = register_access
            {
                .index = (inst.d ? inst.reg : inst.rm) + 8U * inst.w,
                .offset = 0,
                .count = data_size
            };
            inst_ex.operands[1] = register_access
            {
                .index = (inst.d ? inst.rm : inst.reg) + 8U * inst.w,
                .offset = 0,
                .count = data_size
            };
        }
        else // memory mode
        {
            effective_address_expression address_expr = get_instruction_memory(inst);

            inst_ex.size = inst_size;
            if (inst.d)
            {
                inst_ex.operands[0] = register_access
                {
                    .index = inst.reg + 8U * inst.w,
                    .offset = 0,
                    .count = data_size
                };
                inst_ex.operands[1] = address_expr;
            }
            else
            {
                inst_ex.operands[0] = address_expr;
                inst_ex.operands[1] = register_access
                {
                    .index = inst.reg + 8U * inst.w,
                    .offset = 0,
                    .count = data_size
                };
            }
        }
        break;
    }

    case opcode::mov_immediate_to_register_or_memory:
    case opcode::add_immediate_to_register_or_memory:
    case opcode::sub_immediate_from_register_or_memory:
    case opcode::cmp_immediate_with_register_or_memory:
    {
        inst_ex.size = inst_size;

        if (inst.mod == 0b11) // register mode
        {
            inst_ex.operands[0] = register_access
            {
                .index = inst.rm + 8U * inst.w,
                .offset = 0,
                .count = data_size
            };
            inst_ex.operands[1] = immediate
            {
                .value = get_instruction_data(inst),
                .flags = 0
            };
        }
        else // memory mode
        {
            effective_address_expression address_expr = get_instruction_memory(inst);
            inst_ex.operands[0] = address_expr;
            inst_ex.operands[1] = immediate
            {
                .value = get_instruction_data(inst),
                .flags = 0
            };
        }
        break;
    }

    case opcode::mov_immediate_to_register:
    case opcode::add_immediate_to_accumulator:
    case opcode::sub_immediate_from_accumulator:
    case opcode::cmp_immediate_with_accumulator:
    {
        inst_ex.size = inst_size;
        inst_ex.operands[0] = register_access
        {
            .index = inst.reg + 8U * inst.w,
            .offset = 0,
            .count = data_size
        };
        inst_ex.operands[1] = immediate
        {
            .value = get_instruction_data(inst),
            .flags = 0
        };
        break;
    }

    case opcode::mov_memory_to_accumulator:
    {
        inst_ex.size = inst_size;
        inst_ex.operands[0] = register_access
        {
            .index = inst.w ? 8U : 0,
            .offset = 0,
            .count = data_size
        };
        inst_ex.operands[1] = immediate
        {
            .value = get_instruction_address(inst),
            .flags = 0
        };
        break;
    }

    case opcode::mov_accumulator_to_memory:
    {
        inst_ex.size = inst_size;
        inst_ex.operands[0] = immediate
        {
            .value = get_instruction_address(inst),
            .flags = 0
        };
        inst_ex.operands[1] = register_access
        {
            .index = inst.w ? 8U : 0,
            .offset = 0,
            .count = data_size
        };
        break;
    }

    case opcode::mov_to_segment_register:
    case opcode::mov_from_segment_register:
        // todo
        break;

    case opcode::je:
    case opcode::jl:
    case opcode::jle:
    case opcode::jb:
    case opcode::jbe:
    case opcode::jp:
    case opcode::jo:
    case opcode::js:
    case opcode::jne:
    case opcode::jnl:
    case opcode::jg:
    case opcode::jnb:
    case opcode::ja:
    case opcode::jnp:
    case opcode::jno:
    case opcode::jns:
    case opcode::loop:
    case opcode::loopz:
    case opcode::loopnz:
    case opcode::jcxz:
    {
        inst_ex.size = 2;
        inst_ex.operands[0] = register_access
        {
            .index = inst.reg + 8U * inst.w,
            .offset = 0,
            .count = 1
        };
        break;
    }

    default:
    case opcode::none:
        return {};
    }

    return inst_ex;
}

std::optional<instruction> decode_instruction(data_iterator& data_iter, const data_iterator& data_end)
{
    const std::optional<instruction_fields> fields_result = read_fields(data_iter, data_end);
    if (fields_result.has_value())
    {
        const std::optional<instruction> inst = decode_instruction(fields_result.value());
        return inst;
    }
    return {};
}
