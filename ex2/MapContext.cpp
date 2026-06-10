#include "MapContext.h"

// implement here your constructor and destructor

MapContext::MapContext(IntermediateVec * intermediatePairs)
    : intermediatePairs(intermediatePairs)
{
}

MapContext::~MapContext()
{
}

void MapContext::addIntermediate(std::shared_ptr<K2> key, std::shared_ptr<V2> value)
{
    if (intermediatePairs)
    {
        intermediatePairs->emplace_back(std::move(key), std::move(value));
    }
}
