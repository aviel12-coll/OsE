#include "MapReduceJob.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <chrono>
#include <thread>

class TestElement : public K1, public K2, public K3, public V1, public V2, public V3
{
public:
    int val;
    TestElement(int v) : val(v) {}
    bool operator<(const K1 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
    bool operator<(const K2 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
    bool operator<(const K3 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
};

class AdvancedClient : public MapReduceClient
{
public:
    void map(const std::shared_ptr<K1> key, const std::shared_ptr<V1> value, MapContext &context) const override
    {
        auto k = std::dynamic_pointer_cast<TestElement>(key);
        auto v = std::dynamic_pointer_cast<TestElement>(value);
        context.addIntermediate(k, v);
        context.addIntermediate(k, std::make_shared<TestElement>(v->val + 1));
    }

    void reduce(const IntermediateVec &pairs, ReduceContext &context) const override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!pairs.empty())
        {
            auto k = std::dynamic_pointer_cast<TestElement>(pairs[0].first);
            context.addOutput(k, std::make_shared<TestElement>(static_cast<int>(pairs.size())));
        }
    }
};

int main()
{
    AdvancedClient client;
    InputVec inputVec;
    for(int i = 0; i < 50; i++)
    {
        inputVec.push_back({std::make_shared<TestElement>(i % 5), std::make_shared<TestElement>(i)});
    }

    std::cout << "[TEST] Starting Job with 4 threads..." << std::endl;
    MapReduceJob job(client, inputVec, 4);

    bool saw_mapping = false;
    bool saw_reducing = false;
    for (int i = 0; i < 20; i++)
    {
        MapReduceState state = job.getState();
        assert(state.percentage >= 0.0 && state.percentage <= 100.0);
        if (state.stage == MAP_STAGE && state.percentage > 0) saw_mapping = true;
        if (state.stage == REDUCE_STAGE && state.percentage > 0) saw_reducing = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "[TEST] Testing job.wait()..." << std::endl;
    job.wait();

    MapReduceState final_state = job.getState();
    std::cout << "[TEST] Final Stage: " << final_state.stage << " | Final Percentage: " << final_state.percentage << "%" << std::endl;

    assert(final_state.stage == REDUCE_STAGE);
    assert(final_state.percentage == 100.0);

    if(!job.isDone())
    {
        std::cerr << "[FAIL] isDone() returned false after wait!" << std::endl;
        return 1;
    }

    try {
        job.wait(); 
    } catch (...) {
        std::cerr << "[FAIL] Call to wait() a second time caused an exception!" << std::endl;
        return 1;
    }

    std::cout << "[SUCCESS] All advanced checks passed successfully!" << std::endl;
    return 0;
}
