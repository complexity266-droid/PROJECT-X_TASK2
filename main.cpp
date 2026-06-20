#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iomanip>
#include <string>
#include <climits>

using namespace std;

static const int L1_CAP     = 32;
static const int L2_CAP     = 128;
static const int L3_CAP     = 512;
static const int L1_LATENCY = 4;
static const int L2_LATENCY = 12;
static const int L3_LATENCY = 40;
static const int RAM_LATENCY= 200;
static const int NUM_CORES  = 2;
static const int TIME_QUANTUM= 3;  

struct Task {
    string id;
    int burst;                    
    int remaining;                
    vector<string> mem; 
    int memIdx = 0;               
    int arrivalCycle = 0;
    int startCycle   = -1;
    int finishCycle  = -1;
};

class CacheLevel {
public:
    int capacity;
    int latency;
    string name;
    deque<string> slots;
    bool useLRU;

    CacheLevel(const string& n, int cap, int lat, bool lru = true)
        : capacity(cap), latency(lat), name(n), useLRU(lru) {}

    bool contains(const string& block) const {
        for (auto& s : slots) if (s == block) return true;
        return false;
    }

    void touch(const string& block) {
        if (useLRU) {
            auto it = find(slots.begin(), slots.end(), block);
            if (it != slots.end()) {
                slots.erase(it);
                slots.push_back(block);
            }
        }
    }

    string insert(const string& block) {
        string evicted;
        if ((int)slots.size() >= capacity) {
            evicted = slots.front();
            slots.pop_front();
        }
        slots.push_back(block);
        return evicted;
    }

    void remove(const string& block) {
        auto it = find(slots.begin(), slots.end(), block);
        if (it != slots.end()) slots.erase(it);
    }

    string dump() const {
        string out = name + ": [";
        for (int i = 0; i < (int)slots.size(); ++i) {
            out += slots[i];
            if (i + 1 < (int)slots.size()) out += ", ";
        }
        out += "]";
        return out;
    }
};

class CacheHierarchy {
public:
    CacheLevel l1, l2, l3;
    int ramAccesses = 0;
    int totalLatency = 0;
    int hits[3]   = {0, 0, 0};
    int misses[3] = {0, 0, 0};

    CacheHierarchy()
        : l1("L1", L1_CAP,  L1_LATENCY,  true),
          l2("L2", L2_CAP,  L2_LATENCY,  true),
          l3("L3", L3_CAP,  L3_LATENCY,  true) {}

    int access(const string& block, ostream& log) {
        int lat = 0;

        if (l1.contains(block)) {
            l1.touch(block);
            hits[0]++;
            lat = L1_LATENCY;
            log << "    " << l1.dump() << " -> HIT (" << lat << " cycles)\n";
            log << "    " << l2.dump() << "\n";
            log << "    " << l3.dump() << "\n";
            totalLatency += lat;
            return lat;
        }
        misses[0]++;
        log << "    " << l1.dump() << " >> MISS\n";

        if (l2.contains(block)) {
            l2.touch(block);
            hits[1]++;
            lat = L2_LATENCY;
            log << "    " << l2.dump() << " >> HIT (" << lat << " cycles)\n";
            log << "    " << l3.dump() << "\n";
            log << "    Promoting " << block << " -> L1\n";
            string evL1 = l1.insert(block);
            if (!evL1.empty()) {
                log << "    L1 full: evicting " << evL1 << " -> L2\n";
                string evL2 = l2.insert(evL1);
                l2.remove(block);      
                if (!evL2.empty())
                    log << "    L2 full: evicting " << evL2 << " -> L3\n";
            } else {
                l2.remove(block);
            }
            log << "    " << l1.dump() << "\n";
            log << "    " << l2.dump() << "\n";
            totalLatency += lat;
            return lat;
        }
        misses[1]++;
        log << "    " << l2.dump() << " >> MISS\n";

        if (l3.contains(block)) {
            l3.touch(block);
            hits[2]++;
            lat = L3_LATENCY;
            log << "    " << l3.dump() << " >> HIT (" << lat << " cycles)\n";
            log << "    Promoting " << block << " -> L1\n";
            string evL1 = l1.insert(block);
            if (!evL1.empty()) {
                log << "    L1 full: evicting " << evL1 << " -> L2\n";
                string evL2 = l2.insert(evL1);
                if (!evL2.empty()) {
                    log << "    L2 full: evicting " << evL2 << " -> L3\n";
                    string evL3 = l3.insert(evL2);
                    l3.remove(block);
                    if (!evL3.empty())
                        log << "    L3 full: evicting " << evL3 << " (discarded)\n";
                } else {
                    l3.remove(block);
                }
            } else {
                l3.remove(block);
            }
            log << "    " << l1.dump() << "\n";
            log << "    " << l2.dump() << "\n";
            log << "    " << l3.dump() << "\n";
            totalLatency += lat;
            return lat;
        }
        misses[2]++;
        log << "    " << l3.dump() << " >> MISS\n";

        ramAccesses++;
        lat = RAM_LATENCY;
        log << "    Fetching " << block << " from RAM (" << lat << " cycles)\n";
        string evL1 = l1.insert(block);
        if (!evL1.empty()) {
            log << "    L1 full: evicting " << evL1 << " -> L2\n";
            string evL2 = l2.insert(evL1);
            if (!evL2.empty()) {
                log << "    L2 full: evicting " << evL2 << " -> L3\n";
                string evL3 = l3.insert(evL2);
                if (!evL3.empty())
                    log << "    L3 full: evicting " << evL3 << " (discarded)\n";
            }
        }
        log << "    " << l1.dump() << "\n";
        log << "    " << l2.dump() << "\n";
        log << "    " << l3.dump() << "\n";
        totalLatency += lat;
        return lat;
    }
};

vector<Task> parseTasks(const string& filename) {
    vector<Task> tasks;
    ifstream fin(filename);
    if (!fin) {
        cerr << "ERROR: Cannot open file: " << filename << "\n";
        return tasks;
    }
    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        istringstream ss(line);
        string token;
        Task t;
        ss >> token;                
        if (token != "TASK") continue;
        ss >> t.id;                 
        ss >> token;                
        ss >> t.burst;
        t.remaining = t.burst;
        ss >> token;                
        while (ss >> token) t.mem.push_back(token);
        tasks.push_back(t);
    }
    return tasks;
}

class Scheduler {
public:
    vector<Task*> readyQueue;
    Task* cores[NUM_CORES];
    int   coreFinish[NUM_CORES];   
    CacheHierarchy& cache;
    ostream& log;
    int cycle = 0;
    int completedTasks = 0;
    int perCoreCompletions[NUM_CORES] = {};
    long long perCoreLatency[NUM_CORES] = {};

    Scheduler(CacheHierarchy& c, ostream& l) : cache(c), log(l) {
        for (int i = 0; i < NUM_CORES; ++i) { cores[i] = nullptr; coreFinish[i] = 0; }
    }

    void enqueue(Task* t) { readyQueue.push_back(t); }

    Task* pickSJF() {
        if (readyQueue.empty()) return nullptr;
        auto it = min_element(readyQueue.begin(), readyQueue.end(),
            [](Task* a, Task* b){ return a->remaining < b->remaining; });
        Task* chosen = *it;
        readyQueue.erase(it);
        return chosen;
    }

    void run() {
        log << "=== CPU Scheduler + Cache Simulator ===\n";
        log << "Algorithm : SJF Non-preemptive  |  Cores: " << NUM_CORES << "  |  Cache eviction: LRU\n\n";

        bool anyWork = true;
        while (anyWork) {
            anyWork = false;

            for (int c = 0; c < NUM_CORES; ++c) {
                if (cores[c] == nullptr && !readyQueue.empty()) {
                    cores[c] = pickSJF();
                    if (cores[c]->startCycle == -1)
                        cores[c]->startCycle = cycle;
                }
            }

            bool allIdle = true;
            for (int c = 0; c < NUM_CORES; ++c)
                if (cores[c]) { allIdle = false; break; }
            if (allIdle && readyQueue.empty()) break;
            if (allIdle) { cycle++; anyWork = true; continue; }

            anyWork = true;
            cycle++;

            for (int c = 0; c < NUM_CORES; ++c) {
                Task* t = cores[c];
                if (!t) continue;

                log << "Cycle " << setw(4) << cycle
                    << " [Core " << c << "] Running: " << t->id
                    << "  (remaining=" << t->remaining << ")\n";

                if (t->memIdx < (int)t->mem.size()) {
                    const string& blk = t->mem[t->memIdx];
                    log << "  Requesting: " << blk << "\n";
                    int lat = cache.access(blk, log);
                    perCoreLatency[c] += lat;
                    t->memIdx++;
                } else {
                    log << "  (no memory request this cycle)\n";
                    log << "    " << cache.l1.dump() << "\n";
                    log << "    " << cache.l2.dump() << "\n";
                    log << "    " << cache.l3.dump() << "\n";
                }

                t->remaining--;

                if (t->remaining == 0) {
                    t->finishCycle = cycle;
                    log << "  >> Task " << t->id << " COMPLETED at cycle " << cycle << "\n";
                    completedTasks++;
                    perCoreCompletions[c]++;
                    cores[c] = nullptr;
                }
                log << "\n";
            }
        }
    }
};

int main(int argc, char* argv[]) {
    string inputFile = "input_task2.txt";
    if (argc >= 2) inputFile = argv[1];

    vector<Task> tasks = parseTasks(inputFile);
    if (tasks.empty()) {
        cerr << "No tasks loaded. Check input file.\n";
        return 1;
    }

    CacheHierarchy cache;
    ostream& log = cout;

    Scheduler sched(cache, log);
    for (auto& t : tasks) sched.enqueue(&t);

    sched.run();

    int totalBurst = 0, totalTurnaround = 0;
    log << "==========================================\n";
    log << "              FINAL RESULTS               \n";
    log << "==========================================\n";
    log << left << setw(6) << "Task"
        << setw(8) << "Burst"
        << setw(10) << "Start"
        << setw(10) << "Finish"
        << setw(14) << "Turnaround\n";
    log << string(48, '-') << "\n";
    for (auto& t : tasks) {
        int ta = (t.finishCycle >= 0 && t.startCycle >= 0) ? t.finishCycle - t.startCycle : -1;
        log << left << setw(6) << t.id
            << setw(8) << t.burst
            << setw(10) << (t.startCycle  >= 0 ? to_string(t.startCycle)  : "N/A")
            << setw(10) << (t.finishCycle >= 0 ? to_string(t.finishCycle) : "N/A")
            << setw(14) << (ta >= 0 ? to_string(ta) : "N/A") << "\n";
        if (t.finishCycle >= 0) totalTurnaround += (t.finishCycle - t.startCycle);
        totalBurst += t.burst;
    }
    log << "\n";
    log << "Total Cycles      : " << sched.cycle << "\n";
    log << "Tasks Completed   : " << sched.completedTasks << " / " << tasks.size() << "\n";
    log << "Scheduler         : SJF Non-preemptive (" << NUM_CORES << " cores)\n";
    log << "Cache Eviction    : LRU\n";
    log << "RAM Accesses      : " << cache.ramAccesses << "\n";
    log << "Total Mem Latency : " << cache.totalLatency << " cycles\n";
    log << "Avg Turnaround    : "
        << (sched.completedTasks ? totalTurnaround / sched.completedTasks : 0)
        << " cycles\n";
    log << "\nCache Hit/Miss Summary:\n";
    log << "  L1 hits=" << cache.hits[0] << "  misses=" << cache.misses[0] << "\n";
    log << "  L2 hits=" << cache.hits[1] << "  misses=" << cache.misses[1] << "\n";
    log << "  L3 hits=" << cache.hits[2] << "  misses=" << cache.misses[2] << "\n";
    log << "\nPer-Core Stats:\n";
    for (int c = 0; c < NUM_CORES; ++c) {
        log << "  Core " << c << ": " << sched.perCoreCompletions[c]
            << " tasks completed, " << sched.perCoreLatency[c]
            << " cycles memory latency\n";
    }
    log << "==========================================\n";

    return 0;
}
