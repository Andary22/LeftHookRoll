/**
 * @file DataStore.hpp
 * @brief A DataStore is a flexible data structure, it is either a RAM buffer or a file on disk, depending on the size of the data.
 * you dont care about it's form, you just read/write from it using the helper functions provided.
 */

#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cerrno>

#define FILEPREFIX "/tmp/lefthookroll_"
#define BUFFERLIMIT 1024 * 1024

/**
 * @enum BufferMode
 * @brief Indicates where the data is currently being stored.
 */
enum BufferMode {
    RAM,
    FILE_MODE // FILE_MODE to avoid naming collisions with C standard FILE
};

class DataStore {
public:
    // Canonical Form
    DataStore();
    DataStore(const DataStore& other);
    DataStore& operator=(const DataStore& other);
    ~DataStore();

    //  Core Behavior

    /**
     * @brief Appends raw byte data to the store.
     * Automatically transitions from RAM to FILE_MODE if _bufferLimit is exceeded.
     * @param data Pointer to the buffer to append.
     * @param length Number of bytes to append.
     */
    void append(const char* data, size_t length);

    /**
     * @brief Convenience overload to append a std::string directly.
     */
    void append(const std::string& data);

    /**
     * @brief Resets the store, clears the vector, and closes the temp file descriptor.
     */
    void clear();

    //  Getters

    /**
     * @brief Returns the current storage mode (RAM or FILE_MODE).
     */
    BufferMode getMode() const;

    /**
     * @brief Returns a reference to the RAM buffer.
     * @warning Only valid if mode is RAM. should never use in normal operation, only for testing/debugging purposes.
     */
    const std::vector<char>& getVector() const;

    /**
     * @brief Returns the file descriptor of the temporary file.
     * @warning Only valid if mode is FILE_MODE. should never use in normal operation, only for testing/debugging purposes.
     */
    int getFd() const;

    /**
     * @brief Gets total bytes currently stored (whether in RAM or File).
     */
    size_t getSize() const;

private:
    //  Identity & State
    BufferMode          _mode;
    size_t              _bufferLimit;
    size_t              _currentSize;

    //  RAM Storage
    std::vector<char>   _dataBuffer;

    //  File Storage
    int                 _fileFd;
    std::string         _absolutePath;

    //  Private Helpers
    /**
     * @brief Handles the transition from RAM to a temporary file on disk.
     * Uses the immediate unlink() trick to ensure OS-level cleanup on crashes.
     */
    void _switchToFileMode();

    /**
     * @brief Generates a unique temporary filename (e.g., FILEPREFIX_XXXXXX).
     */
    void _generateTempFileName();

    /**
     * @brief Ensures all data is written to the file descriptor.
     */
    void write_all(int fd, const char* data, size_t length);

    /**
     * @brief Copies data directly from one FD to another using a buffer.
     */
    void copy_fd_contents(const std::string& srcPath, int dstFd, size_t totalBytes);
};