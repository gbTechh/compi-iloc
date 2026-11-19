#pragma once

#include "instruction.hpp"

#include <vector>

std::vector<std::vector<int>> schedule(
    std::vector<Instruction>& instructions,
    int units
);

int scheduleLength(
    const std::vector<Instruction>& instructions
);

void printSchedule(
    const std::vector<Instruction>& instructions,
    const std::vector<std::vector<int>>& schedules
);