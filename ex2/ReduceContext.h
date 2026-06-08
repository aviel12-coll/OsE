#ifndef REDUCE_CONTEXT_H
#define REDUCE_CONTEXT_H
#include <vector>
#include <memory>
#include <mutex>

#include "MapReduceKeys.h"
using outputvec= std::vector<std::pair<std::shared_ptr<K3>, std::shared_ptr<V3>>>;
class ReduceContext
{

private:
outputvec &output;
std::mutex &mutex;    
public:

    ReduceContext(outputvec &output, std::mutex &mutex);
    ~ReduceContext();
   

    void addOutput(std::shared_ptr<K3> key, std::shared_ptr<V3> value);

    /*
    You can change everything else, including the constructor/destructor
    You can also add fields here (even public ones)
    */

};

#endif // REDUCE_CONTEXT_H