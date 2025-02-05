#ifndef UI_HPP
#define UI_HPP

#include <curses.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

#include "ui/Screen.hpp"
#include "core/DownloadManager.hpp"

static constexpr int LEFT_PADDING = 2;
static constexpr int BAR_WIDTH = 32;

class UI
{
public:
    explicit UI(DownloadManager &manager);

    void run();
    void stop();
    void changeScreen(ScreenType newScreen);

private:
    DownloadManager &_manager;
    bool _isRunning;
    std::string _commandBuffer;
    std::chrono::steady_clock::time_point _lastFullUpdateTime;
    std::unique_ptr<Screen> _screen;

    WINDOW *_headerWin = nullptr;
    WINDOW *_cmdLineWin = nullptr;
    WINDOW *_bodyPad = nullptr;

    int _cmdLineHeight = 0;
    int _headerHeight = 0;
    int _padHeight = 0;
    int _padWidth = 0;
    int _scrollOffset = 0;
    int _maxContentHeight = 0;

    void initialiseCurses();
    void cleanupCurses();
    void createWindows();
    void destroyWindows();

    void processInput();
    void handleKeyPress(int ch);
    void handleCommand(const std::string &command);
    void updateScreen(bool immediate = false);

    void drawFullScreen();
    void drawHeader();
    void drawBody();
    void drawMainContent();
    void drawCommandLine();
    void clearScreen();
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);

    void sleepBriefly(int intervalMs);
};

#endif