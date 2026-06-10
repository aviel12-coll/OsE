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
    , ouputVec(), reduce_mutex()
{
    intermediateVectors.resize(multiThreadLevel);
    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        threads.emplace_back(&MapReduceJob::runThread, this, i);
        
    }



    // TODO: implement this constructor
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


void MapReduceJob::setstage(MapReduceStage stage) 
{


    //cast the value from enum to unit64_t
        uint64_t stage_value = static_cast<uint64_t>(stage);

    //read the current value of job_stage and update it to the new stage value
        uint64_t current_stage = job_stage.load();
        
    // create a mask to save the rest of the bits in job_stage except the stage bits
        uint64_t mask = ~(0x3ULL << 62);
        uint64_t new_stage;
        do {
            new_stage = (current_stage & mask) | (stage_value<<62);
        } while (!job_stage.compare_exchange_weak(current_stage, new_stage));   

    
}



// the role of this function is to wait for all the threads to finish their work and then join them
// join just stop the main thread until the thread that is being joined is finished and then it continues to the next line of code
void MapReduceJob::wait(void)
{
    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        threads[i].join();
    }
}

OutputVec MapReduceJob::getOutput(void)
{
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
    setstage(MAP_STAGE);

    IntermediateVec local_intermediatePairs;

    MapContext map_context(&local_intermediatePairs);

    //sort phase
    std::sort(local_intermediatePairs.begin(), local_intermediatePairs.end());

    // shuffle phase , first merge all the intermediate pairs from all the threads into one vector and then sort it by key
    intermediateVectors[thread_id] = local_intermediatePairs;

    map_barrier.arrive_and_wait();

    if (thread_id == 0)
    {
     shuffle();
    }
    map_barrier.arrive_and_wait();

    setstage(REDUCE_STAGE);
    reducePhase();

}

void MapReduceJob::shuffle(void)
{

    // merge all the intermediate pairs from all the threads into one vector and then sort it by key
    std::vector<IntermediatePair> all_intermediatePairs;

    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        all_intermediatePairs.insert(all_intermediatePairs.end(), intermediateVectors[i].begin(), intermediateVectors[i].end());
    }

    std::sort(all_intermediatePairs.begin(), all_intermediatePairs.end());
    this->mergedIntermediatePairs = std::move(all_intermediatePairs);
}


void MapReduceJob::reducePhase(void)
{
    IntermediateVec group;
    while (true)
    {
        std::lock_guard<std::mutex> lock(reduce_mutex);

        if (reduce_index >= mergedIntermediatePairs.size())
        {
            break;
        }

       auto current_key = mergedIntermediatePairs[reduce_index].first;
         while (reduce_index < mergedIntermediatePairs.size() && are_keys_equal(mergedIntermediatePairs[reduce_index].first, current_key))
         {
              group.push_back(mergedIntermediatePairs[reduce_index]);
              reduce_index++;
         }
        
    }
    ReduceContext reduce_context(ouputVec, reduce_mutex);
    Myclient.reduce( group, reduce_context);
    

}    
bool MapReduceJob::are_keys_equal(const std::shared_ptr<K2> &key1, const std::shared_ptr<K2> &key2)
{
    return !(*key1 < *key2) && !(*key2 < *key1);
}
