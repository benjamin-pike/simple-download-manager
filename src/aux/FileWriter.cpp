#include <iostream>

#include "aux/FileWriter.hpp"

FileWriter::FileWriter(const std::string& fp, bool isAppendMode)
{
    std::ios::openmode mode = std::ios::binary; // Open file in binary mode
    if (isAppendMode) {
        mode |= std::ios::app; // Append bytes to end of file
    } else {
        mode |= std::ios::trunc; // Overwrite existing file
    }

    _out.open(fp, mode); // Open file for writing
}

FileWriter::~FileWriter()
{
    // Close file in destructor if still open
    if (_out.is_open()) {
        _out.close();
    }
}

bool FileWriter::isOpen() const
{
    return _out.is_open();
}

void FileWriter::write(const char* data, size_t size)
{
    if (_out.is_open()) {
        // Write data to file stream
        _out.write(data, static_cast<std::streamsize>(size));
    }
}