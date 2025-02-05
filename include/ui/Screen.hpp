#ifndef SCREEN_HPP
#define SCREEN_HPP

#include <curses.h>
#include <string>
#include <vector>
#include <functional>

#include "core/DownloadManager.hpp"

class UI;

enum class MatchType
{
    EXACT,
    PREFIX
};

struct CommandEntry
{
    std::vector<std::string> commands;
    MatchType matchType;
    std::function<void(const std::string &)> action;
};

enum class ScreenType
{
    ACTIVE,
    HISTORY
};

class Screen
{
public:
    explicit Screen(DownloadManager &manager, UI &ui) : _manager(manager), _ui(ui) {}
    virtual ~Screen() = default;

    virtual std::vector<CommandEntry> getCommandTable() const = 0;
    virtual void drawAvailableCommands(int &currentRow, WINDOW *window) = 0;
    virtual void drawScreen(int &currentRow, WINDOW *window) = 0;

protected:
    DownloadManager &_manager;
    UI &_ui;

private:
    const std::vector<CommandEntry> _commandTable;
};

#endif