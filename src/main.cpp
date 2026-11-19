#include "../include/dependency_graph.hpp"
#include "../include/instruction.hpp"
#include "../include/parser.hpp"
#include "../include/scheduler.hpp"

#include <iostream>
#include <string>
#include <vector>

const bool show_instructions = true;

int main(
    int argc,
    char* argv[]
) {

    // ./sched <unidades> < bloque.iloc

    if (argc != 2) {
        std::cerr
            << "Uso:\n"
            << "  ./sched <unidades> < bloque.iloc\n";

        return 1;
    }

    int units;

    try {
        units = std::stoi(
            argv[1]
        );
    }
    catch (...) {
        std::cerr
            << "Error: numero de unidades invalido.\n";

        return 1;
    }

    if (units <= 0) {
        std::cerr
            << "Error: el numero de unidades debe "
            << "ser mayor que 0.\n";

        return 1;
    }

    // =====================================================
    // 1. PARSER
    // =====================================================

    std::vector<Instruction> instructions =
        readInstructions();

    if (instructions.empty()) {
        std::cerr
            << "Error: no se encontraron instrucciones.\n";

        return 1;
    }

    std::cout<<"Intriuscicones: "<<"\n";

    if (show_instructions) {
        for (int i = 0; i < (int)instructions.size(); ++i) {

            const Instruction& inst = instructions[i];

            std::cout << "--------------------------\n";
            std::cout << "Instruction " << i << "\n";

            std::cout << "index: " << inst.index << "\n";
            std::cout << "original: " << inst.original << "\n";
            std::cout << "opcode: " << inst.opcode << "\n";

            // READS
            std::cout << "reads: {";

            for (int j = 0; j < (int)inst.reads.size(); ++j) {
                std::cout << inst.reads[j];

                if (j < (int)inst.reads.size() - 1) {
                    std::cout << ", ";
                }
            }

            std::cout << "}\n";

            // WRITES
            std::cout << "writes: {";

            for (int j = 0; j < (int)inst.writes.size(); ++j) {
                std::cout << inst.writes[j];

                if (j < (int)inst.writes.size() - 1) {
                    std::cout << ", ";
                }
            }

            std::cout << "}\n";

            std::cout << "latency: " << inst.latency << "\n";

            std::cout << "isLoad: "
                    << (inst.isLoad ? "true" : "false")
                    << "\n";

            std::cout << "isStore: "
                    << (inst.isStore ? "true" : "false")
                    << "\n";

            std::cout << "base: "
                    << (inst.base.empty() ? "\"\"" : inst.base)
                    << "\n";

            std::cout << "offset: "
                    << (inst.offset.empty() ? "\"\"" : inst.offset)
                    << "\n";

            std::cout << "predecessors: "
                    << inst.predecessors.size()
                    << "\n";

            std::cout << "successors: "
                    << inst.successors.size()
                    << "\n";

            std::cout << "priority: "
                    << inst.priority
                    << "\n";

            std::cout << "scheduledCycle: "
                    << inst.scheduledCycle
                    << "\n";
        }

        std::cout << "--------------------------\n";
    }
    
    // =====================================================
    // 2. GRAFO DE DEPENDENCIAS
    // =====================================================

    buildDependencyGraph(
        instructions
    );

    // =====================================================
    // 3. PRIORIDADES
    // =====================================================

    computeAllPriorities(
        instructions
    );

    // =====================================================
    // 4. FORWARD LIST SCHEDULING
    // =====================================================

    std::vector<std::vector<int>> schedules =
        schedule(
            instructions,
            units
        );

    // =====================================================
    // 5. OUTPUT
    // =====================================================

    printSchedule(
        instructions,
        schedules
    );

    return 0;
}