#ifndef HOT_STREAMING_WORKERTHREADS_H
#define HOT_STREAMING_WORKERTHREADS_H

#include <functional>

// Background helpers the streaming code starts, tracked so closing the game can stop them before statics are destroyed.
namespace HoT::Streaming::WorkerThreads {

// Runs fn on a tracked background thread. Returns false without running it once shutdown has begun.
bool Start(std::function<void()> fn);

bool ShuttingDown();

// Sleeps up to ms, returns false as soon as shutdown begins.
bool SleepFor(unsigned long long ms);

// Stops new threads from starting and wakes every SleepFor.
void BeginShutdown();

// Waits up to timeoutMs for every tracked thread to finish, returns false on timeout.
bool WaitAll(unsigned long long timeoutMs);

}

#endif
