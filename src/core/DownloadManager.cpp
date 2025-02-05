#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "core/DownloadManager.hpp"
#include "util/http.hpp"
#include "util/format.hpp"
#include "util/file.hpp"

namespace
{
    // Returns the file path where download manager state should be stored
    // Creates ~/.sdm if necessary (on non-Windows platforms)
    std::string getStateFilePath()
    {
        const char *home = std::getenv("HOME");
        if (!home)
        {
            // Fallback to current directory if HOME is not set
            return SDM_STATE_FILENAME;
        }

        std::string stateDirectory = std::string(home) + "/." + SDM_STATE_DIRECTORY;
#ifndef _WIN32
        mkdir(stateDirectory.c_str(), 0755); // Create directory if it doesn't exist
#endif
        return stateDirectory + "/" + SDM_STATE_FILENAME;
    }

    // Returns true if the download task is effectively complete (i.e., its progress is >= 99.9999)
    bool isTaskComplete(const std::shared_ptr<DownloadTask> &task)
    {
        return (task->getProgress() >= 99.9999);
    }

    int statusToInt(DownloadStatus s)
    {
        return static_cast<int>(s);
    }

    DownloadStatus intToStatus(int i)
    {
        switch (i)
        {
        case 0:
            return DownloadStatus::QUEUED;
        case 1:
            return DownloadStatus::ACTIVE;
        case 2:
            return DownloadStatus::PAUSED;
        case 3:
            return DownloadStatus::COMPLETED;
        case 4:
            return DownloadStatus::FAILED;
        default:
            return DownloadStatus::CANCELED;
        }
    }
}

// Initialises thread pool and loads saved download states
DownloadManager::DownloadManager()
    : _threadPool(5),
      _stateFilePath(getStateFilePath())
{
    loadState();
}

// Cleans up thread pool, pauses all downloads, and saves state
DownloadManager::~DownloadManager()
{
    _threadPool.~ThreadPool();
    pauseAllDownloads();
    saveState();
}

// Assigns a given task to the container matching its current status
void DownloadManager::addTaskToStatusContainer(std::shared_ptr<DownloadTask> task)
{
    switch (task->getStatus())
    {
    case DownloadStatus::QUEUED:
        _queued.push_back(task);
        break;
    case DownloadStatus::ACTIVE:
        _active.push_back(task);
        break;
    case DownloadStatus::PAUSED:
        _paused.push_back(task);
        break;
    case DownloadStatus::COMPLETED:
        _completed.push_back(task);
        break;
    case DownloadStatus::FAILED:
        _failed.push_back(task);
        break;
    default:
        break; // CANCELED tasks are not stored
    }
}

// Removes a given task from whichever container currently holds it
void DownloadManager::removeTaskFromCurrentContainer(std::shared_ptr<DownloadTask> task)
{
    auto removeTask = [&](std::vector<std::shared_ptr<DownloadTask>> &list)
    {
        auto it = std::find(list.begin(), list.end(), task);
        if (it != list.end())
        {
            list.erase(it);
        }
    };

    switch (task->getStatus())
    {
    case DownloadStatus::QUEUED:
        removeTask(_queued);
        break;
    case DownloadStatus::ACTIVE:
        removeTask(_active);
        break;
    case DownloadStatus::PAUSED:
        removeTask(_paused);
        break;
    case DownloadStatus::COMPLETED:
        removeTask(_completed);
        break;
    case DownloadStatus::FAILED:
        removeTask(_failed);
        break;
    default:
        break; // CANCELED tasks won't be in a container
    }
}

// Updates a task's status, removes it from its old container,
// Places it into the container of the new status (unless cancelled).
void DownloadManager::updateTaskStatus(std::shared_ptr<DownloadTask> task, DownloadStatus newStatus)
{
    removeTaskFromCurrentContainer(task);
    task->setStatus(newStatus);

    if (newStatus != DownloadStatus::CANCELED)
    {
        addTaskToStatusContainer(task);
    }

    saveState();
}

// Creates a new download task from the given URL and destination and adds it to the queued container
void DownloadManager::queueDownload(const std::string &url, const std::string &destination)
{
    auto task = std::make_shared<DownloadTask>(url);

    std::string resolvedDestination = destination;
    if (resolvedDestination.empty())
    {
        // If the user does not provide a destination, resolve it from the server
        resolvedDestination = http::resolveFilenameFromServer(*task);
    }

    if (task->getErrorCode() != CURLE_OK)
    {
        // If the server returned an error during filename resolution, mark the task as failed
        updateTaskStatus(task, DownloadStatus::FAILED);
        return;
    }

    task->setDestination(getUniqueFilename(resolvedDestination));
    _queued.push_back(task);

    saveState();
}

// Pauses an active download by index, moving it to the paused container
void DownloadManager::pauseDownload(size_t index)
{
    if (index >= _active.size())
        return;

    auto task = _active[index];
    updateTaskStatus(task, DownloadStatus::PAUSED);
}

// Resumes a paused download by index, moving it to the queued container
void DownloadManager::resumeDownload(size_t index)
{
    if (index >= _paused.size())
        return;

    auto task = _paused[index];
    task->resume();
    updateTaskStatus(task, DownloadStatus::QUEUED);
}

// Cancels an active download by index
void DownloadManager::cancelDownload(size_t index)
{
    if (index >= _active.size())
        return;

    auto task = _active[index];
    updateTaskStatus(task, DownloadStatus::CANCELED);
}

// Retries a failed download by index, moving it to the queued container
void DownloadManager::retryDownload(size_t index)
{
    if (index >= _failed.size())
        return;

    auto task = _failed[index];
    removeTaskFromCurrentContainer(task);
    queueDownload(task->getUrl(), task->getDestination());
}

// Pauses all active and queued downloads
void DownloadManager::pauseAllDownloads()
{
    // Drain _activeDownloads from the back
    while (!_active.empty())
    {
        auto &task = _active.back();
        updateTaskStatus(task, DownloadStatus::PAUSED);
    }

    // Drain _queuedDownloads from the back
    while (!_queued.empty())
    {
        auto &task = _queued.back();
        updateTaskStatus(task, DownloadStatus::PAUSED);
    }
}

// Resumes all paused downloads, moving them to the queued container
void DownloadManager::resumeAllDownloads()
{
    while (!_paused.empty())
    {
        auto &task = _paused.back();
        task->resume();
        updateTaskStatus(task, DownloadStatus::QUEUED);
    }
}

// Cancels all active or queued downloads
void DownloadManager::cancelAllDownloads()
{
    for (auto &task : _active)
    {
        updateTaskStatus(task, DownloadStatus::CANCELED);
    }
    for (auto &task : _queued)
    {
        updateTaskStatus(task, DownloadStatus::CANCELED);
    }
}

void DownloadManager::retryAllDownloads()
{
    while (!_failed.empty())
    {
        auto &task = _failed.back();
        removeTaskFromCurrentContainer(task);
        queueDownload(task->getUrl(), task->getDestination());
    }
}

// Updates the status of active tasks, moving them if needed
// Starts new tasks from the queue if possible
void DownloadManager::update()
{
    // Collect any tasks that are still active
    std::vector<std::shared_ptr<DownloadTask>> stillActive;
    stillActive.reserve(_active.size());

    for (auto &task : _active)
    {
        DownloadStatus status = task->getStatus();
        switch (status)
        {
        case DownloadStatus::ACTIVE:
            stillActive.push_back(task);
            break;
        case DownloadStatus::COMPLETED:
            updateTaskStatus(task, DownloadStatus::COMPLETED);
            break;
        case DownloadStatus::FAILED:
            updateTaskStatus(task, DownloadStatus::FAILED);
            break;
        default:
            break;
        }
    }
    // Overwrite _active with only the tasks still active
    _active = std::move(stillActive);

    // Start new tasks if there is room in the thread pool
    while (!_queued.empty() && _active.size() < _threadPool.size())
    {
        auto task = _queued.back();
        _queued.pop_back();
        _active.push_back(task);

        _threadPool.enqueue([task]()
                            { task->run(); });
    }

    saveState(); // Persist changes
}

// Clears all history of completed and failed downloads (removes them from their containers)
void DownloadManager::clearHistory()
{
    _completed.clear();
    _failed.clear();
    saveState();
}

//------------------------------------------------------------------------------
// Loading and saving download state from and to disk
//------------------------------------------------------------------------------

// Loads the download manager's state from _stateFilePath
// Creates tasks and places them in the appropriate containers
void DownloadManager::loadState()
{
    std::ifstream inFile(_stateFilePath);
    if (!inFile.is_open())
    {
        return; // No file => nothing to load
    }

    std::string line;
    while (std::getline(inFile, line))
    {
        if (line.empty()) 
            continue;

        std::istringstream iss(line);

        std::string url, destination;
        double downloadedBytes, totalBytes;
        int statusInt, httpStatus, errorCodeInt;
        time_t addedAt, endedAt;

        // Read a line of data for a single task
        if (!(iss >> std::quoted(url)
                  >> std::quoted(destination) // Enable reading strings with spaces
                  >> downloadedBytes
                  >> totalBytes
                  >> statusInt
                  >> httpStatus
                  >> errorCodeInt
                  >> addedAt
                  >> endedAt))
        {
            break; // Break on EOF or malformed data
        }

        auto task = std::make_shared<DownloadTask>(url);
        task->setDestination(destination);
        task->setBytesDownloaded(downloadedBytes);
        task->setTotalBytes(totalBytes);
        task->setStatus(static_cast<DownloadStatus>(statusInt));
        task->setHttpStatus(httpStatus);
        task->setErrorCode(static_cast<CURLcode>(errorCodeInt));
        task->setAddedAt(addedAt);
        task->setEndedAt(endedAt);

        addTaskToStatusContainer(task);
    }

    inFile.close();
}

// Saves the current download manager state (all containers) to _stateFilePath
void DownloadManager::saveState() const
{
    std::ofstream outFile(_stateFilePath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open())
    {
        return;
    }

    auto writeContainer = [&](const std::vector<std::shared_ptr<DownloadTask>> &container)
    {
        for (const auto &task : container)
        {
            outFile << std::quoted(task->getUrl()) << " "
                    << std::quoted(task->getDestination()) << " "
                    << task->getBytesDownloaded() << " "
                    << task->getTotalBytes() << " "
                    << statusToInt(task->getStatus()) << " "
                    << task->getHttpStatus() << " "
                    << task->getErrorCode() << " "
                    << task->getAddedAt() << " "
                    << task->getEndedAt() << "\n";
        }
    };

    // Write out each container in turn
    writeContainer(_queued);
    writeContainer(_active);
    writeContainer(_paused);
    writeContainer(_completed);
    writeContainer(_failed);

    outFile.close();
}