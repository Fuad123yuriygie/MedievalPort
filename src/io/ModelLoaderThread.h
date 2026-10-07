#pragma once

#include "io/LoadData.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>

// Asynchronous import worker. Cancellation is cooperative and addressed by request ID: closing the
// application or discarding a model stops its queued or in-flight import instead of waiting for it.
class ModelLoaderThread {
public:
    explicit ModelLoaderThread(ImportSettings settings = {});
    ~ModelLoaderThread();
    ModelLoaderThread(const ModelLoaderThread&) = delete;
    ModelLoaderThread& operator=(const ModelLoaderThread&) = delete;

    bool QueueModelLoad(ModelDescription description, ModelId requestId);
    std::vector<PendingModelData> TakeCompleted(std::size_t maxCount);
    // Requests cooperative cancellation of the queued or active import for this request ID.
    void Cancel(ModelId requestId);
    void Stop();

    // Total bytes currently retained by queued, active and completed import work.
    std::size_t ResidentBytes() const;

private:
    struct LoadJob {
        ModelDescription description;
        ModelId requestId;
        std::stop_source stop;
        std::size_t estimatedBytes = 0;
    };

    void LoadWorker(std::stop_token stop);

    ImportSettings settings_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCV_;
    bool shouldExit_ = false;
    std::deque<LoadJob> loadQueue_;
    std::deque<PendingModelData> completedQueue_;
    std::unordered_map<ModelId, std::stop_source> sources_;
    std::size_t residentBytes_ = 0;
    std::once_flag stopOnce_;
    std::jthread workerThread_;
};
