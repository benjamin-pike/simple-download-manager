#ifndef DOWNLOADMANAGER_HPP
#define DOWNLOADMANAGER_HPP

#include <string>
#include <vector>
#include <memory>

#include "core/DownloadTask.hpp"
#include "aux/ThreadPool.hpp"

static constexpr const char SDM_STATE_DIRECTORY[] = "sdm";
static constexpr const char SDM_STATE_FILENAME[] = "downloads";

class DownloadManager
{
public:
    DownloadManager();
    ~DownloadManager();

    void queueDownload(const std::string &url, const std::string &destination);

    void update();

    void updateTaskStatus(std::shared_ptr<DownloadTask> task, DownloadStatus newStatus);

    void pauseDownload(size_t index);
    void resumeDownload(size_t index);
    void cancelDownload(size_t index);
    void retryDownload(size_t index);
    void pauseAllDownloads();
    void resumeAllDownloads();
    void cancelAllDownloads();
    void retryAllDownloads();

    void clearHistory();

    const std::vector<std::shared_ptr<DownloadTask>> &getQueued() const { return _queued; }
    const std::vector<std::shared_ptr<DownloadTask>> &getActive() const { return _active; }
    const std::vector<std::shared_ptr<DownloadTask>> &getPaused() const { return _paused; }
    const std::vector<std::shared_ptr<DownloadTask>> &getCompleted() const { return _completed; }
    const std::vector<std::shared_ptr<DownloadTask>> &getFailed() const { return _failed; }

private:
    ThreadPool _threadPool;
    std::string _stateFilePath;

    std::vector<std::shared_ptr<DownloadTask>> _queued;
    std::vector<std::shared_ptr<DownloadTask>> _active;
    std::vector<std::shared_ptr<DownloadTask>> _paused;
    std::vector<std::shared_ptr<DownloadTask>> _completed;
    std::vector<std::shared_ptr<DownloadTask>> _failed;

    void loadState();
    void saveState() const;

    void addTaskToStatusContainer(std::shared_ptr<DownloadTask> task);
    void removeTaskFromCurrentContainer(std::shared_ptr<DownloadTask> task);
};

#endif