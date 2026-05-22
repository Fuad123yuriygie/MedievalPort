#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <string>
#include <functional>
#include "LoadData.h"

// Structure to hold data that needs to be uploaded to GPU
// This is thread-safe data that gets passed from loader thread to main thread
struct PendingModelData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<std::string> diffuseTextures;
    std::vector<int> materialIndices;
    std::string filePath;
    bool loadSuccess = false;
};

// Callback type: called when a model finishes loading
using ModelLoadedCallback = std::function<void(const PendingModelData&)>;

class ModelLoaderThread {
public:
    static ModelLoaderThread& GetInstance() {
        static ModelLoaderThread instance;
        return instance;
    }

    // Queue a file for async loading with a callback when done
    // The callback will be called from the worker thread when the model is loaded
    void QueueModelLoad(const std::string& filePath, ModelLoadedCallback callback);

    ~ModelLoaderThread();

private:
    ModelLoaderThread();

    // Thread worker function
    void LoadWorker();

    // Structure to hold a queued job
    struct LoadJob {
        std::string filePath;
        ModelLoadedCallback callback;
    };

    // Thread-safe queue for file paths and callbacks to load
    std::queue<LoadJob> loadQueue;
    std::mutex queueMutex;

    // Worker thread
    std::thread workerThread;
    bool shouldExit = false;
    std::mutex exitMutex;

    // Condition variable for signaling work
    std::condition_variable queueCV;
};
