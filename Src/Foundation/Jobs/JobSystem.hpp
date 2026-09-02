#pragma once

#include "TaskScheduler.h"
#include <cstdint>

namespace vengine {

// Owns the engine-wide enkiTS task scheduler: construction, configuration and
// shutdown. Task submission still goes through scheduler() directly
// (enki::TaskSet / enki::IPinnedTask) - this only removes the setup/teardown
// boilerplate from the application layer.
class JobSystem {
public:
    JobSystem() = default;
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Starts the scheduler. `extraThreads` worker threads are created on top of
    // the hardware thread count so a permanently-blocked pinned task (e.g. the
    // async texture loader) doesn't steal a core from the pool.
    void initialize(uint32_t extraThreads = 0);

    // Waits for all outstanding tasks and tears the scheduler down. Safe to call
    // more than once; also run from the destructor.
    void shutdown();

    enki::TaskScheduler& scheduler() { return taskScheduler; }
    uint32_t threadCount() { return taskScheduler.GetNumTaskThreads(); }

private:
    enki::TaskScheduler taskScheduler;
    bool initialized = false;
};

} // namespace vengine
