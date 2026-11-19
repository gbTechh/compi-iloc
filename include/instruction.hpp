#pragma once

#include <string>
#include <vector>

enum class DepType {
    RAW,
    WAR,
    WAW,
    MEMORY
};

struct Edge {
    // Índice de la otra instrucción.
    int node;

    // Cantidad de ciclos que debe respetarse.
    int delay;

    DepType type;
};

struct Instruction {
    // Posición original del bloque.
    int index = -1;

    // Texto original para poder imprimirlo después.
    std::string original;

    std::string opcode;

    // Registros lectura
    std::vector<std::string> reads;

    // Registros escritura
    std::vector<std::string> writes;

    int latency = 1;

    bool isLoad = false;
    bool isStore = false;

    // Para loadAI/storeAI.
    std::string base;
    std::string offset;

    // Grafo.
    std::vector<Edge> predecessors;
    std::vector<Edge> successors;

    // Scheduling.
    int priority = -1;
    int scheduledCycle = -1;
};