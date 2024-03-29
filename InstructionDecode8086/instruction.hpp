﻿#ifndef WS_INSTRUCTION_HPP
#define WS_INSTRUCTION_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <variant>

#include "flag_utils.hpp"
#include "register_access.hpp"

enum class operation_type : uint32_t
{
    none,

    mov,

    add,
    sub,
    cmp,

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

    jmp,

    nop,

    count,
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

FLAG_OPERATIONS(instruction_flags);

struct direct_address
{
    uint32_t address;
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

FLAG_OPERATIONS(immediate_flags);

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

#endif
