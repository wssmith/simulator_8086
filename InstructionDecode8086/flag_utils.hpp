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

#endif
