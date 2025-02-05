#include "core/DownloadApplication.hpp"
#include "core/DownloadManager.hpp"
#include "ui/UI.hpp"

DownloadApplication::DownloadApplication() {}
DownloadApplication::~DownloadApplication() {}

void DownloadApplication::run()
{
    DownloadManager manager;
    UI ui(manager);
    ui.run();
}