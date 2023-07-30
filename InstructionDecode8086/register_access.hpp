#ifndef WS_REGISTERACCESS_HPP
#define WS_REGISTERACCESS_HPP

#include <cstdint>
#include <type_traits>

#include "flag_utils.hpp"

using register_index = uint32_t;

struct register_access
{
    register_index index{};
    uint32_t offset{};
    uint32_t count{};
};

enum class register_types : uint32_t
{
    none = 0,
    ax = 1 << 0,
    ah = 1 << 1,
    al = 1 << 2,
    bx = 1 << 3,
    bh = 1 << 4,
    bl = 1 << 5,
    cx = 1 << 6,
    ch = 1 << 7,
    cl = 1 << 8,
    dx = 1 << 9,
    dh = 1 << 10,
    dl = 1 << 11,
    sp = 1 << 12,
    bp = 1 << 13,
    si = 1 << 14,
    di = 1 << 15,
    cs = 1 << 16,
    ds = 1 << 17,
    ss = 1 << 18,
    es = 1 << 19,
    ip = 1 << 20,
    flags = 1 << 21
};

FLAG_OPERATIONS(register_types);

char const* get_register_name(const register_access& reg_access);

register_types get_register_types(const register_access& reg_access);

#endif
