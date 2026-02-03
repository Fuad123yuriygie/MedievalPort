#include "ModelLoaderThread.h"
#include <iostream>

ModelLoaderThread::ModelLoaderThread() {
    // Start the worker thread
    workerThread = std::thread(&ModelLoaderThread::LoadWorker, this);
}

ModelLoaderThread::~ModelLoaderThread() {
    {
        std::lock_guard<std::mutex> lock(exitMutex);
        shouldExit = true;
    }
    queueCV.notify_one();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void ModelLoaderThread::QueueModelLoad(const std::string& filePath, ModelLoadedCallback callback) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        loadQueue.push({filePath, callback});
    }
    queueCV.notify_one();
}

void ModelLoaderThread::LoadWorker() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);

        // Wait for a job or exit signal
        queueCV.wait(lock, [this] {
            return !loadQueue.empty() || shouldExit;
        });

        // Check if we should exit
        {
            std::lock_guard<std::mutex> exitLock(exitMutex);
            if (shouldExit && loadQueue.empty()) {
                break;
            }
        }

        // Process one item from the queue
        if (!loadQueue.empty()) {
            LoadJob job = loadQueue.front();
            loadQueue.pop();
            lock.unlock();  // Release lock while doing the heavy lifting

            // Load the model data (this is CPU-intensive and thread-safe)
            std::vector<float> vertices;
            std::vector<unsigned int> indices;
            std::vector<std::string> diffuseTextures;
            std::vector<int> materialIndices;

            bool success = LoadObject(job.filePath, vertices, indices, diffuseTextures, materialIndices);

            if (success) {
                std::cout << "Successfully loaded OBJ file on worker thread: " << job.filePath << std::endl;
            }
            else {
                std::cerr << "Failed to load OBJ file on worker thread: " << job.filePath << std::endl;
            }

            // Prepare the result
            PendingModelData result;
            result.vertices = std::move(vertices);
            result.indices = std::move(indices);
            result.diffuseTextures = std::move(diffuseTextures);
            result.materialIndices = std::move(materialIndices);
            result.filePath = job.filePath;
            result.loadSuccess = success;

            // Call the callback - this happens on the worker thread
            // The callback (typically FileParser::AddModelFromLoader) will handle GPU resource creation
            // by posting to the main thread if needed
            if (job.callback) {
                job.callback(result);
            }
        }
    }
}
