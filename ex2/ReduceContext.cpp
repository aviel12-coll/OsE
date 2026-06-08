#include "ReduceContext.h"
#include <mutex>

// implement here your constructor and destructor

ReduceContext::ReduceContext(outputvec &output, std::mutex &mutex) 
    : output(output), mutex(mutex) {}

ReduceContext::~ReduceContext() {}


void ReduceContext::addOutput(std::shared_ptr<K3> key, std::shared_ptr<V3> value)
{
   std::lock_guard<std::mutex> lock(mutex);
    output.push_back({key, value});
}