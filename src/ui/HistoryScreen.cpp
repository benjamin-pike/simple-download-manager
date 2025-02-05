#include <curses.h>
#include <algorithm>
#include <string>
#include <vector>

#include "ui/HistoryScreen.hpp"
#include "ui/UI.hpp"
#include "util/format.hpp"
#include "util/args.hpp"

HistoryScreen::HistoryScreen(DownloadManager &manager, UI &ui)
    : Screen(manager, ui),
      _commandTable{
          {{"retry", "r"},
           MatchType::PREFIX,
           [this](const std::string &command)
           {
               parseRetryCommand(command);
               _ui.changeScreen(ScreenType::ACTIVE);
           }},
          {{"clear", "c"},
           MatchType::PREFIX,
           [this](const std::string & /*command*/)
           {
               _manager.clearHistory();
           }},
          {{"back", "b", ""},
           MatchType::EXACT,
           [this](const std::string & /*unused*/)
           {
               _ui.changeScreen(ScreenType::ACTIVE);
           }}}
{
}

void HistoryScreen::drawAvailableCommands(int &currentRow, WINDOW *win)
{
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "retry [index] | Retry a failed download");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "clear         | Clear download history");
    mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "back          | Return to active downloads (%zu|%zu|%zu)",
              _manager.getActive().size(), _manager.getQueued().size(), _manager.getPaused().size());
}

void HistoryScreen::drawScreen(int &currentRow, WINDOW *win)
{
    auto &completed = _manager.getCompleted();
    auto &failed = _manager.getFailed();

    // Completed downloads
    if (completed.empty())
    {
        mvwprintw(win, currentRow, LEFT_PADDING, "Completed downloads: None");
    }
    else
    {
        mvwprintw(win, currentRow, LEFT_PADDING, "Completed Downloads: %zu", completed.size());
        for (size_t i = 0; i < completed.size(); ++i)
        {
            auto task = completed[i];
            // <index>) <time> - <url>
            mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "%zu) %s - %s",
                      i + 1,
                      formatTime(task->getEndedAt()).c_str(),
                      task->getUrl().c_str());
            // Saved to <destination> (<size>)
            mvwprintw(win, ++currentRow, LEFT_PADDING + 3, "Saved to %s (%s)",
                      task->getDestination().c_str(),
                      formatBytes(task->getBytesDownloaded()).c_str());
        }
    }

    // Failed downloads
    if (!failed.empty())
    {
        mvwprintw(win, currentRow += 2, LEFT_PADDING, "Failed Downloads: %zu", failed.size());
        for (size_t i = 0; i < failed.size(); ++i)
        {
            auto task = failed[i];
            // <index>) <time> - <url>
            mvwprintw(win, ++currentRow, LEFT_PADDING + 2, "%zu) %s - %s",
                      i + 1,
                      formatTime(task->getEndedAt()).c_str(),
                      task->getUrl().c_str());
            // E-<curl code>-<http code>: <message>
            mvwprintw(win, ++currentRow, LEFT_PADDING + 3, "E-%02d-%03d: %s",
                      task->getErrorCode(),
                      task->getHttpStatus(),
                      task->getErrorMessage().c_str());
        }
    }
}

// ------------------------------------------------------------------------------
// Private methods
// ------------------------------------------------------------------------------

void HistoryScreen::parseRetryCommand(const std::string &command)
{
    auto args = extractArguments(command, 1);
    if (args.empty())
    {
        // No index provided, retry all
        _manager.retryAllDownloads();
    }
    else
    {
        _manager.retryDownload(std::stoul(args[0]) - 1);
    }
}