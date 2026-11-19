#include "../include/utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>

std::string trim(const std::string& text) {
    const std::size_t first =
        text.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const std::size_t last =
        text.find_last_not_of(" \t\r\n");

    return text.substr(
        first,
        last - first + 1
    );
}

std::vector<std::string> uniqueVector(
    const std::vector<std::string>& values
) {
    std::vector<std::string> result;

    for (const std::string& value : values) {
        if (
            std::find(
                result.begin(),
                result.end(),
                value
            ) == result.end()
        ) {
            result.push_back(value);
        }
    }

    return result;
}

int getLatency(const std::string& opcode) {
    if (opcode == "loadI") {
        return 1;
    }

    if (opcode == "loadAI") {
        return 3;
    }

    if (opcode == "storeAI") {
        return 3;
    }

    if (opcode == "add" || opcode == "sub") {
        return 1;
    }

    if (opcode == "mult") {
        return 2;
    }

    if (
        opcode == "lshift" ||
        opcode == "rshift"
    ) {
        return 1;
    }

    std::cerr
        << "Error: opcode desconocido: "
        << opcode
        << '\n';

    std::exit(EXIT_FAILURE);
}