#include "MapReduceJob.h"
#include <thread>
#include <atomic>
#include <cmath>

/*
===============================================
Implement:
===============================================
*/

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel):
    Myclient(client), MyinputVec(inputVec), MymultiThreadLevel(multiThreadLevel)
{
    count = 0;

    for (int i = 0; i < MymultiThreadLevel; i++)
    {
        std::thread t([this](){
            while (true)
            {
                int index = count.fetch_add(1);
                if (index >= MyinputVec.size())
                {
                    break;
                }
                Myclient.map(MyinputVec[index].first, MyinputVec[index].second, *this);
            }
        });
        t.detach();
    }



    // TODO: implement this constructor
}

MapReduceState MapReduceJob::getState(void) const
{
    // TODO: implement this function
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
