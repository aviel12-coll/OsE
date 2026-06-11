#ifndef MAP_REDUCE_JOB_H
#define MAP_REDUCE_JOB_H

#include <atomic>
#include <cstdint>
#include <barrier>
#include <thread>
#include <vector>
#include <mutex>

#include "MapReduceClient.h"
// you can add other includes here

enum MapReduceStage
{
	UNDEFINED_STAGE, // 0
	MAP_STAGE, // 1
	SHUFFLE_STAGE, // 2
	REDUCE_STAGE // 3
};

class MapReduceState
{
private:
	
public:
	MapReduceStage stage;
	double percentage;

	inline bool operator==(const MapReduceState &other) const
	{
		return this->stage == other.stage && std::abs(this->percentage - other.percentage) < 1e-6;
	}

	inline bool operator!=(const MapReduceState &other) const
	{
		return !(*this == other);
	}
};

class MapReduceJob
{

public:
	/*
	You CAN NOT change or add properties to this part (public API).
	*/

	MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel);

	~MapReduceJob();

	MapReduceState getState(void) const;

	bool isDone(void) const;
	
	void wait(void);

	OutputVec getOutput(void);

	void setstage(MapReduceStage stage, uint64_t total_elements);
	void runThread(int thread_id);
	void shuffle(void);
	void reducePhase(void);
	bool are_keys_equal(const std::shared_ptr<K2> &key1, const std::shared_ptr<K2> &key2);

private:
	/*
		You can change everything on this part (these are just recommendations)
	*/
std::atomic<uint64_t> job_stage;
std::atomic<int> count;
std::barrier<> map_barrier;
std::vector<std::thread> threads;
std::mutex reduce_mutex;
OutputVec ouputVec;
size_t reduce_index;


std::vector<IntermediateVec> intermediateVectors; // one vector for each thread to store the intermediate pairs
IntermediateVec mergedIntermediatePairs;

const MapReduceClient &Myclient;
const InputVec &MyinputVec;
const int MymultiThreadLevel;


	// you can add other properties here


};
	
#endif // MAP_REDUCE_JOB_H
