#ifndef WS_DECODER_HPP
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

enum class instruction_flag
{
    inst_lock = 0x1,
    inst_rep = 0x2,
    inst_segment = 0x4,
    inst_wide = 0x8,
    inst_far = 0x10,
    inst_rep_ne = 0x20,
};

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

enum class effective_address_flag
{
    address_explicit_segment = 0x1,
};

struct effective_address_expression
{
    effective_address_term term1{};
    std::optional<effective_address_term> term2{};
    uint32_t explicit_segment{};
    int32_t displacement{};
    uint32_t flags{};
};

enum class immediate_flag
{
    immediate_relative_jump_displacement = 0x1,
};

struct immediate
{
    int32_t value{};
    uint32_t flags{};
};

using instruction_operand = std::variant<std::monostate, direct_address, effective_address_expression, register_access, immediate>;

struct instruction
{
    uint32_t address{};
    uint32_t size{};

    operation_type op{};
    uint32_t flags{};

    std::array<instruction_operand, 2> operands{ std::monostate{}, std::monostate{} };

    register_index segment_override{};
};

using data_iterator = std::vector<uint8_t>::const_iterator;

std::optional<instruction_fields> read_fields(data_iterator& data_iter, const data_iterator& data_end);
std::optional<instruction> decode_instruction(const instruction_fields& fields);
std::optional<instruction> decode_instruction(data_iterator& data_iter, const data_iterator& data_end);

char const* get_register_name(const register_access& reg_access);
char const* get_mneumonic(operation_type type);

#endif
