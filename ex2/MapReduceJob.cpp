#include "MapReduceJob.h"
#include <thread>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>
/*
===============================================
Implement:
===============================================
*/


// clinent is the client that contains the map and reduce functions
// inputVec is the vector of (K1, V1) pairs that will be processed
// multiThreadLevel is the number of threads that will be used to process the job
//the constructor should start the job by creating the threads and assigning them to process the input vector


MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel):
    Myclient(client), MyinputVec(inputVec), MymultiThreadLevel(multiThreadLevel), count(0), map_barrier(multiThreadLevel)
    , ouputVec(), reduce_mutex(), reduce_index(0)
{
    intermediateVectors.resize(multiThreadLevel);
    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        threads.emplace_back(&MapReduceJob::runThread, this, i);
        
    }

}

MapReduceState MapReduceJob::getState(void) const
{
    // TODO: implement this function
    //get the current stage value from job_stage 
     uint64_t val = job_stage.load();

     //take the two last bits of stage_value and the 31 bits of total and return them as a MapReduceState object
     uint64_t stage_value = (val >> 62) & 0x3;

     uint64_t total = (val >> 31) & 0x7FFFFFFF;

     uint64_t processed = val & 0x7FFFFFFF;

     // builf the MapReduceState object
     MapReduceState state;
        state.stage = static_cast<MapReduceStage>(stage_value);

        if (total == 0)
        {
            state.percentage = 0.0;
        }
        else
        {
            state.percentage = static_cast<double>(processed) / total * 100.0;
        }
        return state;


}

void MapReduceJob::setstage(MapReduceStage stage, uint64_t total_elements) 
{
    uint64_t stage_value = static_cast<uint64_t>(stage);
    
    uint64_t new_stage = (stage_value << 62) | (total_elements << 31) | 0ULL;
    
    job_stage.store(new_stage);   
}



// the role of this function is to wait for all the threads to finish their work and then join them
// join just stop the main thread until the thread that is being joined is finished and then it continues to the next line of code
void MapReduceJob::wait(void)
{
    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }
    threads.clear();
}

OutputVec MapReduceJob::getOutput(void)
{
    wait();
    return this->ouputVec;
}

bool MapReduceJob::isDone(void) const
{

    uint64_t val = job_stage.load();
    uint64_t total = (val >> 31) & 0x7FFFFFFF;
    uint64_t processed = val & 0x7FFFFFFF;

    return (total>0 && processed == total);

}

MapReduceJob::~MapReduceJob()
{
   wait();
}


void MapReduceJob::runThread(int thread_id)
{
    // map phase

    if (thread_id == 0)
    {
           setstage(MAP_STAGE, MyinputVec.size());

    }
    map_barrier.arrive_and_wait();


    IntermediateVec local_intermediatePairs;

    MapContext map_context(&intermediateVectors[thread_id]);

    while (true)
    {
        int idx = count.fetch_add(1);
        if (idx >= (int)MyinputVec.size()) break;
        Myclient.map(MyinputVec[idx].first, MyinputVec[idx].second, map_context);
    }

    //sort phase
    std::sort( intermediateVectors[thread_id].begin() , intermediateVectors[thread_id].end(),
    [](const IntermediatePair& a, const IntermediatePair& b) {
        return *(a.first) < *(b.first);
    });

    // shuffle phase , first merge all the intermediate pairs from all the threads into one vector and then sort it by key
    map_barrier.arrive_and_wait();

    if (thread_id == 0)
    {
        shuffle();
        setstage(REDUCE_STAGE, mergedIntermediatePairs.size());
    }
    map_barrier.arrive_and_wait();

    reducePhase();

}

void MapReduceJob::shuffle(void)
{

    // merge all the intermediate pairs from all the threads into one vector and then sort it by key
    std::vector<IntermediatePair> all_intermediatePairs;

    for (size_t i = 0; i < intermediateVectors.size(); i++)
    {
        all_intermediatePairs.insert(all_intermediatePairs.end(), intermediateVectors[i].begin(), intermediateVectors[i].end());
    }

    std::sort(all_intermediatePairs.begin(), all_intermediatePairs.end(),
        [](const IntermediatePair& a, const IntermediatePair& b) {
            return *(a.first) < *(b.first);
        });
    this->mergedIntermediatePairs = std::move(all_intermediatePairs);
}


void MapReduceJob::reducePhase(void)
{
    while (true)
    {
        IntermediateVec group;
        {
            std::lock_guard<std::mutex> lock(reduce_mutex);

            if (reduce_index >= mergedIntermediatePairs.size())
                break;

            auto current_key = mergedIntermediatePairs[reduce_index].first;
            while (reduce_index < mergedIntermediatePairs.size() && are_keys_equal(mergedIntermediatePairs[reduce_index].first, current_key))
            {
                group.push_back(mergedIntermediatePairs[reduce_index]);
                reduce_index++;
            }
        }
        ReduceContext reduce_context(ouputVec, reduce_mutex);
        Myclient.reduce(group, reduce_context);
        job_stage.fetch_add((uint64_t)group.size());
    }
}    
bool MapReduceJob::are_keys_equal(const std::shared_ptr<K2> &key1, const std::shared_ptr<K2> &key2)
{
    return !(*key1 < *key2) && !(*key2 < *key1);
}
