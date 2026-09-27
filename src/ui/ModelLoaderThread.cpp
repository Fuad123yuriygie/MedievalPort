#include "ui/ModelLoaderThread.h"
#include "utils/Log.h"

#include <algorithm>
#include <exception>
#include <utility>

ModelLoaderThread::ModelLoaderThread(ImportSettings settings)
    : settings_(settings), workerThread_(&ModelLoaderThread::LoadWorker, this) {
}

ModelLoaderThread::~ModelLoaderThread() {
    Stop();
}

bool ModelLoaderThread::QueueModelLoad(ModelDescription description, ModelId requestId) {
    try {
        {
            const std::lock_guard lock(queueMutex_);
            if(shouldExit_) {
                return false;
            }
            loadQueue_.push({std::move(description), requestId});
        }
        queueCV_.notify_all();
        return true;
    }
    catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
        return false;
    }
}

std::vector<PendingModelData> ModelLoaderThread::TakeCompleted(std::size_t maxCount) {
    std::vector<PendingModelData> completed;
    {
        const std::lock_guard lock(queueMutex_);
        const auto count = std::min(maxCount, completedQueue_.size());
        completed.reserve(count);
        for(std::size_t i = 0; i < count; ++i) {
            completed.push_back(std::move(completedQueue_.front()));
            completedQueue_.pop();
        }
    }
    queueCV_.notify_all();
    return completed;
}

void ModelLoaderThread::Stop() {
    std::call_once(stopOnce_, [this] {
        {
            const std::lock_guard lock(queueMutex_);
            shouldExit_ = true;
            while(!loadQueue_.empty()) {
                loadQueue_.pop();
            }
            while(!completedQueue_.empty()) {
                completedQueue_.pop();
            }
        }
        queueCV_.notify_all();
        if(workerThread_.joinable()) {
            workerThread_.join();
        }
    });
}

void ModelLoaderThread::LoadWorker() {
    try {
        for(;;) {
            LoadJob job;
            {
                std::unique_lock lock(queueMutex_);
                // Apply backpressure before parsing the next potentially large model.
                queueCV_.wait(lock, [this] {
                    return shouldExit_ || (!loadQueue_.empty() && completedQueue_.size() < 2);
                });
                if(shouldExit_) {
                    return;
                }
                job = std::move(loadQueue_.front());
                loadQueue_.pop();
            }

            PendingModelData result;
            try {
                result = LoadModelData(job.description, settings_);
            }
            catch(const std::exception& error) {
                result.description = std::move(job.description);
                result.error = error.what();
            }
            catch(...) {
                result.description = std::move(job.description);
                result.error = "Unknown model import error";
            }
            result.requestId = job.requestId;
            {
                const std::lock_guard lock(queueMutex_);
                if(shouldExit_) {
                    return;
                }
                completedQueue_.push(std::move(result));
            }
        }
    }
    catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
    }
    catch(...) {
        Log(LogLevel::Error, "Unexpected loader worker failure");
    }
    const std::lock_guard lock(queueMutex_);
    shouldExit_ = true;
    while(!loadQueue_.empty()) {
        loadQueue_.pop();
    }
    queueCV_.notify_all();
}
