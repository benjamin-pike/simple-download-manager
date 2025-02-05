#ifndef DOWNLOADFILENAMERESOLVER_HPP
#define DOWNLOADFILENAMERESOLVER_HPP

#include <string>

#include "core/DownloadTask.hpp"

static constexpr char DEFAULT_FILENAME[] = "downloaded_file";

namespace http
{

    std::string resolveFilenameFromServer(DownloadTask &task);
}

#endif