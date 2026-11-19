#include "../include/parser.hpp"
#include "../include/utils.hpp"

#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

Instruction parseInstruction(
    const std::string& line,
    int index
) {
    Instruction instruction;

    instruction.index = index;
    instruction.original = trim(line);

    std::smatch match;


    const std::regex loadIRegex(
        R"(^loadI\s+(-?[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (
        std::regex_match(
            instruction.original,
            match,
            loadIRegex
        )
    ) {
        instruction.opcode = "loadI";
        instruction.latency =
            getLatency(instruction.opcode);

        // loadI escribe el registro destino.
        instruction.writes.push_back(
            match[2].str()
        );

        return instruction;
    }


    const std::regex loadAIRegex(
        R"(^loadAI\s+(r[0-9]+)\s*,\s*(@?[A-Za-z_][A-Za-z0-9_]*|-?[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (
        std::regex_match(
            instruction.original,
            match,
            loadAIRegex
        )
    ) {
        instruction.opcode = "loadAI";
        instruction.latency =
            getLatency(instruction.opcode);

        instruction.isLoad = true;

        instruction.base = match[1].str();
        instruction.offset = match[2].str();

        // loadAI utiliza el registro base.
        instruction.reads.push_back(
            instruction.base
        );

        // Y escribe el resultado.
        instruction.writes.push_back(
            match[3].str()
        );

        return instruction;
    }


    const std::regex storeAIRegex(
        R"(^storeAI\s+(r[0-9]+)\s*=>\s*(r[0-9]+)\s*,\s*(@?[A-Za-z_][A-Za-z0-9_]*|-?[0-9]+)\s*$)"
    );

    if (
        std::regex_match(
            instruction.original,
            match,
            storeAIRegex
        )
    ) {
        instruction.opcode = "storeAI";
        instruction.latency =
            getLatency(instruction.opcode);

        instruction.isStore = true;

        // Valor que queremos almacenar.
        instruction.reads.push_back(
            match[1].str()
        );

        // Registro base.
        instruction.base = match[2].str();

        instruction.reads.push_back(
            instruction.base
        );

        instruction.offset = match[3].str();

        instruction.reads =
            uniqueVector(instruction.reads);

        return instruction;
    }

    const std::regex binaryRegex(
        R"(^(add|sub|mult|lshift|rshift)\s+(r[0-9]+)\s*,\s*(r[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (
        std::regex_match(
            instruction.original,
            match,
            binaryRegex
        )
    ) {
        instruction.opcode =
            match[1].str();

        instruction.latency =
            getLatency(instruction.opcode);

        instruction.reads.push_back(
            match[2].str()
        );

        instruction.reads.push_back(
            match[3].str()
        );

        instruction.writes.push_back(
            match[4].str()
        );

        instruction.reads =
            uniqueVector(instruction.reads);

        instruction.writes =
            uniqueVector(instruction.writes);

        return instruction;
    }

    std::cerr
        << "Error: no se pudo parsear:\n"
        << "    "
        << instruction.original
        << '\n';

    std::exit(EXIT_FAILURE);
}

std::vector<Instruction> readInstructions() {
    std::vector<Instruction> instructions;

    std::string line;

    int index = 0;

    while (std::getline(std::cin, line)) {
        line = trim(line);

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        instructions.push_back(
            parseInstruction(
                line,
                index
            )
        );

        ++index;
    }

    return instructions;
}