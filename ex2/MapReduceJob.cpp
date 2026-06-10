#include "MapReduceJob.h"
#include <thread>
#include <atomic>
#include <cmath>

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
{
    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        threads.emplace_back([this]()
        {
            IntermediateVec intermediatePairs;
            MapContext map_context(&intermediatePairs);
            while (true)
            {
                int index = count.fetch_add(1);
                if (index >= MyinputVec.size())
                {
                    break;
                }
                Myclient.map(MyinputVec[index].first, MyinputVec[index].second, map_context);
            }

        });
        
    }



    // TODO: implement this constructor
}

MapReduceState MapReduceJob::getState(void) const
{
    // TODO: implement this function
}


void MapReduceJob::setstage(MapReduceStage stage) 
{


    //cast the value from enum to unit64_t
        uint64_t stage_value = static_cast<uint64_t>(stage);

    //read the current value of job_stage and update it to the new stage value
        uint64_t current_stage = job_stage.load();
        
    // create a mask to save the rest of the bits in job_stage except the stage bits
        uint64_t mask =0xFFFFFFFFFFFFFFFF ^ 0xF;
        unit64_t new_stage;
        while (!job_stage.compare_exchange_weak(current_stage, stage_value)) {
            // If the exchange failed, current_stage is updated with the latest value of job_stage
            // We can check if the current stage is already the desired stage to avoid unnecessary updates
            if (current_stage == stage_value) {
                break; // The stage is already set to the desired value, no need to update
            }

        }      

    
}




void MapReduceJob::wait(void)
{
    // TODO: implement this function
}

OutputVec MapReduceJob::getOutput(void)
{
    // TODO: implement this function
}

bool MapReduceJob::isDone(void) const
{
    // TODO: implement this function
}

MapReduceJob::~MapReduceJob()
{
    // TODO: implement this destructor
}
