#pragma once

#include "instruction.hpp"

#include <string>
#include <vector>

Instruction parseInstruction(
    const std::string& line,
    int index
);

std::vector<Instruction> readInstructions();