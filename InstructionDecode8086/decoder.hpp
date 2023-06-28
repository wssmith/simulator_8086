﻿#ifndef WS_DECODER_HPP
#define WS_DECODER_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

struct instruction_fields;

using register_index = uint32_t;

enum class operation_type : uint32_t
{
    op_none,

    op_mov,

    op_add,
    op_sub,
    op_cmp,

    op_je,
    op_jl,
    op_jle,
    op_jb,
    op_jbe,
    op_jp,
    op_jo,
    op_js,
    op_jne,
    op_jnl,
    op_jg,
    op_jnb,
    op_ja,
    op_jnp,
    op_jno,
    op_jns,
    op_loop,
    op_loopz,
    op_loopnz,
    op_jcxz,

    op_count,
};

enum class instruction_flags : uint16_t
{
    none = 0,
    lock = 1 << 0,
    rep = 1 << 1,
    segment = 1 << 2,
    wide = 1 << 3,
    far = 1 << 4,
    rep_ne = 1 << 5,
};

constexpr instruction_flags operator|(instruction_flags f1, instruction_flags f2)
{
    return static_cast<instruction_flags>(static_cast<uint16_t>(f1) | static_cast<uint16_t>(f2));
}

constexpr instruction_flags operator&(instruction_flags f1, instruction_flags f2)
{
    return static_cast<instruction_flags>(static_cast<uint16_t>(f1) & static_cast<uint16_t>(f2));
}

struct direct_address
{
    uint32_t address;
};

struct register_access
{
    register_index index{};
    uint32_t offset{};
    uint32_t count{};
};

struct effective_address_term
{
    register_access reg{};
    int32_t scale{};
};

enum class effective_address_flags : uint16_t
{
    none = 0,
    segment = 1 << 0,
};

struct effective_address_expression
{
    effective_address_term term1{};
    std::optional<effective_address_term> term2{};
    uint32_t explicit_segment{};
    int32_t displacement{};
    effective_address_flags flags{};
};

enum class immediate_flags : uint16_t
{
    none = 0,
    relative_jump_displacement = 1 << 0,
};

struct immediate
{
    int32_t value{};
    immediate_flags flags{};
};

using instruction_operand = std::variant<std::monostate, effective_address_expression, direct_address, register_access, immediate>;

struct instruction
{
    uint32_t address{};
    uint32_t size{};

    operation_type op{};
    instruction_flags flags{};

    std::array<instruction_operand, 2> operands{ std::monostate{}, std::monostate{} };

    register_index segment_override{};
};

using data_iterator = std::vector<uint8_t>::const_iterator;

std::optional<instruction_fields> read_fields(data_iterator& data_iter, const data_iterator& data_end);
std::optional<instruction> decode_instruction(data_iterator& data_iter, const data_iterator& data_end, uint32_t address);

char const* get_register_name(const register_access& reg_access);
char const* get_mneumonic(operation_type type);

#endif
