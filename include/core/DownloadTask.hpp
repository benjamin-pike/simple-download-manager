#ifndef DOWNLOADTASK_HPP
#define DOWNLOADTASK_HPP

#include <string>
#include <deque>
#include <atomic>
#include <chrono>
#include <curl/curl.h>

enum class DownloadStatus
{
    QUEUED,
    ACTIVE,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELED
};

class DownloadTask
{
public:
    DownloadTask(const std::string &url);

    void run();
    void resume();

    bool isPaused() const;
    bool isFailed() const;
    bool isCanceled() const;

    void recordSpeedSample(time_t time, double bytesDownloaded);
    double calcEstimatedTimeRemaining() const;
    double calcCurrentSpeedBps() const;

    std::string getUrl() const { return _url; }
    std::string getDestination() const { return _destination; }
    time_t getAddedAt() const { return _addedAt; }
    time_t getEndedAt() const { return _endedAt; }
    double getTotalBytes() const { return _totalBytes.load(); }
    double getBytesDownloaded() const { return _bytesDownloaded.load(); }
    double getProgress() const { return _progress.load(); }
    double getResumeOffset() const { return _resumeOffset.load(); }
    DownloadStatus getStatus() const { return _status; }
    int getHttpStatus() const { return _httpStatus.load(); }
    CURLcode getErrorCode() const { return _errorCode; }
    std::string getErrorMessage() const { return curl_easy_strerror(_errorCode); }

    void setDestination(const std::string &dest) { _destination = dest; }
    void setAddedAt(time_t t) { _addedAt = t; }
    void setEndedAt(time_t t) { _endedAt = t; }
    void setTotalBytes(double d) { _totalBytes.store(d); }
    void setBytesDownloaded(double d) { _bytesDownloaded.store(d); }
    void setProgress(double p) { _progress.store(p); }
    void setStatus(DownloadStatus s) { _status = s; }
    void setHttpStatus(int status) { _httpStatus.store(status); }
    void setErrorCode(CURLcode code) { _errorCode = code; }


private:
    std::string _url;
    std::string _destination;
    time_t _addedAt{0};
    time_t _endedAt{0};
    std::atomic<double> _totalBytes{0.0};
    std::atomic<double> _bytesDownloaded{0.0};
    std::atomic<double> _progress{0.0};
    std::atomic<double> _resumeOffset{0.0};
    DownloadStatus _status{DownloadStatus::QUEUED};
    std::atomic<int> _httpStatus{0};
    CURLcode _errorCode{CURLE_OK};

    std::atomic<bool> _resumeEnabled{false};
    std::atomic<bool> _cancelRequested{false};
    std::chrono::steady_clock::time_point _startTime;
    std::deque<std::pair<time_t, double>> _speedSamples;

    void configureResume(CURL *curlHandle);

    void onDownloadPause();
    void onDownloadCancel();
    void onDownloadComplete();
    void onDownloadError(const CURLcode res);
};

#endif