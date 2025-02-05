#ifndef FILE_HPP
#define FILE_HPP

#include <string>

bool fileExists(const std::string &path);
std::string getUniqueFilename(const std::string &originalPath);

#endif