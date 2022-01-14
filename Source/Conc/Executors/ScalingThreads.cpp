#include "FifoQueue.h"
#include "BagQueue.h"
#include "ScalingExecutor.h"

std::shared_ptr<IExecutor> CreateScalingFIFOExecutor(int min, int max, int linger) {
    using namespace Internal::Executor;
    return std::make_shared<ScalingExecutor<FifoQueue>>(min, max, linger);
}

std::shared_ptr<IExecutor> CreateScalingBagExecutor(int min, int max, int linger) {
    using namespace Internal::Executor;
    return std::make_shared<ScalingExecutor<BagQueue>>(min, max, linger);
}