#include <curses.h>
#include <algorithm>
#include <string>
#include <vector>

#include "ui/ActiveScreen.hpp"
#include "ui/UI.hpp"
#include "util/format.hpp"
#include "util/args.hpp"

ActiveScreen::ActiveScreen(DownloadManager &manager, UI &ui)
    : Screen(manager, ui),
      _commandTable{
          {{"exit", "quit", "q"},
           MatchType::EXACT,
           [this](const std::string & /*unused*/)
           {
               _manager.pauseAllDownloads();
               _ui.stop();
           }},
          {{"download", "d"},
           MatchType::PREFIX,
           [this](const std::string &command)
           {
               parseDownloadCommand(command);
           }},
          {{"pause", "p"},
           MatchType::PREFIX,
           [this](const std::string &command)
           {
               parsePauseCommand(command);
           }},
          {{"resume", "r"},
           MatchType::PREFIX,
           [this](const std::string &command)
           {
               parseResumeCommand(command);
           }},
          {{"cancel", "c"},
           MatchType::PREFIX,
           [this](const std::string &command)
           {
               parseCancelCommand(command);
           }},
          {{"history", "h"},
           MatchType::EXACT,
           [this](const std::string & /*unused*/)
           {
               _ui.changeScreen(ScreenType::HISTORY);
           }}}
{
}

void ActiveScreen::drawAvailableCommands(int &currentRow, WINDOW *win)
{
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "download <URL> [file] | Start a new download");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "pause [index]         | Pause a download");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "resume [index]        | Resume a paused download");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "cancel [index]        | Cancel an active download");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "history               | Show past downloads (%zu|%zu)",
              _manager.getCompleted().size(), _manager.getFailed().size());
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "exit                  | Quit the program");
}

void ActiveScreen::drawScreen(int &currentRow, WINDOW *win)
{
    auto &active = _manager.getActive();
    auto &paused = _manager.getPaused();
    auto &queued = _manager.getQueued();
    auto &failed = _manager.getFailed();

    // Active downloads
    if (active.empty())
    {
        mvwprintw(win, currentRow, LEFT_PADDING, "Active Downloads: None");
    }
    else
    {
        mvwprintw(win, currentRow, LEFT_PADDING, "Active Downloads: %zu", active.size());
        for (size_t i = 0; i < active.size(); ++i)
        {
            drawDownloadProgress(++currentRow, win, i + 1, active[i], true);
        }
    }

    // Paused downloads
    if (!paused.empty())
    {
        mvwprintw(win, currentRow += 2, LEFT_PADDING, "Paused Downloads: %zu", paused.size());
        for (size_t i = 0; i < paused.size(); ++i)
        {
            drawDownloadProgress(++currentRow, win, i + 1, paused[i], false);
        }
    }

    // Queued downloads
    if (!queued.empty())
    {
        mvwprintw(win, currentRow += 2, LEFT_PADDING, "Queued Downloads: %zu", queued.size());
    }

    // Failed downloads
    if (!failed.empty())
    {
        mvwprintw(win, currentRow += 2, LEFT_PADDING, "Failed Downloads: %zu", failed.size());
    }
}

// ------------------------------------------------------------------------------
// Private methods
// ------------------------------------------------------------------------------

void ActiveScreen::parseDownloadCommand(const std::string &command)
{
    auto args = extractArguments(command, 2);
    if (args.empty())
        return;

    if (args.size() == 1)
        // No destination provided
        _manager.queueDownload(args[0], "");
    else
        _manager.queueDownload(args[0], args[1]);
}

void ActiveScreen::parsePauseCommand(const std::string &command)
{
    auto args = extractArguments(command, 1);
    if (args.empty())
        // No index provided, pause all
        _manager.pauseAllDownloads();
    else
        _manager.pauseDownload(std::stoul(args[0]) - 1);
}

void ActiveScreen::parseResumeCommand(const std::string &command)
{
    auto args = extractArguments(command, 1);
    if (args.empty())
        // No index provided, resume all
        _manager.resumeAllDownloads();
    else
        _manager.resumeDownload(std::stoul(args[0]) - 1);
}

void ActiveScreen::parseCancelCommand(const std::string &command)
{
    auto args = extractArguments(command, 1);
    if (args.empty())
        // No index provided, cancel all
        _manager.cancelAllDownloads();
    else
        _manager.cancelDownload(std::stoul(args[0]) - 1);
}

void ActiveScreen::drawDownloadProgress(int &currentRow,
                                        WINDOW *win,
                                        size_t index,
                                        const std::shared_ptr<DownloadTask> &task,
                                        bool isActive)
{
    // <index>) <url> -> <destination>
    mvwprintw(win, currentRow++, LEFT_PADDING + 1,
              "%zu) %s -> %s",
              index,
              task->getUrl().c_str(),
              task->getDestination().c_str());

    // Prepare progress bar
    double progress = task->getProgress();
    double bytesDownloaded = task->getBytesDownloaded();
    double totalBytes = task->getTotalBytes();
    int filled = 0;

    if (totalBytes > 0.1)
    {
        filled = static_cast<int>((bytesDownloaded / totalBytes) * BAR_WIDTH);
        filled = std::min(filled, BAR_WIDTH);
    }

    // [=======>   ] <progress>% (<currentBytes> MB / <totalBytes> MB) ETA: <time remaining> @ <speed>/s
    mvwaddstr(win, currentRow, 0, std::string(LEFT_PADDING, ' ').c_str());
    waddch(win, '[');
    for (int j = 0; j < BAR_WIDTH; ++j)
    {
        if (j < filled)
            waddch(win, '=');
        else if (j == filled)
            waddch(win, isActive ? '>' : '|'); 
        else
            waddch(win, ' ');
    }
    waddch(win, ']');

    // Print percentage progress
    wprintw(win, " %.1f%%", progress);

    // Print size info
    if (bytesDownloaded <= 0.0)
    {
        wprintw(win, " (size unknown)");
    }
    else
    {
        std::string currentStr = formatBytes(bytesDownloaded);
        std::string totalStr = formatBytes(totalBytes);
        wprintw(win, " (%s / %s)", currentStr.c_str(), totalStr.c_str());
    }

    // If active, show ETA and speed
    if (isActive)
    {
        double eta = task->calcEstimatedTimeRemaining();
        std::string speed = formatBytes(task->calcCurrentSpeedBps());

        if (eta > 0.0)
        {
            int sec = static_cast<int>(eta);
            int min = sec / 60;
            int seconds = sec % 60;
            wprintw(win, " ETA: %dm %ds @ %s/s", min, seconds, speed.c_str());
        }
        else
            wprintw(win, " ETA: -- @ %s/s", speed.c_str());
    }

    currentRow++;
}