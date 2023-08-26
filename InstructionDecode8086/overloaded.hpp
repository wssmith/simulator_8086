#ifndef WS_OVERLOADED_HPP
#define WS_OVERLOADED_HPP

template<typename... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif
