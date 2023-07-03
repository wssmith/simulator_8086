#ifndef WS_FLAGUTILS_HPP
#define WS_FLAGUTILS_HPP

template<typename TFlags>
constexpr bool has_any_flag(TFlags flag_to_test, TFlags flags)
{
    return (flag_to_test & flags) != TFlags{};
}

template<typename TFlags>
constexpr bool has_all_flags(TFlags flag_to_test, TFlags flags)
{
    return (flag_to_test & flags) == flags;
}

#define FLAG_OPERATIONS(flag_type) constexpr flag_type operator|(flag_type f1, flag_type f2) \
{ \
    using underlying_type = std::underlying_type_t<flag_type>; \
    return flag_type(static_cast<underlying_type>(f1) | static_cast<underlying_type>(f2)); \
} \
\
constexpr flag_type operator&(flag_type f1, flag_type f2) \
{ \
    using underlying_type = std::underlying_type_t<flag_type>; \
    return flag_type(static_cast<underlying_type>(f1) & static_cast<underlying_type>(f2)); \
} \
\
constexpr flag_type operator^(flag_type f1, flag_type f2) \
{ \
    using underlying_type = std::underlying_type_t<flag_type>; \
    return flag_type(static_cast<underlying_type>(f1) ^ static_cast<underlying_type>(f2)); \
} \
\
constexpr flag_type operator~(flag_type f) \
{ \
    using underlying_type = std::underlying_type_t<flag_type>; \
    return flag_type(~static_cast<underlying_type>(f)); \
} \
\
constexpr flag_type& operator|=(flag_type& f1, flag_type f2) \
{ \
    f1 = f1 | f2; \
    return f1; \
} \
\
constexpr flag_type& operator&=(flag_type& f1, flag_type f2) \
{ \
    f1 = f1 & f2; \
    return f1; \
} \
\
constexpr flag_type& operator^=(flag_type& f1, flag_type f2) \
{ \
    f1 = f1 ^ f2; \
    return f1; \
}

#endif
