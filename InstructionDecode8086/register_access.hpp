#ifndef WS_REGISTERACCESS_HPP
#define WS_REGISTERACCESS_HPP

#include <cstdint>

using register_index = uint32_t;

struct register_access
{
    register_index index{};
    uint32_t offset{};
    uint32_t count{};
};

#endif
