#include "register_access.hpp"

#include <array>

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

    constexpr std::array register_type_table =
    {
        std::array{ register_types::ax, register_types::ah, register_types::al },
        std::array{ register_types::bx, register_types::bh, register_types::bl },
        std::array{ register_types::cx, register_types::ch, register_types::cl },
        std::array{ register_types::dx, register_types::dh, register_types::dl },
        std::array{ register_types::sp, register_types::sp, register_types::sp },
        std::array{ register_types::bp, register_types::bp, register_types::bp },
        std::array{ register_types::si, register_types::si, register_types::si },
        std::array{ register_types::di, register_types::di, register_types::di },
        std::array{ register_types::cs, register_types::cs, register_types::cs },
        std::array{ register_types::ds, register_types::ds, register_types::ds },
        std::array{ register_types::ss, register_types::ss, register_types::ss },
        std::array{ register_types::es, register_types::es, register_types::es },
        std::array{ register_types::ip, register_types::ip, register_types::ip },
        std::array{ register_types::flags, register_types::flags, register_types::flags }
    };
}

char const* get_register_name(register_access reg_access)
{
    return registers[reg_access.index][reg_access.offset + (reg_access.count == 1)];
}

register_types get_register_types(register_access reg_access)
{
    return register_type_table[reg_access.index][reg_access.offset + (reg_access.count == 1)];
}
