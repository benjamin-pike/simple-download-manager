#ifndef ACTIVE_SCREEN_HPP
#define ACTIVE_SCREEN_HPP

#include "ui/Screen.hpp"
#include "ui/UI.hpp"

class ActiveScreen : public Screen
{
public:
    explicit ActiveScreen(DownloadManager &manager, UI &ui);

    std::vector<CommandEntry> getCommandTable() const override { return _commandTable; } 
    void drawAvailableCommands(int &currentRow, WINDOW *window) override;
    void drawScreen(int &currentRow, WINDOW *window) override;

private:
    const std::vector<CommandEntry> _commandTable;

    void parseDownloadCommand(const std::string &command);
    void parsePauseCommand(const std::string &command);
    void parseResumeCommand(const std::string &command);
    void parseCancelCommand(const std::string &command);
    void drawDownloadProgress(int &currentRow,
                              WINDOW *win,
                              size_t index,
                              const std::shared_ptr<DownloadTask> &task,
                              bool isActive);
};

#endif