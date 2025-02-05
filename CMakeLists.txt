cmake_minimum_required(VERSION 3.15)
project(SimpleDownloadManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(SimpleDownloadManager
    src/main.cpp
    src/core/DownloadApplication.cpp
    src/core/DownloadManager.cpp
    src/core/DownloadTask.cpp
    src/aux/ThreadPool.cpp
    src/aux/FileWriter.cpp
    src/ui/UI.cpp
    src/ui/ActiveScreen.cpp
    src/ui/HistoryScreen.cpp
    src/util/format.cpp
    src/util/args.cpp
    src/util/file.cpp
    src/util/http.cpp
)

target_include_directories(SimpleDownloadManager
    PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CURSES_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(SimpleDownloadManager
    PRIVATE
        ${CURL_LIBRARIES}
        ${CURSES_LIBRARIES}
        Threads::Threads
)
