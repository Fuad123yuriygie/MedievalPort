#pragma once

#include "io/LoadData.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ModelLoaderThread {
public:
    explicit ModelLoaderThread(ImportSettings settings = {});
    ~ModelLoaderThread();
    ModelLoaderThread(const ModelLoaderThread&) = delete;
    ModelLoaderThread& operator=(const ModelLoaderThread&) = delete;

    bool QueueModelLoad(ModelDescription description, ModelId requestId);
    std::vector<PendingModelData> TakeCompleted(std::size_t maxCount);
    void Stop();

private:
    struct LoadJob {
        ModelDescription description;
        ModelId requestId;
    };

    void LoadWorker();

    ImportSettings settings_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    bool shouldExit_ = false;
    std::queue<LoadJob> loadQueue_;
    std::queue<PendingModelData> completedQueue_;
    std::once_flag stopOnce_;
    std::thread workerThread_;
};
