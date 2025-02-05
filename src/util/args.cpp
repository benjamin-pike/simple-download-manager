#include <sstream>
#include <vector>
#include <string>

std::vector<std::string> extractArguments(const std::string &command, size_t maxArgs)
{
    std::istringstream iss(command);
    std::vector<std::string> parts;
    std::string part;
    std::string currentArg;
    bool isQuoted = false; // Allows for parsing of quoted arguments (e.g. filenames with spaces)

    iss >> part; // Skip the command token itself (e.g. "download", "pause")

    while (iss)
    {
        char c = iss.get();
        if (iss.eof()) break;

        if (c == '"')
        {
            isQuoted = !isQuoted;
        }
        else if (c == ' ' && !isQuoted)
        {
            if (!currentArg.empty())
            {
                parts.push_back(currentArg);
                currentArg.clear();
                if (parts.size() >= maxArgs) return parts;
            }
        }
        else
        {
            currentArg += c;
        }
    }

    if (!currentArg.empty() && parts.size() < maxArgs)
    {
        parts.push_back(currentArg);
    }

    return parts;
}
