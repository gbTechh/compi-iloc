#include <algorithm>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

enum class DepType {
    RAW,
    WAR,
    WAW,
    MEMORY
};

struct Edge {
    int node;
    int delay;
    DepType type;
};

struct Instruction {
    int index = -1;
    string original;
    string opcode;

    vector<string> reads;
    vector<string> writes;

    int latency = 1;

    bool isLoad = false;
    bool isStore = false;

    // Solo usados por loadAI/storeAI
    string base;
    string offset;

    vector<Edge> predecessors;
    vector<Edge> successors;

    int priority = -1;
    int scheduledCycle = -1;
};

// ---------------------------------------------------------
// Utilidades
// ---------------------------------------------------------

string trim(const string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");

    if (first == string::npos)
        return "";

    size_t last = s.find_last_not_of(" \t\r\n");

    return s.substr(first, last - first + 1);
}

vector<string> uniqueVector(const vector<string>& v) {
    vector<string> result;

    for (const auto& x : v) {
        if (find(result.begin(), result.end(), x) == result.end()) {
            result.push_back(x);
        }
    }

    return result;
}

int getLatency(const string& opcode) {
    if (opcode == "loadI")
        return 1;

    if (opcode == "loadAI")
        return 3;

    if (opcode == "storeAI")
        return 3;

    if (opcode == "add" || opcode == "sub")
        return 1;

    if (opcode == "mult")
        return 2;

    if (opcode == "lshift" || opcode == "rshift")
        return 1;

    cerr << "Opcode desconocido: " << opcode << '\n';
    exit(1);
}

// ---------------------------------------------------------
// Parser
// ---------------------------------------------------------

Instruction parseInstruction(const string& line, int index) {
    Instruction inst;

    inst.index = index;
    inst.original = trim(line);

    smatch match;

    // -----------------------------------------------------
    // loadI 5 => r1
    // -----------------------------------------------------

    regex loadIRegex(
        R"(^loadI\s+(-?[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (regex_match(inst.original, match, loadIRegex)) {
        inst.opcode = "loadI";
        inst.latency = getLatency(inst.opcode);

        inst.writes.push_back(match[2]);

        return inst;
    }

    // -----------------------------------------------------
    // loadAI r0,@x => r1
    //
    // También permite offsets numéricos:
    // loadAI r0,4 => r1
    // -----------------------------------------------------

    regex loadAIRegex(
        R"(^loadAI\s+(r[0-9]+)\s*,\s*(@?[A-Za-z_][A-Za-z0-9_]*|-?[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (regex_match(inst.original, match, loadAIRegex)) {
        inst.opcode = "loadAI";
        inst.latency = getLatency(inst.opcode);

        inst.isLoad = true;

        inst.base = match[1];
        inst.offset = match[2];

        // Lee el registro base.
        inst.reads.push_back(inst.base);

        // Escribe el registro destino.
        inst.writes.push_back(match[3]);

        return inst;
    }

    // -----------------------------------------------------
    // storeAI r1 => r0,@x
    // -----------------------------------------------------

    regex storeAIRegex(
        R"(^storeAI\s+(r[0-9]+)\s*=>\s*(r[0-9]+)\s*,\s*(@?[A-Za-z_][A-Za-z0-9_]*|-?[0-9]+)\s*$)"
    );

    if (regex_match(inst.original, match, storeAIRegex)) {
        inst.opcode = "storeAI";
        inst.latency = getLatency(inst.opcode);

        inst.isStore = true;

        // storeAI lee el valor que va a guardar.
        inst.reads.push_back(match[1]);

        // Y también lee el registro base.
        inst.base = match[2];
        inst.reads.push_back(inst.base);

        inst.offset = match[3];

        return inst;
    }

    // -----------------------------------------------------
    // add r1,r2 => r3
    // sub r1,r2 => r3
    // mult r1,r2 => r3
    // lshift r1,r2 => r3
    // rshift r1,r2 => r3
    // -----------------------------------------------------

    regex binaryRegex(
        R"(^(add|sub|mult|lshift|rshift)\s+(r[0-9]+)\s*,\s*(r[0-9]+)\s*=>\s*(r[0-9]+)\s*$)"
    );

    if (regex_match(inst.original, match, binaryRegex)) {
        inst.opcode = match[1];
        inst.latency = getLatency(inst.opcode);

        inst.reads.push_back(match[2]);
        inst.reads.push_back(match[3]);

        inst.writes.push_back(match[4]);

        inst.reads = uniqueVector(inst.reads);
        inst.writes = uniqueVector(inst.writes);

        return inst;
    }

    cerr << "No se pudo parsear la instruccion:\n";
    cerr << "  " << line << '\n';

    exit(1);
}

// ---------------------------------------------------------
// Aristas
// ---------------------------------------------------------

void addEdge(
    vector<Instruction>& instructions,
    int from,
    int to,
    int delay,
    DepType type
) {
    if (from == to)
        return;

    /*
     * Puede ocurrir que dos instrucciones tengan más de una
     * razón para depender entre sí.
     *
     * Para scheduling lo importante es satisfacer la
     * restricción más fuerte:
     *
     * S(to) >= S(from) + max(delay)
     */

    for (auto& edge : instructions[from].successors) {
        if (edge.node == to) {
            if (delay > edge.delay) {
                edge.delay = delay;
                edge.type = type;

                // Actualizar también predecessor.
                for (auto& pred : instructions[to].predecessors) {
                    if (pred.node == from) {
                        pred.delay = delay;
                        pred.type = type;
                        break;
                    }
                }
            }

            return;
        }
    }

    instructions[from].successors.push_back(
        {to, delay, type}
    );

    instructions[to].predecessors.push_back(
        {from, delay, type}
    );
}

// ---------------------------------------------------------
// Conflictos de memoria
// ---------------------------------------------------------

bool memoryConflict(
    const Instruction& a,
    const Instruction& b
) {
    /*
     * load-load nunca genera conflicto.
     */

    if (a.isLoad && b.isLoad)
        return false;

    /*
     * Misma base:
     *
     * mismo offset     => conflicto
     * diferente offset => no conflicto
     */

    if (a.base == b.base) {
        return a.offset == b.offset;
    }

    /*
     * Bases diferentes:
     *
     * El laboratorio exige asumir conflicto.
     */

    return true;
}

// ---------------------------------------------------------
// Grafo de dependencias
// ---------------------------------------------------------

void buildDependencyGraph(vector<Instruction>& instructions) {
    /*
     * Última instrucción que escribió cada registro.
     */
    unordered_map<string, int> lastWriter;

    /*
     * Lectores ocurridos desde la última escritura.
     */
    unordered_map<string, vector<int>> readersSinceWrite;

    /*
     * Lista de instrucciones de memoria anteriores.
     */
    vector<int> previousMemory;

    for (int j = 0; j < (int)instructions.size(); ++j) {
        Instruction& current = instructions[j];

        current.reads = uniqueVector(current.reads);
        current.writes = uniqueVector(current.writes);

        // =================================================
        // 1. RAW
        //
        // writer anterior -> reader actual
        // =================================================

        for (const string& reg : current.reads) {
            auto it = lastWriter.find(reg);

            if (it != lastWriter.end()) {
                int writer = it->second;

                addEdge(
                    instructions,
                    writer,
                    j,
                    instructions[writer].latency,
                    DepType::RAW
                );
            }
        }

        // =================================================
        // 2. WAR y WAW
        // =================================================

        for (const string& reg : current.writes) {

            // ---------------------------------------------
            // WAR
            //
            // lectores anteriores -> writer actual
            // delay = 1
            // ---------------------------------------------

            auto readersIt = readersSinceWrite.find(reg);

            if (readersIt != readersSinceWrite.end()) {
                for (int reader : readersIt->second) {
                    addEdge(
                        instructions,
                        reader,
                        j,
                        1,
                        DepType::WAR
                    );
                }
            }

            // ---------------------------------------------
            // WAW
            //
            // writer anterior -> writer actual
            // delay = lat(writer)
            // ---------------------------------------------

            auto writerIt = lastWriter.find(reg);

            if (writerIt != lastWriter.end()) {
                int writer = writerIt->second;

                addEdge(
                    instructions,
                    writer,
                    j,
                    instructions[writer].latency,
                    DepType::WAW
                );
            }
        }

        // =================================================
        // Actualizamos información de escrituras.
        // =================================================

        for (const string& reg : current.writes) {
            lastWriter[reg] = j;

            /*
             * Una nueva escritura comienza una nueva
             * "versión" del registro.
             */
            readersSinceWrite[reg].clear();
        }

        // =================================================
        // Registramos las lecturas actuales.
        //
        // Si la misma instrucción también escribe ese
        // registro, no necesitamos agregarla como lector
        // posterior a su propia escritura.
        // =================================================

        for (const string& reg : current.reads) {
            bool alsoWrites =
                find(
                    current.writes.begin(),
                    current.writes.end(),
                    reg
                ) != current.writes.end();

            if (!alsoWrites) {
                readersSinceWrite[reg].push_back(j);
            }
        }

        // =================================================
        // 3. Dependencias de memoria
        // =================================================

        if (current.isLoad || current.isStore) {

            for (int i : previousMemory) {
                Instruction& previous = instructions[i];

                if (!memoryConflict(previous, current))
                    continue;

                /*
                 * load -> load
                 *
                 * memoryConflict ya devuelve false.
                 */

                // -----------------------------------------
                // store -> load
                // -----------------------------------------

                if (previous.isStore && current.isLoad) {
                    addEdge(
                        instructions,
                        i,
                        j,
                        previous.latency,
                        DepType::MEMORY
                    );
                }

                // -----------------------------------------
                // load -> store
                // -----------------------------------------

                else if (previous.isLoad && current.isStore) {
                    addEdge(
                        instructions,
                        i,
                        j,
                        1,
                        DepType::MEMORY
                    );
                }

                // -----------------------------------------
                // store -> store
                //
                // Conservamos el orden de escrituras.
                // -----------------------------------------

                else if (previous.isStore && current.isStore) {
                    addEdge(
                        instructions,
                        i,
                        j,
                        previous.latency,
                        DepType::MEMORY
                    );
                }
            }

            previousMemory.push_back(j);
        }
    }
}

// ---------------------------------------------------------
// Prioridad
// ---------------------------------------------------------

int computePriority(
    int node,
    vector<Instruction>& instructions
) {
    if (instructions[node].priority != -1)
        return instructions[node].priority;

    int maxSuccessorPriority = 0;

    for (const Edge& edge : instructions[node].successors) {
        maxSuccessorPriority = max(
            maxSuccessorPriority,
            computePriority(edge.node, instructions)
        );
    }

    instructions[node].priority =
        instructions[node].latency +
        maxSuccessorPriority;

    return instructions[node].priority;
}

void computeAllPriorities(vector<Instruction>& instructions) {
    for (int i = 0; i < (int)instructions.size(); ++i) {
        computePriority(i, instructions);
    }
}

// ---------------------------------------------------------
// Ready
// ---------------------------------------------------------

bool isReady(
    int node,
    int cycle,
    const vector<Instruction>& instructions
) {
    if (instructions[node].scheduledCycle != -1)
        return false;

    for (const Edge& predEdge :
         instructions[node].predecessors) {

        const Instruction& pred =
            instructions[predEdge.node];

        /*
         * El predecesor todavía ni siquiera fue programado.
         */
        if (pred.scheduledCycle == -1)
            return false;

        /*
         * Debemos cumplir:
         *
         * S(pred) + delay <= cycle
         */
        if (
            pred.scheduledCycle + predEdge.delay > cycle
        ) {
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------
// Forward List Scheduling
// ---------------------------------------------------------

vector<vector<int>> schedule(
    vector<Instruction>& instructions,
    int units
) {
    int n = instructions.size();

    int scheduledCount = 0;
    int cycle = 1;

    /*
     * schedules[ciclo] contiene los índices de instrucciones
     * programadas en ese ciclo.
     *
     * Dejamos posición 0 sin usar.
     */
    vector<vector<int>> schedules(1);

    while (scheduledCount < n) {

        vector<int> ready;

        // -------------------------------------------------
        // Buscar instrucciones disponibles.
        // -------------------------------------------------

        for (int i = 0; i < n; ++i) {
            if (isReady(i, cycle, instructions)) {
                ready.push_back(i);
            }
        }

        // -------------------------------------------------
        // Prioridad más alta primero.
        //
        // Empate:
        // instrucción que apareció primero.
        // -------------------------------------------------

        sort(
            ready.begin(),
            ready.end(),
            [&](int a, int b) {

                if (
                    instructions[a].priority !=
                    instructions[b].priority
                ) {
                    return instructions[a].priority >
                           instructions[b].priority;
                }

                return instructions[a].index <
                       instructions[b].index;
            }
        );

        vector<int> selected;

        int count = min(
            units,
            (int)ready.size()
        );

        // -------------------------------------------------
        // Seleccionar como máximo <unidades>.
        // -------------------------------------------------

        for (int k = 0; k < count; ++k) {
            int node = ready[k];

            instructions[node].scheduledCycle = cycle;

            selected.push_back(node);

            scheduledCount++;
        }

        schedules.push_back(selected);

        cycle++;
    }

    return schedules;
}

// ---------------------------------------------------------
// Longitud final
// ---------------------------------------------------------

int scheduleLength(
    const vector<Instruction>& instructions
) {
    int maximum = 0;

    for (const auto& inst : instructions) {
        maximum = max(
            maximum,
            inst.scheduledCycle + inst.latency
        );
    }

    /*
     * Definición del laboratorio:
     *
     * max(S(n) + lat(n)) - 1
     */
    return maximum - 1;
}

// ---------------------------------------------------------
// Imprimir
// ---------------------------------------------------------

void printSchedule(
    const vector<Instruction>& instructions,
    const vector<vector<int>>& schedules
) {
    int length = scheduleLength(instructions);

    for (int cycle = 1; cycle <= length; ++cycle) {

        cout << cycle << ": ";

        if (
            cycle >= (int)schedules.size() ||
            schedules[cycle].empty()
        ) {
            cout << "nop";
        }
        else {
            for (
                int k = 0;
                k < (int)schedules[cycle].size();
                ++k
            ) {
                if (k > 0)
                    cout << " ; ";

                int node = schedules[cycle][k];

                cout << instructions[node].original;
            }
        }

        cout << '\n';
    }

    cout << "length: "
         << length
         << " cycles\n";
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cerr << "Uso:\n";
        cerr << "  ./sched <unidades> < bloque.iloc\n";

        return 1;
    }

    int units;

    try {
        units = stoi(argv[1]);
    }
    catch (...) {
        cerr << "Numero de unidades invalido.\n";
        return 1;
    }

    if (units <= 0) {
        cerr << "El numero de unidades debe ser mayor que 0.\n";
        return 1;
    }

    vector<Instruction> instructions;

    string line;

    int index = 0;

    // -----------------------------------------------------
    // Leer ILOC desde stdin
    // -----------------------------------------------------

    while (getline(cin, line)) {
        line = trim(line);

        if (line.empty())
            continue;

        /*
         * Ignorar comentarios completos.
         */
        if (line[0] == '#')
            continue;

        instructions.push_back(
            parseInstruction(line, index)
        );

        index++;
    }

    if (instructions.empty()) {
        cerr << "No se encontraron instrucciones.\n";
        return 1;
    }

    // -----------------------------------------------------
    // Pipeline completo
    // -----------------------------------------------------

    buildDependencyGraph(instructions);

    computeAllPriorities(instructions);

    vector<vector<int>> schedules =
        schedule(instructions, units);

    printSchedule(
        instructions,
        schedules
    );

    return 0;
}