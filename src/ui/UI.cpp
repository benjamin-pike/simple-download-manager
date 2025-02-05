#include <curses.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>

#include "ui/UI.hpp"
#include "ui/ActiveScreen.hpp"
#include "ui/HistoryScreen.hpp"
#include "util/format.hpp"

// Constructs the UI object, setting up the command dispatch table and the active screen
UI::UI(DownloadManager &manager)
    : _manager(manager),
      _isRunning(true),
      _lastFullUpdateTime(std::chrono::steady_clock::now()),
      _screen(std::make_unique<ActiveScreen>(_manager, *this))
{
}

// Runs the main UI loop, initialising and cleaning up curses and drawing the full screen
void UI::run()
{
    initialiseCurses();
    createWindows();
    drawFullScreen();

    while (_isRunning)
    {
        processInput();
        updateScreen();
        sleepBriefly(10); // 10ms sleep between render
    }

    destroyWindows();
    cleanupCurses();
}

// Stops the UI loop
void UI::stop()
{
    _isRunning = false;
}

// Changes the current screen to the specified type
void UI::changeScreen(ScreenType newScreen)
{
    switch (newScreen)
    {
    case ScreenType::ACTIVE:
        _screen = std::make_unique<ActiveScreen>(_manager, *this);
        break;
    case ScreenType::HISTORY:
        _screen = std::make_unique<HistoryScreen>(_manager, *this);
        break;
    }

    _scrollOffset = 0; // Reset scroll offset
    drawFullScreen();
}

// ------------------------------------------------------------------------------
// Private methods
// ------------------------------------------------------------------------------

// Initialises curses and sets up the terminal
void UI::initialiseCurses()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); // Non-blocking getch
}

// Restores terminal state from curses mode
void UI::cleanupCurses()
{
    endwin(); // Restore terminal settings
}

// Creates the windows for the header, command line, and body
void UI::createWindows()
{
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);

    _headerHeight = 1;                                                    // Determined according to content
    _cmdLineHeight = 3;                                                   // Account for padding above and below
    _headerWin = newwin(_headerHeight, maxX, 0, 0);                       // Create header window, default to 1 row
    _cmdLineWin = newwin(_cmdLineHeight, maxX, maxY - _cmdLineHeight, 0); // Create command line window

    _padWidth = maxX;
    _padHeight = 1000;
    _bodyPad = newpad(_padHeight, _padWidth); // Create a pad for the screen body

    // Disable scrolling
    scrollok(_headerWin, FALSE);
    scrollok(_cmdLineWin, FALSE);
    scrollok(_bodyPad, FALSE);
}

// Destroys the windows created by createWindows
void UI::destroyWindows()
{
    if (_headerWin)
    {
        delwin(_headerWin);
        _headerWin = nullptr;
    }

    if (_cmdLineWin)
    {
        delwin(_cmdLineWin);
        _cmdLineWin = nullptr;
    }

    if (_bodyPad)
    {
        delwin(_bodyPad);
        _bodyPad = nullptr;
    }
}

// Checks for keyboard input and processes each keypress if available
void UI::processInput()
{
    int ch = getch();
    while (ch != ERR)
    {
        handleKeyPress(ch);
        ch = getch();
    }
}

// Interprets a single keypress to update or complete the command buffer
void UI::handleKeyPress(int ch)
{
    switch (ch)
    {
    case '\n':
    case '\r':
        handleCommand(_commandBuffer);
        _commandBuffer.clear();
        break;

    case KEY_BACKSPACE:
    case 127: // DEL
        if (!_commandBuffer.empty())
            _commandBuffer.pop_back();
        break;

    case KEY_UP: // Up arrow
        scrollUp();
        break;
    case KEY_DOWN: // Down arrow
        scrollDown();
        break;
    case KEY_PPAGE: // Page up
        scrollUp(5);
        break;
    case KEY_NPAGE: // Page down
        scrollDown(5);
        break;

    default:
        if (std::isprint(ch))
            // Add printable characters to the command buffer
            _commandBuffer.push_back(static_cast<char>(ch));
        break;
    }

    updateScreen(true);
}

// Matches the user input against the dispatch table and runs the corresponding action
void UI::handleCommand(const std::string &userInput)
{
    for (const auto &entry : _screen->getCommandTable())
    {
        for (const auto &alias : entry.commands)
        {
            bool match = false;
            switch (entry.matchType)
            {
            case MatchType::EXACT:
                if (userInput == alias)
                    match = true;
                break;
            case MatchType::PREFIX:
                if (userInput.rfind(alias, 0) == 0)
                    match = true;
                break;
            }
            if (match)
            {
                entry.action(userInput);
                break;
            }
        }
    }

    updateScreen(true);
}

// Periodically updates the screen (full or partial) based on elapsed time
void UI::updateScreen(bool immediate)
{
    auto now = std::chrono::steady_clock::now();
    double secondsSinceLastFullUpdate = std::chrono::duration<double>(now - _lastFullUpdateTime).count();

    // Redraw the entire interface every half second or immediately, if specified
    if (immediate || secondsSinceLastFullUpdate >= 0.5)
    {
        _manager.update();
        drawFullScreen();
        _lastFullUpdateTime = now;
    }
    else
    {
        drawCommandLine(); // Only update the command line
    }
}

 // Redraws the entire curses interface
void UI::drawFullScreen()
{
    clearScreen();

    drawHeader(); // Draw the header and dynamically update the header height

    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX); // Get current screen size

    mvwin(_headerWin, 0, 0);                  // Move the header window to the top
    wresize(_headerWin, _headerHeight, maxX); // Resize the header window
    wrefresh(_headerWin);                     // Refresh the header window

    _maxContentHeight = maxY - _headerHeight - _cmdLineHeight - 1;

    mvwin(_cmdLineWin, maxY - _cmdLineHeight, 0); // Move the command line window to the bottom
    wresize(_cmdLineWin, _cmdLineHeight, maxX);   // Resize the command line window
    wrefresh(_cmdLineWin);                        // Refresh the command line window

    drawBody();
    drawCommandLine();

    prefresh(
        _bodyPad,
        _scrollOffset,                     // Pad row to start reading
        0,                                 // Pad col to start reading
        _headerHeight,                     // Top alignment of the pad (below the header, with gap)
        0,                                 // Left alignment of the pad
        _headerHeight + _maxContentHeight, // Bottom alignment of the pad (above the command line)
        _padWidth - 1);                    // Right alignment of the pad
}

// Draws the header content and available commands
void UI::drawHeader()
{
    werase(_headerWin); // Clear the header window

    // Print the header content
    int currentRow = 0;
    mvwprintw(_headerWin, ++currentRow, LEFT_PADDING, "SDM - Simple Download Manager");
    mvwprintw(_headerWin, currentRow += 2, LEFT_PADDING, "Commands:");
    _screen->drawAvailableCommands(currentRow, _headerWin);

    wrefresh(_headerWin); // Refresh the header window

    _headerHeight = currentRow + 2;
}

// Draws the main content of the screen body
void UI::drawBody()
{
    // Draw the screen body content and calculate the pad height
    int currentRow = 0;
    _screen->drawScreen(currentRow, _bodyPad);
    _padHeight = std::max(currentRow + 1, _maxContentHeight);
}

// // Draws the command line section at the bottom of the screen
void UI::drawCommandLine()
{
    werase(_cmdLineWin); // Clear comand line window

    // Print the current command buffer and position the cursor
    mvwprintw(_cmdLineWin, 1, LEFT_PADDING, "> %s", _commandBuffer.c_str());
    wmove(_cmdLineWin, 1, LEFT_PADDING + 2 + (int)_commandBuffer.size());

    wrefresh(_cmdLineWin);
}

// Clears the screen windows
void UI::clearScreen()
{
    werase(_headerWin);
    werase(_cmdLineWin);
    werase(_bodyPad);
}

// Scrolls the screen body up by n lines
void UI::scrollUp(int lines)
{
    _scrollOffset = std::max(_scrollOffset - lines, 0);
}

// Scrolls the screen body down by n lines
void UI::scrollDown(int lines)
{
    int maxOffset = std::max(_padHeight - _maxContentHeight, 0);
    _scrollOffset = std::min(_scrollOffset + lines, maxOffset);
}

// Pauses execution briefly to prevent excessive CPU usage in the UI loop
void UI::sleepBriefly(int intervalMs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
}