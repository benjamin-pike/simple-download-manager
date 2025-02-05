#include <sstream>
#include <stdio.h>
#include <time.h>
#include <iomanip>

#include "util/format.hpp"

std::string formatBytes(double bytes) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;

    std::ostringstream oss;
    oss << std::fixed;

    if (bytes < MB) {
        oss << std::setprecision(0) << (bytes / KB) << " KB";
    } else if (bytes < GB) {
        oss << std::setprecision(1) << (bytes / MB) << " MB";
    } else {
        oss << std::setprecision(2) << (bytes / GB) << " GB";
    }

    return oss.str();
}

std::string formatTime(time_t time) {
    char buffer[20];
    struct tm *timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), "%H:%M:%S %d/%m/%y", timeinfo);
    
    return std::string(buffer);
}