#include "MapReduceJob.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include <iomanip>

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::milliseconds;

// ---------- Keys / Values ----------

class IntKey : public K1, public K2, public K3 {
public:
    int val;
    IntKey(int v) : val(v) {}
    bool operator<(const K1& o) const override { return val < static_cast<const IntKey&>(o).val; }
    bool operator<(const K2& o) const override { return val < static_cast<const IntKey&>(o).val; }
    bool operator<(const K3& o) const override { return val < static_cast<const IntKey&>(o).val; }
};

class LongValue : public V1, public V2, public V3 {
public:
    long long val;
    LongValue(long long v) : val(v) {}
};

// ---------- Heavy compute: sieve of Eratosthenes ----------

static long long countPrimes(int limit) {
    std::vector<bool> sieve(limit + 1, true);
    sieve[0] = sieve[1] = false;
    for (int i = 2; (long long)i * i <= limit; i++)
        if (sieve[i])
            for (int j = i * i; j <= limit; j += i)
                sieve[j] = false;
    long long cnt = 0;
    for (int i = 2; i <= limit; i++)
        if (sieve[i]) cnt++;
    return cnt;
}

// Client: each map call runs a sieve scaled by the input key (~5-15ms per call)
class PrimeCountClient : public MapReduceClient {
public:
    void map(const std::shared_ptr<K1> key, const std::shared_ptr<V1>,
             MapContext& ctx) const override {
        int n = std::static_pointer_cast<IntKey>(key)->val;
        long long count = countPrimes(300000 + n * 7000); // ~5-15ms per call
        ctx.addIntermediate(std::make_shared<IntKey>(n % 8),
                            std::make_shared<LongValue>(count));
    }

    void reduce(const IntermediateVec& pairs, ReduceContext& ctx) const override {
        auto k = std::static_pointer_cast<IntKey>(pairs[0].first);
        long long total = 0;
        for (auto& p : pairs)
            total += std::static_pointer_cast<LongValue>(p.second)->val;
        ctx.addOutput(std::make_shared<IntKey>(k->val),
                      std::make_shared<LongValue>(total));
    }
};

// ---------- Benchmark ----------

struct BenchResult {
    int    threads;
    long long ms;
    long long checksum;
};

static BenchResult runBench(InputVec& inputVec, int numThreads) {
    PrimeCountClient client;
    auto t0 = Clock::now();
    MapReduceJob job(client, inputVec, numThreads);
    OutputVec out = job.getOutput();
    long long ms = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();

    long long checksum = 0;
    for (auto& p : out)
        checksum += std::static_pointer_cast<LongValue>(p.second)->val;

    return {numThreads, ms, checksum};
}

int main() {
    const int N = 80; // 80 inputs, sieve 300K–860K per map call
    InputVec inputVec;
    for (int i = 1; i <= N; i++)
        inputVec.push_back({std::make_shared<IntKey>(i), std::make_shared<LongValue>(0)});

    std::cout << "Benchmark: " << N << " inputs, sieve 300K-860K per map call\n";
    std::cout << std::string(52, '-') << "\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(14) << "Time (ms)"
              << std::setw(12) << "Speedup"
              << "Efficiency\n";
    std::cout << std::string(52, '-') << "\n";

    std::vector<BenchResult> results;
    for (int t : {1, 2, 4, 8})
        results.push_back(runBench(inputVec, t));

    // Verify correctness: all thread counts must produce the same checksum
    for (size_t i = 1; i < results.size(); i++)
        assert(results[i].checksum == results[0].checksum
               && "checksum mismatch — parallel result differs from single-threaded!");

    long long baseline = results[0].ms;
    for (auto& r : results) {
        double speedup    = (double)baseline / r.ms;
        double efficiency = speedup / r.threads * 100.0;
        std::cout << std::left
                  << std::setw(10) << r.threads
                  << std::setw(14) << r.ms
                  << std::setw(12) << (std::to_string(speedup).substr(0,4) + "x")
                  << std::fixed << std::setprecision(1) << efficiency << "%\n";
    }

    std::cout << "\n[SUCCESS] All checksums match — results are correct.\n";
    return 0;
}
