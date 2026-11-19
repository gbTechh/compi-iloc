#include "../include/dependency_graph.hpp"
#include "../include/utils.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// =========================================================
// Añadir una arista.
//
// from ----delay----> to
//
// Si ya existe una arista entre los mismos nodos,
// conservamos la restricción con mayor delay.
// =========================================================

void addEdge(
    std::vector<Instruction>& instructions,
    int from,
    int to,
    int delay,
    DepType type
) {
    if (from == to) {
        return;
    }

    // Comprobar si ya existe.
    for (
        Edge& edge :
        instructions[from].successors
    ) {
        if (edge.node == to) {

            if (delay > edge.delay) {
                edge.delay = delay;
                edge.type = type;

                // Actualizar también la copia que está
                // almacenada como predecessor.
                for (
                    Edge& predecessor :
                    instructions[to].predecessors
                ) {
                    if (
                        predecessor.node == from
                    ) {
                        predecessor.delay = delay;
                        predecessor.type = type;
                        break;
                    }
                }
            }

            return;
        }
    }

    // Nueva arista.
    instructions[from].successors.push_back(
        Edge{
            to,
            delay,
            type
        }
    );

    instructions[to].predecessors.push_back(
        Edge{
            from,
            delay,
            type
        }
    );
}

// =========================================================
// Determinar si dos accesos a memoria pueden entrar
// en conflicto.
//
// Reglas del laboratorio:
//
// load-load:
//     nunca conflicto.
//
// misma base:
//     mismo offset     -> conflicto
//     distinto offset  -> no conflicto
//
// bases diferentes:
//     asumir conflicto.
// =========================================================

bool memoryConflict(
    const Instruction& first,
    const Instruction& second
) {
    // Dos lecturas no interfieren.
    if (
        first.isLoad &&
        second.isLoad
    ) {
        return false;
    }

    // Misma base.
    if (first.base == second.base) {
        return first.offset == second.offset;
    }

    // Bases distintas:
    // asumir posible aliasing.
    return true;
}

// =========================================================
// Prioridad recursiva:
//
// priority(n) =
//     latency(n) +
//     max(priority(successor))
// =========================================================

int computePriority(
    int node,
    std::vector<Instruction>& instructions
) {
    Instruction& instruction =
        instructions[node];

    // Memoization.
    if (instruction.priority != -1) {
        return instruction.priority;
    }

    int maximumSuccessorPriority = 0;

    for (
        const Edge& edge :
        instruction.successors
    ) {
        maximumSuccessorPriority =
            std::max(
                maximumSuccessorPriority,
                computePriority(
                    edge.node,
                    instructions
                )
            );
    }

    instruction.priority =
        instruction.latency +
        maximumSuccessorPriority;

    return instruction.priority;
}

} // namespace

// =========================================================
// Construcción del grafo.
// =========================================================

void buildDependencyGraph(
    std::vector<Instruction>& instructions
) {
    /*
     * Para cada registro:
     *
     * lastWriter[r]
     *
     * nos dice cuál fue la última instrucción que
     * escribió r.
     */
    std::unordered_map<std::string, int>
        lastWriter;

    /*
     * Para cada registro guardamos los lectores que han
     * aparecido desde la última escritura.
     *
     * Esto nos permite crear WAR.
     */
    std::unordered_map<
        std::string,
        std::vector<int>
    > readersSinceWrite;

    /*
     * Instrucciones anteriores que accedieron a memoria.
     */
    std::vector<int> previousMemory;

    for (
        int currentIndex = 0;
        currentIndex <
            static_cast<int>(instructions.size());
        ++currentIndex
    ) {
        Instruction& current =
            instructions[currentIndex];

        current.reads =
            uniqueVector(current.reads);

        current.writes =
            uniqueVector(current.writes);

        // =================================================
        // RAW
        //
        // Una instrucción actual lee un registro producido
        // por una instrucción anterior.
        //
        // WRITE -> READ
        // =================================================

        for (
            const std::string& reg :
            current.reads
        ) {
            auto writer =
                lastWriter.find(reg);

            if (writer == lastWriter.end()) {
                continue;
            }

            const int writerIndex =
                writer->second;

            /*
             * RAW usa como delay la latencia del productor.
             */
            addEdge(
                instructions,
                writerIndex,
                currentIndex,
                instructions[writerIndex].latency,
                DepType::RAW
            );
        }

        // =================================================
        // WAR y WAW
        //
        // Las dos aparecen cuando la instrucción actual
        // escribe un registro.
        // =================================================

        for (
            const std::string& reg :
            current.writes
        ) {
            // =============================================
            // WAR
            //
            // READ -> WRITE
            //
            // Los lectores anteriores deben leer antes de
            // que current sobrescriba el registro.
            //
            // delay = 1
            // =============================================

            auto readers =
                readersSinceWrite.find(reg);

            if (
                readers !=
                readersSinceWrite.end()
            ) {
                for (
                    int readerIndex :
                    readers->second
                ) {
                    addEdge(
                        instructions,
                        readerIndex,
                        currentIndex,
                        1,
                        DepType::WAR
                    );
                }
            }

            // =============================================
            // WAW
            //
            // WRITE -> WRITE
            //
            // Debemos conservar el orden de escrituras.
            //
            // delay = latencia del writer anterior.
            // =============================================

            auto previousWriter =
                lastWriter.find(reg);

            if (
                previousWriter !=
                lastWriter.end()
            ) {
                const int writerIndex =
                    previousWriter->second;

                addEdge(
                    instructions,
                    writerIndex,
                    currentIndex,
                    instructions[writerIndex].latency,
                    DepType::WAW
                );
            }
        }

        // =================================================
        // Ahora la instrucción actual se convierte en el
        // último writer de sus registros destino.
        // =================================================

        for (
            const std::string& reg :
            current.writes
        ) {
            lastWriter[reg] =
                currentIndex;

            /*
             * Una nueva escritura comienza una nueva
             * "versión" del registro.
             */
            readersSinceWrite[reg].clear();
        }

        // =================================================
        // Registrar las lecturas actuales.
        //
        // Si una instrucción lee Y escribe el mismo
        // registro:
        //
        // add r1,r1 => r1
        //
        // no registramos la lectura después de haber
        // actualizado lastWriter.
        // =================================================

        for (
            const std::string& reg :
            current.reads
        ) {
            const bool alsoWrites =
                std::find(
                    current.writes.begin(),
                    current.writes.end(),
                    reg
                ) != current.writes.end();

            if (!alsoWrites) {
                readersSinceWrite[reg]
                    .push_back(currentIndex);
            }
        }

        // =================================================
        // DEPENDENCIAS DE MEMORIA
        // =================================================

        if (
            current.isLoad ||
            current.isStore
        ) {
            for (
                int previousIndex :
                previousMemory
            ) {
                const Instruction& previous =
                    instructions[previousIndex];

                if (
                    !memoryConflict(
                        previous,
                        current
                    )
                ) {
                    continue;
                }

                // =========================================
                // STORE -> LOAD
                //
                // delay = lat(store)
                // =========================================

                if (
                    previous.isStore &&
                    current.isLoad
                ) {
                    addEdge(
                        instructions,
                        previousIndex,
                        currentIndex,
                        previous.latency,
                        DepType::MEMORY
                    );
                }

                // =========================================
                // LOAD -> STORE
                //
                // delay = 1
                // =========================================

                else if (
                    previous.isLoad &&
                    current.isStore
                ) {
                    addEdge(
                        instructions,
                        previousIndex,
                        currentIndex,
                        1,
                        DepType::MEMORY
                    );
                }

                // =========================================
                // STORE -> STORE
                //
                // Debemos conservar el orden de las
                // escrituras sobre memoria conflictiva.
                // =========================================

                else if (
                    previous.isStore &&
                    current.isStore
                ) {
                    addEdge(
                        instructions,
                        previousIndex,
                        currentIndex,
                        previous.latency,
                        DepType::MEMORY
                    );
                }
            }

            previousMemory.push_back(
                currentIndex
            );
        }
    }
}

void computeAllPriorities(
    std::vector<Instruction>& instructions
) {
    for (
        int i = 0;
        i < static_cast<int>(instructions.size());
        ++i
    ) {
        computePriority(
            i,
            instructions
        );
    }
}