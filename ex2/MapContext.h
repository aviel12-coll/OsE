#ifndef MAP_CONTEXT_H
#define MAP_CONTEXT_H

#include "MapReduceKeys.h"
// you can add other includes as you wish
using intermediatePair = std::pair<std::shared_ptr<K2>, std::shared_ptr<V2>>;
class MapContext
{
private:
    intermediatePair * intermediatePairs;     
public:
    /*
    You must keep and implement this function:
    */
   MapContext(intermediatePair * intermediatePairs): intermediatePairs(intermediatePairs) {}
    void addIntermediate(std::shared_ptr<K2> key, std::shared_ptr<V2> value);

    /*
    You can change everything else, including the constructor/desturctor
    You can also add fields here (even public ones)
    */
};

#endif // MAP_CONTEXT_H