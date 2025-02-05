#ifndef FILEWRITER_HPP
#define FILEWRITER_HPP

#include <string>
#include <fstream>

class FileWriter
{
public:
    FileWriter(const std::string& filePath, bool isAppendMode);
    ~FileWriter();

    bool isOpen() const;
    void write(const char* data, size_t size);

private:
    std::ofstream _out;
};

#endif