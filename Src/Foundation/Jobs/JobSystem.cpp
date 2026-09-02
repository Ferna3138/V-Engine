#include "Foundation/Jobs/JobSystem.hpp"

namespace vengine {

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::initialize(uint32_t extraThreads) {
    if (initialized) return;

    enki::TaskSchedulerConfig config;
    // enkiTS default is GetNumHardwareThreads()-1 (thread 0 is the calling
    // thread). Any extra threads requested here sit on top of that so a fully
    // blocked pinned task can be parked without shrinking the usable pool.
    config.numTaskThreadsToCreate = enki::GetNumHardwareThreads() + extraThreads;
    taskScheduler.Initialize(config);

    initialized = true;
}

void JobSystem::shutdown() {
    if (!initialized) return;
    taskScheduler.WaitforAllAndShutdown();
    initialized = false;
}

} // namespace vengine
