#include "../include/scheduler.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

namespace {

// S(pred) + delay <= cycle

bool isReady(
    int node,
    int cycle,
    const std::vector<Instruction>& instructions
) {
    const Instruction& instruction =
        instructions[node];

    // Ya fue programada.
    if (instruction.scheduledCycle != -1) {
        return false;
    }

    for (
        const Edge& edge :
        instruction.predecessors
    ) {
        const Instruction& predecessor =
            instructions[edge.node];

        // El predecesor todavía no ha sido programado.
        if (
            predecessor.scheduledCycle == -1
        ) {
            return false;
        }

        // S(pred) + delay <= cycle
        if (
            predecessor.scheduledCycle +
            edge.delay >
            cycle
        ) {
            return false;
        }
    }

    return true;
}

} // namespace

std::vector<std::vector<int>> schedule(
    std::vector<Instruction>& instructions,
    int units
) {
    const int instructionCount =
        static_cast<int>(
            instructions.size()
        );

    int scheduledCount = 0;

    // El laboratorio empieza en ciclo 1.
    int cycle = 1;

    
    std::vector<std::vector<int>> schedules(1);

    while (
        scheduledCount <
        instructionCount
    ) {
        std::vector<int> ready;

        // intruscciones ready
        for (
            int i = 0;
            i < instructionCount;
            ++i
        ) {
            if (
                isReady(
                    i,
                    cycle,
                    instructions
                )
            ) {
                ready.push_back(i);
            }
        }

        // =================================================
        // Ordenar Ready.
        //
        // 1. Mayor prioridad primero.
        // 2. Empate -> instrucción original más temprana.
        // =================================================

        std::sort(
            ready.begin(),
            ready.end(),

            [&](int a, int b) {
                if (
                    instructions[a].priority !=
                    instructions[b].priority
                ) {
                    return
                        instructions[a].priority >
                        instructions[b].priority;
                }

                return
                    instructions[a].index <
                    instructions[b].index;
            }
        );

        // =================================================
        // Podemos ejecutar como máximo <units>
        // instrucciones en este ciclo.
        // =================================================

        const int amount =
            std::min(
                units,
                static_cast<int>(
                    ready.size()
                )
            );

        std::vector<int> selected;

        for (
            int i = 0;
            i < amount;
            ++i
        ) {
            const int node = ready[i];

            instructions[node].scheduledCycle =
                cycle;

            selected.push_back(node);

            ++scheduledCount;
        }

        schedules.push_back(
            selected
        );

        ++cycle;
    }

    return schedules;
}

int scheduleLength(
    const std::vector<Instruction>& instructions
) {
    int maximum = 0;

    for (
        const Instruction& instruction :
        instructions
    ) {
        maximum =
            std::max(
                maximum,
                instruction.scheduledCycle +
                instruction.latency
            );
    }

    /*
     * Definición del lab:
     *
     * max(S(n) + lat(n)) - 1
     */
    return maximum - 1;
}

void printSchedule(
    const std::vector<Instruction>& instructions,
    const std::vector<std::vector<int>>& schedules
) {
    const int length =
        scheduleLength(instructions);

    for (
        int cycle = 1;
        cycle <= length;
        ++cycle
    ) {
        std::cout
            << cycle
            << ": ";

        if (
            cycle >=
                static_cast<int>(
                    schedules.size()
                ) ||
            schedules[cycle].empty()
        ) {
            std::cout << "nop";
        }
        else {
            for (
                int i = 0;
                i <
                    static_cast<int>(
                        schedules[cycle].size()
                    );
                ++i
            ) {
                if (i != 0) {
                    std::cout << " ; ";
                }

                const int instructionIndex =
                    schedules[cycle][i];

                std::cout
                    << instructions[
                           instructionIndex
                       ].original;
            }
        }

        std::cout << '\n';
    }

    std::cout
        << "length: "
        << length
        << " cycles\n";
}