#include <curl/curl.h>
#include <iostream>
#include <sys/stat.h>
#include <cstdio>
#include <chrono>
#include <deque>

#include "core/DownloadTask.hpp"
#include "aux/FileWriter.hpp"

namespace
{
    // Writes incoming data from libcurl to the destination file
    size_t curlWriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
    {
        auto *writer = static_cast<FileWriter *>(userdata);
        if (!writer)
        {
            return 0;
        }

        size_t totalBytes = size * nmemb;
        writer->write(static_cast<const char *>(ptr), totalBytes);
        return totalBytes; // Return number of bytes written
    }

    // Monitors download progress, checks pause/cancel state, and updates progress
    int curlProgressCallback(void *clientp,
                             curl_off_t dltotal,
                             curl_off_t dlnow,
                             curl_off_t /* ultotal */,
                             curl_off_t /* ulnow */)
    {
        auto *task = static_cast<DownloadTask *>(clientp);
        if (!task)
        {
            return 1;
        }

        // Abort if paused or cancelled
        if (task->isPaused() || task->isCanceled())
        {
            return 1;
        }

        // Include any previously downloaded amount (resume offset)
        double resumeOffset = task->getResumeOffset();
        double downloadedBytesSoFar = static_cast<double>(dlnow) + resumeOffset;
        double totalBytesSoFar = static_cast<double>(dltotal) + resumeOffset;

        // Update total bytes if the server reports a larger size
        if (totalBytesSoFar > 0.0 && totalBytesSoFar > task->getTotalBytes())
        {
            task->setTotalBytes(totalBytesSoFar);
        }

        // If total is known, calculate percentage
        double knownTotal = task->getTotalBytes();
        if (knownTotal > 0.0)
        {
            double percentage = (downloadedBytesSoFar / knownTotal) * 100.0;
            if (percentage > 100.0)
            {
                percentage = 100.0;
            }
            task->setProgress(percentage);
        }

        // Update downloaded bytes and record speed
        task->setBytesDownloaded(downloadedBytesSoFar);
        task->recordSpeedSample(std::time(nullptr), downloadedBytesSoFar);

        return 0;
    }

}

DownloadTask::DownloadTask(const std::string &url) : _url(url) {}

// Starts the download process, handles curl initialisation and clean-up
void DownloadTask::run()
{
    // Skip if task is active or already completed
    if (_status == DownloadStatus::COMPLETED || _status == DownloadStatus::ACTIVE)
    {
        return;
    }

    _startTime = std::chrono::steady_clock::now();
    _status = DownloadStatus::ACTIVE;

    CURL *curlHandle = curl_easy_init();
    if (!curlHandle)
    {
        onDownloadError(CURLE_FAILED_INIT); // Failed to initialise curl
        return;
    }

    // Open file (append if resuming, otherwise overwrite)
    FileWriter writer(_destination, _resumeEnabled);
    if (!writer.isOpen())
    {
        // If file cannot be opened, clean up and report error
        curl_easy_cleanup(curlHandle);
        onDownloadError(CURLE_WRITE_ERROR);
        return;
    }

    curl_easy_setopt(curlHandle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &writer);
    curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 0L); // Disable progress meter
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFODATA, this); // Pass this task as client data
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

    // Attempt resume if requested
    if (_resumeEnabled)
    {
        configureResume(curlHandle);
    }

    // Perform the download
    CURLcode res = curl_easy_perform(curlHandle);
    // Get HTTP status code and store it in the task
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &_httpStatus);

    // Check outcome
    if (res == CURLE_OK)
    {
        onDownloadComplete();
    }
    else
    {
        if (isPaused())
        {
            onDownloadPause();
        }
        else if (isCanceled())
        {
            onDownloadCancel();
        }
        else
        {
            onDownloadError(res);
        }
    }

    curl_easy_cleanup(curlHandle); // Clean up curl handle
}

//---------------------------------------------------------------------------------
// Task State Control
//---------------------------------------------------------------------------------

bool DownloadTask::isPaused() const
{
    return _status == DownloadStatus::PAUSED;
}

bool DownloadTask::isFailed() const
{
    return _status == DownloadStatus::FAILED;
}

bool DownloadTask::isCanceled() const
{
    return _status == DownloadStatus::CANCELED;
}

void DownloadTask::onDownloadPause()
{
    _status = DownloadStatus::PAUSED;
}

void DownloadTask::onDownloadCancel()
{
    _status = DownloadStatus::CANCELED;
    std::remove(_destination.c_str());
}

void DownloadTask::onDownloadComplete()
{
    _status = DownloadStatus::COMPLETED;
    _progress.store(100.0);
    _endedAt = std::time(nullptr);
}

void DownloadTask::onDownloadError(const CURLcode errorCode)
{
    _errorCode = errorCode;
    _status = DownloadStatus::FAILED;
    _endedAt = std::time(nullptr); // Current time
}

void DownloadTask::resume()
{
    _resumeEnabled.store(true);
}

void DownloadTask::configureResume(CURL *curlHandle)
{
    // Retrieve file information (e.g., size) for the destination
    struct stat fileStat{};
    if (stat(_destination.c_str(), &fileStat) == 0)
    {
        // If there is a valid file and its size is greater than zero
        curl_off_t resumeFrom = fileStat.st_size;
        if (resumeFrom > 0)
        {
            // Tell libcurl to resume the download at this file size
            curl_easy_setopt(curlHandle, CURLOPT_RESUME_FROM_LARGE, resumeFrom);

            _resumeOffset = resumeFrom;
            setBytesDownloaded(static_cast<double>(resumeFrom));

            // If the total size is known, recalculate the approximate progress
            double tot = getTotalBytes();
            if (tot > 0.0 && resumeFrom < tot)
            {
                double pct = (static_cast<double>(resumeFrom) / tot) * 100.0;
                setProgress(pct);
            }
        }
    }
}

//---------------------------------------------------------------------------------
// Speed calculation
//---------------------------------------------------------------------------------

void DownloadTask::recordSpeedSample(time_t timestamp, double bytesDownloaded)
{
    // Add a new sample (current time, bytes) to the speed list
    _speedSamples.push_back({timestamp, bytesDownloaded});

    // Remove samples older than 10 seconds from the front
    double cutoff = static_cast<double>(timestamp) - 10.0;
    while (!_speedSamples.empty() && static_cast<double>(_speedSamples.front().first) < cutoff)
    {
        _speedSamples.pop_front();
    }
}

double DownloadTask::calcEstimatedTimeRemaining() const
{
    double total = getTotalBytes();
    double downloaded = getBytesDownloaded();

    // If not actively downloading, none downloaded, already finished, or not enough data for calculation, return -1
    if (_status != DownloadStatus::ACTIVE || downloaded <= 0.0 || downloaded >= total || _speedSamples.size() < 2)
    {
        return -1.0;
    }

    // Calculate time difference and downloaded bytes difference between first and last samples
    double t0 = static_cast<double>(_speedSamples.front().first);
    double t1 = static_cast<double>(_speedSamples.back().first);
    double dl0 = _speedSamples.front().second;
    double dl1 = _speedSamples.back().second;

    double dt = t1 - t0;
    double deltaBytes = dl1 - dl0;

    // If the interval or byte difference is too small, return -1
    if (dt < 0.0001 || deltaBytes < 1.0)
    {
        return -1.0;
    }

    // Calculate current speed in bytes per second
    double speedBps = deltaBytes / dt;

    // Estimate time remaining based on the speed and bytes left
    double remaining = total - downloaded;
    double secsLeft = remaining / speedBps;
    return secsLeft;
}

double DownloadTask::calcCurrentSpeedBps() const
{
    // Need at least two samples for current speed calculation
    if (_speedSamples.size() < 2)
    {
        return 0.0;
    }

    // Calculate the speed using the first and last samples
    double t0 = static_cast<double>(_speedSamples.front().first);
    double t1 = static_cast<double>(_speedSamples.back().first);
    double dl0 = _speedSamples.front().second;
    double dl1 = _speedSamples.back().second;

    double dt = t1 - t0;
    double deltaBytes = dl1 - dl0;

    // If data is not valid for a speed calculation, return 0
    if (dt <= 0.0 || deltaBytes <= 0.0)
    {
        return 0.0;
    }

    return deltaBytes / dt; // Bytes per second
}