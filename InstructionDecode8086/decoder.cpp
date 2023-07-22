#include "decoder.hpp"

#include <array>
#include <exception>
#include <utility>
#include <unordered_map>
#include <string>

#include "instruction.hpp"

namespace
{
    constexpr std::array registers =
    {
        std::array{ "ax", "ah", "al" },
        std::array{ "bx", "bh", "bl" },
        std::array{ "cx", "ch", "cl" },
        std::array{ "dx", "dh", "dl" },
        std::array{ "sp", "sp", "sp" },
        std::array{ "bp", "bp", "bp" },
        std::array{ "si", "si", "si" },
        std::array{ "di", "di", "di" },
        std::array{ "cs", "cs", "cs" },
        std::array{ "ds", "ds", "ds" },
        std::array{ "ss", "ss", "ss" },
        std::array{ "es", "es", "es" },
        std::array{ "ip", "ip", "ip" },
        std::array{ "flags", "flags", "flags" }
    };

    constexpr std::array<uint8_t, 16> register_index_map =
    {
        0, 2, 3, 1, 0, 2, 3, 1, 0, 2, 3, 1, 4, 5, 6, 7
    };

    constexpr std::array<uint8_t, 16> register_offset_map =
    {
        1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    constexpr std::array<uint8_t, 16> register_count_map =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2
    };

    constexpr std::array<uint8_t, 4> segment_register_index_map = { 11, 8, 10, 9 };

    std::unordered_map<operation_type, const char*> mnemonics
    {
        { operation_type::mov, "mov" },
        { operation_type::add, "add" },
        { operation_type::sub, "sub" },
        { operation_type::cmp, "cmp" },
        { operation_type::je, "je" },
        { operation_type::jl, "jl" },
        { operation_type::jle, "jle" },
        { operation_type::jb, "jb" },
        { operation_type::jbe, "jbe" },
        { operation_type::jp, "jp" },
        { operation_type::jo, "jo" },
        { operation_type::js, "js" },
        { operation_type::jne, "jne" },
        { operation_type::jnl, "jnl" },
        { operation_type::jg, "jg" },
        { operation_type::jnb, "jnb" },
        { operation_type::ja, "ja" },
        { operation_type::jnp, "jnp" },
        { operation_type::jno, "jno" },
        { operation_type::jns, "jns" },
        { operation_type::loop, "loop" },
        { operation_type::loopz, "loopz" },
        { operation_type::loopnz, "loopnz" },
        { operation_type::jcxz, "jcxz" },
        { operation_type::jmp, "jmp" },
        { operation_type::nop, "nop" }
    };

    constexpr std::array<std::pair<register_access, std::optional<register_access>>, 8> effective_addresses =
    {
        std::pair{ register_access{1, 0, 2}, register_access{6, 0, 2} }, // bx + si
        std::pair{ register_access{1, 0, 2}, register_access{7, 0, 2} }, // bx + di
        std::pair{ register_access{5, 0, 2}, register_access{6, 0, 2} }, // bp + si
        std::pair{ register_access{5, 0, 2}, register_access{7, 0, 2} }, // bp + di
        std::pair{ register_access{6, 0, 2}, std::optional<register_access>{} }, // si
        std::pair{ register_access{7, 0, 2}, std::optional<register_access>{} }, // di
        std::pair{ register_access{5, 0, 2}, std::optional<register_access>{} }, // bp
        std::pair{ register_access{1, 0, 2}, std::optional<register_access>{} }  // bx
    };

    enum class opcode : uint16_t
    {
        none = 0,

        mov_normal,
        mov_immediate_to_register_or_memory,
        mov_immediate_to_register,
        mov_memory_to_accumulator,
        mov_accumulator_to_memory,
        mov_to_segment_register,
        mov_from_segment_register,

        add_normal,
        add_immediate_to_register_or_memory,
        add_immediate_to_accumulator,

        sub_normal,
        sub_immediate_from_register_or_memory,
        sub_immediate_from_accumulator,

        cmp_normal,
        cmp_immediate_with_register_or_memory,
        cmp_immediate_with_accumulator,

        arithmetic_immediate,

        je,
        jl,
        jle,
        jb,
        jbe,
        jp,
        jo,
        js,
        jne,
        jnl,
        jg,
        jnb,
        ja,
        jnp,
        jno,
        jns,
        loop,
        loopz,
        loopnz,
        jcxz,

        jmp_direct,
        jmp_direct_short,
        jmp_indirect,
        jmp_indirect_near,
        jmp_indirect_far,

        nop,

        count
    };

    using opcode_type = std::underlying_type_t<opcode>;

    std::unordered_map<opcode, operation_type> opcode_translation
    {
        { opcode::mov_normal, operation_type::mov },
        { opcode::mov_immediate_to_register_or_memory, operation_type::mov },
        { opcode::mov_immediate_to_register, operation_type::mov },
        { opcode::mov_memory_to_accumulator, operation_type::mov },
        { opcode::mov_accumulator_to_memory, operation_type::mov },
        { opcode::mov_to_segment_register, operation_type::mov },
        { opcode::mov_from_segment_register, operation_type::mov },

        { opcode::add_normal, operation_type::add },
        { opcode::add_immediate_to_register_or_memory, operation_type::add },
        { opcode::add_immediate_to_accumulator, operation_type::add },

        { opcode::sub_normal, operation_type::sub },
        { opcode::sub_immediate_from_register_or_memory, operation_type::sub },
        { opcode::sub_immediate_from_accumulator, operation_type::sub },

        { opcode::cmp_normal, operation_type::cmp },
        { opcode::cmp_immediate_with_register_or_memory, operation_type::cmp },
        { opcode::cmp_immediate_with_accumulator, operation_type::cmp },

        { opcode::je, operation_type::je },
        { opcode::jl, operation_type::jl },
        { opcode::jle, operation_type::jle },
        { opcode::jb, operation_type::jb },
        { opcode::jbe, operation_type::jbe },
        { opcode::jp, operation_type::jp },
        { opcode::jo, operation_type::jo },
        { opcode::js, operation_type::js },
        { opcode::jne, operation_type::jne },
        { opcode::jnl, operation_type::jnl },
        { opcode::jg, operation_type::jg },
        { opcode::jnb, operation_type::jnb },
        { opcode::ja, operation_type::ja },
        { opcode::jnp, operation_type::jnp },
        { opcode::jno, operation_type::jno },
        { opcode::jns, operation_type::jns },
        { opcode::loop, operation_type::loop },
        { opcode::loopz, operation_type::loopz },
        { opcode::loopnz, operation_type::loopnz },
        { opcode::jcxz, operation_type::jcxz },

        { opcode::jmp_direct, operation_type::jmp },
        { opcode::jmp_direct_short, operation_type::jmp },
        { opcode::jmp_indirect_near, operation_type::jmp },
        { opcode::jmp_indirect_far, operation_type::jmp },
  
        { opcode::nop, operation_type::nop }
    };

    std::vector<std::unordered_map<uint8_t, opcode>> opcode_maps
    {
        {
            { 0b1000'1110, opcode::mov_to_segment_register },
            { 0b1000'1100, opcode::mov_from_segment_register },

            { 0b0111'0100, opcode::je },
            { 0b0111'1100, opcode::jl },
            { 0b0111'1110, opcode::jle },
            { 0b0111'0010, opcode::jb },
            { 0b0111'0110, opcode::jbe },
            { 0b0111'1010, opcode::jp },
            { 0b0111'0000, opcode::jo },
            { 0b0111'1000, opcode::js },
            { 0b0111'0101, opcode::jne },
            { 0b0111'1101, opcode::jnl },
            { 0b0111'1111, opcode::jg },
            { 0b0111'0011, opcode::jnb },
            { 0b0111'0111, opcode::ja },
            { 0b0111'1011, opcode::jnp },
            { 0b0111'0001, opcode::jno },
            { 0b0111'1001, opcode::jns },
            { 0b1110'0010, opcode::loop },
            { 0b1110'0001, opcode::loopz },
            { 0b1110'0000, opcode::loopnz },
            { 0b1110'0011, opcode::jcxz },

            { 0b1110'1001, opcode::jmp_direct },
            { 0b1110'1011, opcode::jmp_direct_short },
            { 0b1111'1111, opcode::jmp_indirect },

            { 0b1001'0000, opcode::nop }
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

    struct instruction_fields
    {
        uint16_t size{};
        opcode opcode{};
        uint8_t mod{};
        uint8_t reg{};
        uint8_t rm{};
        uint8_t sr{};
        uint8_t disp_lo{};
        uint8_t disp_hi{};
        uint8_t data_lo{};
        uint8_t data_hi{};
        bool d{};
        bool w{};
        bool s{};
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

    void read_and_advance(data_iterator& iter, const data_iterator& iter_end, uint8_t& b)
    {
        if (iter == iter_end)
            throw std::exception{ "Cannot dereference out-of-range iterator for binary data." };
        
        b = *iter++;
    }

    void read_displacement(data_iterator& iter, const data_iterator& iter_end, instruction_fields& fields)
    {
        const int8_t displacement_bytes = get_displacement_bytes(fields.mod, fields.rm);

        if (displacement_bytes > 0)
        {
            read_and_advance(iter, iter_end, fields.disp_lo);

            if (displacement_bytes > 1)
                read_and_advance(iter, iter_end, fields.disp_hi);
        }
    }

    void read_data(data_iterator& iter, const data_iterator& iter_end, instruction_fields& fields)
    {
        read_and_advance(iter, iter_end, fields.data_lo);

        if (fields.w && !fields.s)
            read_and_advance(iter, iter_end, fields.data_hi);
    }

    int16_t get_instruction_data(const instruction_fields& fields)
    {
        if (fields.w && !fields.s)
            return static_cast<int16_t>(fields.data_lo + (fields.data_hi << 8));
        else
            return static_cast<int8_t>(fields.data_lo);
    }

    uint16_t get_instruction_address(const instruction_fields& fields)
    {
        if (fields.w && !fields.s)
            return static_cast<uint16_t>(fields.data_lo + (fields.data_hi << 8));
        else
            return static_cast<uint8_t>(fields.data_lo);
    }

    int16_t get_instruction_displacement(const instruction_fields& fields, int8_t bytes)
    {
        switch (bytes)
        {
            case 0:
            default:
                return 0;
            case 1:
                return static_cast<int8_t>(fields.disp_lo);
            case 2:
                return static_cast<int16_t>((fields.disp_hi << 8) + fields.disp_lo);
        }
    }

    uint16_t get_instruction_direct_address(const instruction_fields& fields, int8_t bytes)
    {
        switch (bytes)
        {
            case 0:
            default:
                return 0;
            case 1:
                return static_cast<uint8_t>(fields.disp_lo);
            case 2:
                return static_cast<uint16_t>((fields.disp_hi << 8) + fields.disp_lo);
        }
    }

    instruction_operand get_address_operand(const instruction_fields& fields)
    {
        int8_t displacement_bytes = 0;
        bool directAddress = false;
        switch (fields.mod)
        {
            case 0b00: // direct address, 16-bit displacement
                displacement_bytes = 2;
                directAddress = (fields.rm == 0b110);
                break;

            case 0b01: // memory mode, 8-bit displacement
                displacement_bytes = 1;
                break;

            case 0b10: // memory mode, 16-bit displacement
                displacement_bytes = 2;
                break;
        }
        
        if (directAddress)
        {
            const uint16_t address = get_instruction_direct_address(fields, displacement_bytes);
            return direct_address{ .address = address };
        }
        else
        {
            effective_address_expression effective_address{};

            const auto displacement = get_instruction_displacement(fields, displacement_bytes);
            effective_address.displacement = displacement;

            const auto& [term1, term2] = effective_addresses[fields.rm];

            effective_address.term1 = effective_address_term
            {
                .reg = term1,
                .scale = 0
            };

            if (term2.has_value())
            {
                effective_address.term2 = effective_address_term
                {
                    .reg = term2.value(),  // NOLINT(bugprone-unchecked-optional-access)
                    .scale = 0
                };
            }

            return effective_address;
        }
    }

    register_access get_register_from_index(size_t index)
    {
        return register_access
        {
            .index = register_index_map[index],
            .offset = register_offset_map[index],
            .count = register_count_map[index]
        };
    }

    instruction decode_fields(const instruction_fields& fields, uint32_t address)
    {
        instruction inst
        {
            .address = address,
            .size = fields.size,
            .op = opcode_translation[fields.opcode],
            .flags = fields.w ? instruction_flags::wide : instruction_flags::none
        };

        switch (fields.opcode)
        {
            case opcode::mov_normal:
            case opcode::add_normal:
            case opcode::sub_normal:
            case opcode::cmp_normal:
            {
                if (fields.mod == 0b11) // register mode
                {
                    const size_t op1_index = fields.rm + 8 * fields.w;
                    inst.operands[fields.d] = get_register_from_index(op1_index);

                    const size_t op2_index = fields.reg + 8 * fields.w;
                    inst.operands[!fields.d] = get_register_from_index(op2_index);
                }
                else // memory mode
                {
                    const size_t op_index = fields.reg + 8 * fields.w;
                    inst.operands[fields.d] = get_address_operand(fields);
                    inst.operands[!fields.d] = get_register_from_index(op_index);
                }
                break;
            }

            case opcode::add_immediate_to_register_or_memory:
            case opcode::sub_immediate_from_register_or_memory:
            case opcode::cmp_immediate_with_register_or_memory:
            case opcode::mov_immediate_to_register_or_memory:
            {
                if (fields.mod == 0b11) // register mode
                {
                    const size_t op_index = fields.rm + 8 * fields.w;
                    inst.operands[0] = get_register_from_index(op_index);
                }
                else // memory mode
                {
                    inst.operands[0] = get_address_operand(fields);
                }

                inst.operands[1] = immediate
                {
                    .value = get_instruction_data(fields)
                };
                break;
            }

            case opcode::mov_immediate_to_register:
            case opcode::add_immediate_to_accumulator:
            case opcode::sub_immediate_from_accumulator:
            case opcode::cmp_immediate_with_accumulator:
            {
                const size_t op_index = fields.reg + 8 * fields.w;
                inst.operands[0] = get_register_from_index(op_index);
                inst.operands[1] = immediate
                {
                    .value = get_instruction_data(fields)
                };
                break;
            }

            case opcode::mov_memory_to_accumulator:
            case opcode::mov_accumulator_to_memory:
            {
                const bool to_memory = (fields.opcode == opcode::mov_accumulator_to_memory);

                const size_t op_index = fields.w ? 8 : 0;
                inst.operands[to_memory] = get_register_from_index(op_index);
                inst.operands[!to_memory] = direct_address
                {
                    .address = get_instruction_address(fields),
                };
                break;
            }

            case opcode::mov_to_segment_register:
            case opcode::mov_from_segment_register:
            {
                const bool from_segment = (fields.opcode == opcode::mov_from_segment_register);

                inst.operands[from_segment] = register_access
                {
                    .index = segment_register_index_map[fields.sr],
                    .offset = 0,
                    .count = 2
                };

                if (fields.mod == 0b11) // register mode
                {
                    const size_t non_segment_index = fields.rm + 8;
                    inst.operands[!from_segment] = register_access
                    {
                        .index = register_index_map[non_segment_index],
                        .offset = 0,
                        .count = 2
                    };
                }
                else // memory mode
                {
                    inst.operands[!from_segment] = get_address_operand(fields);
                }
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
                inst.operands[0] = immediate
                {
                    .value = get_instruction_data(fields),
                    .flags = immediate_flags::relative_jump_displacement
                };
                break;
            }

            case opcode::nop:
                break;

            default:
            case opcode::none:
                std::string error_message = "Unrecognized opcode while decoding fields: " + std::to_string(static_cast<opcode_type>(fields.opcode));
                throw std::exception{ error_message.c_str() };
        }

        return inst;
    }

    instruction_fields read_fields(data_iterator& data_iter, const data_iterator& data_end)
    {
        const data_iterator initial_position = data_iter;

        instruction_fields fields{};

        uint8_t b = 0;
        read_and_advance(data_iter, data_end, b);

        fields.opcode = read_opcode(b);

        switch (fields.opcode)
        {
            case opcode::mov_normal:
            case opcode::add_normal:
            case opcode::sub_normal:
            case opcode::cmp_normal:
            {
                fields.w = b & 1;
                b >>= 1;
                fields.d = b & 1;

                read_and_advance(data_iter, data_end, b);

                fields.rm = b & 0b111;
                b >>= 3;
                fields.reg = b & 0b111;
                b >>= 3;
                fields.mod = b;

                read_displacement(data_iter, data_end, fields);

                break;
            }

            case opcode::arithmetic_immediate:
                fields.s = (b >> 1) & 1;
                [[fallthrough]];

            case opcode::mov_immediate_to_register_or_memory:
            {
                fields.w = b & 1;

                read_and_advance(data_iter, data_end, b);

                fields.rm = b & 0b111;
                b >>= 3;
                uint8_t op = b & 0b111;
                b >>= 3;
                fields.mod = b;

                if (fields.opcode == opcode::arithmetic_immediate)
                {
                    fields.opcode = [&op]
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

                read_displacement(data_iter, data_end, fields);
                read_data(data_iter, data_end, fields);

                break;
            }

            case opcode::mov_immediate_to_register:
                fields.reg = b & 0b111;
                b >>= 3;
                [[fallthrough]];

            case opcode::add_immediate_to_accumulator:
            case opcode::sub_immediate_from_accumulator:
            case opcode::cmp_immediate_with_accumulator:
            case opcode::mov_memory_to_accumulator:
            case opcode::mov_accumulator_to_memory:
            {
                fields.w = b & 1;

                read_data(data_iter, data_end, fields);
                break;
            }

            case opcode::mov_to_segment_register:
            case opcode::mov_from_segment_register:
            {
                read_and_advance(data_iter, data_end, b);

                fields.rm = b & 0b111;
                b >>= 3;
                fields.sr = b & 0b11;
                b >>= 3;
                fields.mod = b;

                read_displacement(data_iter, data_end, fields);
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
                read_and_advance(data_iter, data_end, fields.data_lo);
                break;
            }

            case opcode::nop:
                break;

            default:
            case opcode::none:
                const std::string error_message = "Unrecognized opcode while reading fields: " + std::to_string(static_cast<opcode_type>(fields.opcode));
                throw std::exception{ error_message.c_str() };
        }

        const data_iterator final_position = data_iter;

        fields.size = static_cast<uint16_t>(std::distance(initial_position, final_position));

        return fields;
    }
}

instruction decode_instruction(data_iterator& data_iter, const data_iterator& data_end, uint32_t address)
{
    const instruction_fields fields = read_fields(data_iter, data_end);
    return decode_fields(fields, address);
}

char const* get_register_name(const register_access& reg_access)
{
    return registers[reg_access.index][reg_access.offset + (reg_access.count == 1)];
}

char const* get_mneumonic(operation_type type)
{
    return mnemonics[type];
}
