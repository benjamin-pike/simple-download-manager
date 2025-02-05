# Simple Download Manager

## Overview
Simple Download Manager (SDM) is a terminal-based application for fetching files over HTTP. It uses multithreading to manage multiple downloads simultaneously and provides a text-based user interface (TUI) using Curses, allowing users to multitask by adding and inspecting downloads whilst others are in progress. SDM is stateful; users can exit the program and later resume downloads, retry failed downloads, and selectively pause or resume ongoing downloads. It is designed as such to enable reliable download handling in unpredictable network conditions or interrupted sessions. Errors are effectively categorised with clear error messages to facilitate quick diagnosis and issue resolution. The project is written in C++ and uses the CURL library for HTTP requests, the Curses library for the TUI, and POSIX threads for multithreading support.

## Features
- Multi-threaded downloading using a thread pool.
- Support for large file downloads via HTTP.
- Command-line interface with arguments for download management.
- A simple TUI using the Curses library.
- Ability to queue multiple downloads.
- Graceful shutdown handling to ensure no corrupted downloads.

## Dependencies
The project requires the following dependencies:
- **CMake** (version 3.15 or later)
- **C++17**
- **CURL** (for handling HTTP requests)
- **Curses** (for the terminal-based UI)
- **POSIX Threads** (for multithreading support)

Ensure these dependencies are installed before proceeding with the build.

## Project Structure
The project is structured as follows:
```
├── include/
│   ├── core/       # Core functionality (DownloadManager, DownloadTask)
│   ├── aux/        # Auxiliary components (ThreadPool, FileWriter)
│   ├── ui/         # UI-related components (ActiveScreen, HistoryScreen)
│   ├── util/       # Utility functions (formatting, arguments parsing, filename resolution)
├── scripts/
│   ├── build.sh    # Builds the project using CMake
│   ├── launch.sh   # Wrapper script for building and running the program
│   ├── run.sh      # Executes the compiled program
├── src/
│   ├── core/
│   ├── aux/
│   ├── ui/
│   ├── util/
│   ├── main.cpp    # Entry point of the application
├── CMakeLists.txt  # Configuration
└── README.md
```

## How the Program Works
### Lifecycle of a Download Task
1. Initiates a download task by providing a URL and an optional filename.
2. The **DownloadManager** assigns the task to the **ThreadPool**, where available worker threads pick up the job.
3. The **DownloadTask** fetches the file via HTTP, using **CURL**.
4. Data is streamed and written to disk using the **FileWriter**.
5. The UI updates the progress in real time.
6. Upon completion, the task is moved to the completed downloads list.
7. If the process is interrupted, partially downloaded files are handled appropriately.

### Thread Capacity
- By default, the number of threads is set to 5.
- Each thread handles one download task at a time.
- When all threads are busy, new tasks are queued until a thread is free.

### Available Commands
- *NB.* All commands can be abbreviated to the first letter (e.g. `d` for `download`).
#### Main Screen
- `download <url> [file]`: Add a new download task, specifying the URL and optional filename.
  - Example: `download https://example.com/file.zip "my_file.zip"`
- `pause [index]`: Pause an active download task by index (omit index to pause all).
- `resume [index]`: Resume a paused download task by index (omit index to resume all).
- `cancel [index]`: Cancel an active download task by index (omit index to cancel all).
- `history`: View completed and failed downloads.
- `quit`: Exit the program.
#### History Screen
- `retry [index]`: Retry a failed download task by index (omit index to retry all).
- `clear`: Clear the history of completed and failed downloads.
- `back`: Return to the main screen.

### Example
```

  SDM - Simple Download Manager

  Commands:
    download <URL> [file] | Start a new download
    pause [index]         | Pause a download
    resume [index]        | Resume a paused download
    cancel [index]        | Cancel an active download
    history               | Show past downloads (4|3)
    exit                  | Quit the program

  Active Downloads: 1
   1) https://example.com/test.zip -> test1.zip
  [==================>             ] 60.0% (600.0 MB / 1.00 GB) ETA: 1m 38s @ 4.1 MB/s
   
   2) https://example.com/test.zip -> test2.zip
  [============>                   ] 40.0% (400.0 MB / 1.00 GB) ETA: 3m 42s @ 2.7 MB/s

  Paused Downloads: 1
   1) https://example.com/test.zip -> test3.zip
  [======|                         ] 20.0% (200.0 MB / 1.00 GB)

  Failed Downloads: 1

```

## Compilation and Installation
To compile and run the project, run:
```sh
./launch.sh
```
The compiled executable will be found in the `build/` directory.

## Licence
This project is open-source under the MIT Licence.