#ifndef WS_DECODER_HPP
#define WS_DECODER_HPP

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

using register_index = uint32_t;

enum class operation_type : uint32_t
{
    op_none,

    op_mov,

    op_push,

    op_pop,

    op_xchg,

    op_in,

    op_out,

    op_xlat,
    op_lea,
    op_lds,
    op_les,
    op_lahf,
    op_sahf,
    op_pushf,
    op_popf,

    op_add,

    op_adc,

    op_inc,

    op_aaa,
    op_daa,

    op_sub,

    op_sbb,

    op_dec,

    op_neg,

    op_cmp,

    op_aas,
    op_das,
    op_mul,
    op_imul,
    op_aam,
    op_div,
    op_idiv,
    op_aad,
    op_cbw,
    op_cwd,

    op_not,
    op_shl,
    op_shr,
    op_sar,
    op_rol,
    op_ror,
    op_rcl,
    op_rcr,

    op_and,

    op_test,

    op_or,

    op_xor,

    op_rep,
    op_movs,
    op_cmps,
    op_scas,
    op_lods,
    op_stos,

    op_call,

    op_jmp,

    op_ret,

    op_retf,

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

    op_int,
    op_int3,

    op_into,
    op_iret,

    op_clc,
    op_cmc,
    op_stc,
    op_cld,
    op_std,
    op_cli,
    op_sti,
    op_hlt,
    op_wait,
    op_esc,
    op_lock,
    op_segment,

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

struct register_access
{
    register_index index;
    uint32_t offset;
};

struct effective_address_term
{
    register_access reg;
    int32_t scale;
};

enum class effective_address_flag
{
    address_explicit_segment = 0x1,
};

struct effective_address_expression
{
    std::array<effective_address_term, 2> terms;
    uint32_t explicit_segment;
    int32_t displacement;
    uint32_t flags;
};

enum class immediate_flag
{
    immediate_relative_jump_displacement = 0x1,
};

struct immediate
{
    int32_t value;
    uint32_t flags;
};

enum class operand_type : uint32_t
{
    operand_none,
    operand_register,
    operand_memory,
    operand_immediate,
};

using instruction_operand = std::variant<std::monostate, effective_address_expression, register_access, immediate>;

struct instruction_ex
{
    uint32_t address{};
    uint32_t size{};

    operation_type op{};
    uint32_t flags{};

    std::array<instruction_operand, 2> operands{ std::monostate{}, std::monostate{} };

    register_index segment_override{};
};

//enum class instruction_bits_usage : u8
//{
//    bits_end,
//
//    bits_literal,
//
//    bits_d,
//    bits_s,
//    bits_w,
//    bits_v,
//    bits_z,
//    bits_mod,
//    bits_reg,
//    bits_rm,
//    brits_sr,
//    brits_disp,
//    bits_data,
//
//    bits_disp_always_w,
//    bits_w_makes_data_w,
//    bits_rm_reg_always_w,
//    bits_rel_jmp_disp,
//    bits_far,
//
//    bits_count,
//};
//
//struct instruction_bits
//{
//    instruction_bits_usage usage;
//    u8 bit_count;
//    u8 shift;
//    u8 value;
//};
//
//struct instruction_encoding
//{
//    operation_type op;
//    std::array<instruction_bits, 16> bits;
//};
//
//using instruction_table = std::vector<instruction_encoding>;

instruction_ex decode_instruction(uint32_t source_size, uint8_t* source);
char const* get_register_name(const register_access& reg_access);
char const* get_mneumonic(operation_type type);
//instruction_table get_instruction_table();

#endif
