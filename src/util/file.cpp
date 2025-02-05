#include <sys/stat.h>
#include <cstdio>
#include <string>

#include "util/file.hpp"

// Checks if a file exists at the given path
bool fileExists(const std::string &path)
{
    struct stat buf;
    return (stat(path.c_str(), &buf) == 0);
}

// Generates a unique filename based on the original path
std::string getUniqueFilename(const std::string &originalPath)
{
    // If the file doesn't exist, return the original filename
    if (!fileExists(originalPath))
        return originalPath;

    std::string base = originalPath;
    std::string extension;
    const std::size_t dotPos = originalPath.find_last_of('.');

    // If a dot is found, separate the base name and extension
    if (dotPos != std::string::npos)
    {
        base = originalPath.substr(0, dotPos);   // Extract base name
        extension = originalPath.substr(dotPos); // Extract extension (including dot)
    }

    if (base.empty()) // Handle cases where the base name is empty (e.g. .hiddenfile)
    {
        base = originalPath;
        extension.clear();
    }

    int counter = 1; // Counter to append to the filename if it already exists
    while (true)
    {
        // Generate a candidate filename with __<counter> appended to the base name
        std::string candidate = base + "__" + std::to_string(counter) + extension;

        if (!fileExists(candidate))
            return candidate;

        ++counter;
    }

    return originalPath; // Unreachable
}