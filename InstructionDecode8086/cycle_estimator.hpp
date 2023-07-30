#ifndef WS_CYCLEESTIMATOR_HPP
#define WS_CYCLEESTIMATOR_HPP

#include <cstdint>

struct instruction;

struct cycle_estimate
{
    int32_t base{};
    int16_t ea{};
};

cycle_estimate estimate_cycles(const instruction& inst);

#endif
