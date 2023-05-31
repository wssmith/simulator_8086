#ifndef WS_OPCODE_HPP
#define WS_OPCODE_HPP

#include <cstdint>

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
};

#endif
