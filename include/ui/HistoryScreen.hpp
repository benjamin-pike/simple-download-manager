#ifndef HISTORY_SCREEN_HPP
#define HISTORY_SCREEN_HPP

#include "ui/Screen.hpp"
#include "ui/UI.hpp"

class HistoryScreen : public Screen
{
public:
    explicit HistoryScreen(DownloadManager &manager, UI &ui);

    std::vector<CommandEntry> getCommandTable() const override { return _commandTable; } 
    void drawAvailableCommands(int &currentRow, WINDOW *window) override;
    void drawScreen(int &currentRow, WINDOW *window) override;

private:
    const std::vector<CommandEntry> _commandTable;

    void parseRetryCommand(const std::string &command);
};

#endif