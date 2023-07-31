#ifndef WS_CYCLEESTIMATOR_HPP
#define WS_CYCLEESTIMATOR_HPP

#include <cstdint>

struct instruction;

struct cycle_interval
{
    int32_t min{};
    int32_t max{};
};

struct cycle_estimate
{
    cycle_interval base{};
    int32_t transfers{};
    int32_t ea{};
};

cycle_estimate estimate_cycles(const instruction& inst);

#endif
