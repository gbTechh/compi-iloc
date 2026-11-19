#include <vector>
#include <string>


using namespace std;

struct Instruction {
    int id;

    vector<string> reads;
    vector<string> writes;

    int latency;
};