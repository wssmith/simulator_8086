#include "decoder.hpp"

#include <array>
#include <unordered_map>

constexpr std::array registers =
{
    "al", "cl", "dl", "bl",
    "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx",
    "sp", "bp", "si", "di",
    "cs", "ds", "ss", "es"
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

char const* get_register_name(const register_access& reg_access)
{
    return registers[reg_access.index + reg_access.offset];
}

char const* get_mneumonic(operation_type type)
{
    return mnemonics[type];
}
