#include "MapReduceJob.h"
#include <iostream>
#include <cassert>
#include <string>
#include <map>
#include <vector>

// K1: dummy key (map function ignores it)
class NullKey : public K1 {
public:
    bool operator<(const K1&) const override { return false; }
};

// V1: integer input value
class IntInput : public V1 {
public:
    int n;
    IntInput(int n) : n(n) {}
};

// K2, K3: category name (e.g. "even", "prime")
class CategoryKey : public K2, public K3 {
public:
    std::string name;
    CategoryKey(const std::string& s) : name(s) {}
    bool operator<(const K2& o) const override { return name < static_cast<const CategoryKey&>(o).name; }
    bool operator<(const K3& o) const override { return name < static_cast<const CategoryKey&>(o).name; }
};

// V2, V3: integer count
class CountValue : public V2, public V3 {
public:
    int count;
    CountValue(int c) : count(c) {}
};

static bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; i++)
        if (n % i == 0) return false;
    return true;
}

// Complex client: each number emits up to 4 intermediate pairs
// (even/odd always, plus div3/div5/prime when applicable)
class NumberClassifierClient : public MapReduceClient {
public:
    void map(const std::shared_ptr<K1>, const std::shared_ptr<V1> value,
             MapContext& ctx) const override {
        int n = std::static_pointer_cast<IntInput>(value)->n;
        auto emit = [&](const std::string& cat) {
            ctx.addIntermediate(std::make_shared<CategoryKey>(cat),
                                std::make_shared<CountValue>(1));
        };
        emit(n % 2 == 0 ? "even" : "odd");
        if (n % 3 == 0) emit("div3");
        if (n % 5 == 0) emit("div5");
        if (isPrime(n))  emit("prime");
    }

    void reduce(const IntermediateVec& pairs, ReduceContext& ctx) const override {
        auto key = std::static_pointer_cast<CategoryKey>(pairs[0].first);
        int total = 0;
        for (auto& p : pairs)
            total += std::static_pointer_cast<CountValue>(p.second)->count;
        ctx.addOutput(std::make_shared<CategoryKey>(key->name),
                      std::make_shared<CountValue>(total));
    }
};

// Compute expected result sequentially (ground truth)
static std::map<std::string, int> computeExpected(const std::vector<int>& nums) {
    std::map<std::string, int> res;
    for (int n : nums) {
        res[n % 2 == 0 ? "even" : "odd"]++;
        if (n % 3 == 0) res["div3"]++;
        if (n % 5 == 0) res["div5"]++;
        if (isPrime(n))  res["prime"]++;
    }
    return res;
}

static void runAndVerify(const std::string& label, InputVec& inputVec,
                         int numThreads,
                         const std::map<std::string, int>& expected) {
    NumberClassifierClient client;
    std::cout << "[TEST] " << label << " | threads=" << numThreads << std::endl;

    MapReduceJob job(client, inputVec, numThreads);
    OutputVec output = job.getOutput();

    // Build result map from output
    std::map<std::string, int> result;
    for (auto& p : output) {
        auto k = std::static_pointer_cast<CategoryKey>(p.first);
        auto v = std::static_pointer_cast<CountValue>(p.second);
        result[k->name] = v->count;
    }

    // Exact match against expected
    assert(result.size() == expected.size() && "wrong number of output categories");
    for (auto& [cat, cnt] : expected) {
        assert(result.count(cat) && "missing category in output");
        assert(result.at(cat) == cnt && "count mismatch for category");
    }

    // Final state must be REDUCE_STAGE at 100%
    MapReduceState state = job.getState();
    assert(state.stage == REDUCE_STAGE);
    assert(state.percentage == 100.0);
    assert(job.isDone());

    // Second wait() must not crash (idempotency)
    job.wait();

    std::cout << "[PASS] ";
    for (auto& [cat, cnt] : result)
        std::cout << cat << "=" << cnt << " ";
    std::cout << std::endl;
}

int main() {
    // --- Test 1: numbers 1..100, multiple thread counts ---
    {
        std::vector<int> nums;
        for (int i = 1; i <= 100; i++) nums.push_back(i);

        InputVec inputVec;
        for (int n : nums)
            inputVec.push_back({std::make_shared<NullKey>(), std::make_shared<IntInput>(n)});

        auto expected = computeExpected(nums);

        for (int t : {1, 2, 4, 8})
            runAndVerify("numbers 1-100", inputVec, t, expected);
    }

    // --- Test 2: single prime number ---
    {
        InputVec single;
        single.push_back({std::make_shared<NullKey>(), std::make_shared<IntInput>(7)});
        auto exp = computeExpected({7});

        runAndVerify("single prime (7)", single, 1, exp);
        runAndVerify("single prime (7)", single, 4, exp);
    }

    // --- Test 3: all identical values (all keys collapse into one group) ---
    {
        InputVec same;
        std::vector<int> sixes(10, 6);
        for (int v : sixes)
            same.push_back({std::make_shared<NullKey>(), std::make_shared<IntInput>(v)});
        auto exp = computeExpected(sixes);

        runAndVerify("10x value=6", same, 1, exp);
        runAndVerify("10x value=6", same, 4, exp);
    }

    // --- Test 4: large input, more threads than elements ---
    {
        InputVec small;
        std::vector<int> few = {2, 3};
        for (int v : few)
            small.push_back({std::make_shared<NullKey>(), std::make_shared<IntInput>(v)});
        auto exp = computeExpected(few);

        runAndVerify("2 elements, 8 threads", small, 8, exp);
    }

    std::cout << "\n[SUCCESS] All complex client tests passed!" << std::endl;
    return 0;
}
