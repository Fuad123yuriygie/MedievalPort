#include "io/ModelLoaderThread.h"
#include "io/PathUtils.h"
#include "utils/Log.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <system_error>
#include <utility>

namespace {
std::size_t EstimateSourceBytes(const ModelDescription& description) {
    std::error_code error;
    const auto path = PathFromUtf8(description.filePath);
    const auto size = std::filesystem::file_size(path, error);
    if(error) {
        return 0;
    }
    return static_cast<std::size_t>(size);
}
} // namespace

ModelLoaderThread::ModelLoaderThread(ImportSettings settings) : settings_(std::move(settings)) {
    workerThread_ = std::jthread([this](std::stop_token stop) { LoadWorker(stop); });
}

ModelLoaderThread::~ModelLoaderThread() {
    Stop();
}

bool ModelLoaderThread::QueueModelLoad(ModelDescription description, ModelId requestId) {
    try {
        const std::size_t estimate = EstimateSourceBytes(description);
        if(settings_.limits.maxSourceBytes != 0 && estimate > settings_.limits.maxSourceBytes) {
            Log(LogLevel::Warning, "Rejecting oversized import: " + description.filePath);
            return false;
        }
        {
            const std::lock_guard lock(queueMutex_);
            if(shouldExit_) {
                return false;
            }
            // Byte-based admission control: queue length alone does not bound memory.
            if(settings_.limits.maxPendingBytes != 0 &&
               residentBytes_ + estimate > settings_.limits.maxPendingBytes) {
                return false;
            }
            LoadJob job;
            job.description = std::move(description);
            job.requestId = requestId;
            job.estimatedBytes = estimate;
            sources_[requestId] = job.stop;
            residentBytes_ += estimate;
            loadQueue_.push_back(std::move(job));
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
            residentBytes_ -= PendingModelBytes(completedQueue_.front());
            completed.push_back(std::move(completedQueue_.front()));
            completedQueue_.pop_front();
        }
    }
    queueCV_.notify_all();
    return completed;
}

void ModelLoaderThread::Cancel(ModelId requestId) {
    {
        const std::lock_guard lock(queueMutex_);
        const auto queued = std::find_if(loadQueue_.begin(), loadQueue_.end(), [requestId](const auto& job) {
            return job.requestId == requestId;
        });
        if(queued != loadQueue_.end()) {
            residentBytes_ -= queued->estimatedBytes;
            loadQueue_.erase(queued);
            sources_.erase(requestId);
        }
        else if(const auto found = sources_.find(requestId); found != sources_.end()) {
            found->second.request_stop();
        }
    }
    queueCV_.notify_all();
}

std::size_t ModelLoaderThread::ResidentBytes() const {
    const std::lock_guard lock(queueMutex_);
    return residentBytes_;
}

void ModelLoaderThread::Stop() {
    std::call_once(stopOnce_, [this] {
        {
            const std::lock_guard lock(queueMutex_);
            shouldExit_ = true;
            for(auto& [id, source] : sources_) {
                source.request_stop();
            }
            sources_.clear();
            loadQueue_.clear();
            completedQueue_.clear();
            residentBytes_ = 0;
        }
        queueCV_.notify_all();
        workerThread_.request_stop();
        if(workerThread_.joinable()) {
            workerThread_.join();
        }
    });
}

void ModelLoaderThread::LoadWorker(std::stop_token stop) {
    try {
        for(;;) {
            LoadJob job;
            {
                std::unique_lock lock(queueMutex_);
                // Apply byte and completion backpressure before parsing the next large model.
                queueCV_.wait(lock, [this, &stop] {
                    if(shouldExit_ || stop.stop_requested()) {
                        return true;
                    }
                    if(loadQueue_.empty() ||
                       completedQueue_.size() >= settings_.limits.maxCompletedImports) {
                        return false;
                    }
                    return settings_.limits.maxPendingBytes == 0 ||
                           residentBytes_ <= settings_.limits.maxPendingBytes;
                });
                if(shouldExit_ || stop.stop_requested()) {
                    return;
                }
                job = std::move(loadQueue_.front());
                loadQueue_.pop_front();
            }

            const auto cancellation = job.stop.get_token();
            PendingModelData result;
            bool cancelled = false;
            try {
                result = LoadModelData(job.description, settings_, cancellation);
            }
            catch(const ImportCancelled&) {
                cancelled = true;
            }
            catch(const std::exception& error) {
                result.description = job.description;
                result.error = error.what();
            }
            catch(...) {
                result.description = job.description;
                result.error = "Unknown model import error";
            }
            if(cancelled || cancellation.stop_requested()) {
                const std::lock_guard lock(queueMutex_);
                sources_.erase(job.requestId);
                residentBytes_ -= std::min(residentBytes_, job.estimatedBytes);
                queueCV_.notify_all();
                continue;
            }
            result.requestId = job.requestId;
            {
                const std::lock_guard lock(queueMutex_);
                sources_.erase(job.requestId);
                residentBytes_ -= std::min(residentBytes_, job.estimatedBytes);
                if(shouldExit_) {
                    return;
                }
                residentBytes_ += PendingModelBytes(result);
                completedQueue_.push_back(std::move(result));
            }
            queueCV_.notify_all();
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
    loadQueue_.clear();
    completedQueue_.clear();
    residentBytes_ = 0;
    queueCV_.notify_all();
}
