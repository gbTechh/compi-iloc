#pragma once

#include "instruction.hpp"

#include <vector>

void buildDependencyGraph(
    std::vector<Instruction>& instructions
);

void computeAllPriorities(
    std::vector<Instruction>& instructions
);