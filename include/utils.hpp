#pragma once

#include <string>
#include <vector>

std::string trim(const std::string& text);

std::vector<std::string> uniqueVector(
    const std::vector<std::string>& values
);

int getLatency(const std::string& opcode);