/**
 * @file DataStore.cpp
 * @brief A DataStore.hpp implementation with some helping functions
 * 
 */

#include "../includes/DataStore.hpp"


/**
 * @brief Ensures all data is written to the file descriptor. Throws on any system error.
 */
void DataStore::write_all(int fd, const char* data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = ::write(fd, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("DataStore: write failed - ") + std::strerror(errno));
        }
        if (written == 0) {
            throw std::runtime_error("DataStore: write failed - 0 bytes written (possible disk full)");
        }
        offset += static_cast<size_t>(written);
    }
}

/**
 * @brief Copies data directly from one FD to another using a buffer. Throws on error.
 */
void DataStore::copy_fd_contents(const std::string& srcPath, int dstFd, size_t totalBytes) {
    int srcFd = ::open(srcPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        throw std::runtime_error(std::string("DataStore: open failed during copy - ") + std::strerror(errno));
    }
    static const size_t kChunkSize = 8192;
    std::vector<char> buffer(kChunkSize);
    size_t offset = 0;
    try {
        while (offset < totalBytes) {
            size_t toRead = std::min(kChunkSize, totalBytes - offset);
            ssize_t readBytes = ::read(srcFd, &buffer[0], toRead);
            
            if (readBytes < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("DataStore: read failed during copy - ") + std::strerror(errno));
            }
            if (readBytes == 0) {
                throw std::runtime_error("DataStore: copy failed - unexpected EOF (source truncated)");
            }

            write_all(dstFd, &buffer[0], static_cast<size_t>(readBytes));
            offset += static_cast<size_t>(readBytes);
        }
    } catch (...) {
        ::close(srcFd);
        throw; // Re-throw the exception after cleaning up the FD
    }
    
    ::close(srcFd);
}
// Canonical Form

DataStore::DataStore(): _mode(RAM), _bufferLimit(BUFFERLIMIT), _currentSize(0), _dataBuffer(), _fileFd(-1), _absolutePath()
{
}

DataStore::DataStore(const DataStore& other): _mode(RAM), _bufferLimit(other._bufferLimit), _currentSize(0), _dataBuffer(), _fileFd(-1), _absolutePath()
{
	if (other._mode == RAM) {
		_mode = RAM;
		_dataBuffer = other._dataBuffer;
		_currentSize = other._currentSize;
	}
    else {
		_mode = RAM;
		_dataBuffer.clear();
		_currentSize = other._currentSize;
		_switchToFileMode();
		if (_currentSize > 0) {
			copy_fd_contents(other._absolutePath, _fileFd, _currentSize);
		}
	}
}

DataStore& DataStore::operator=(const DataStore& other) {
	if (this == &other) {
		return *this;
	}

	clear();
	_bufferLimit = other._bufferLimit;

	if (other._mode == RAM) {
		_mode = RAM;
		_dataBuffer = other._dataBuffer;
		_currentSize = other._currentSize;
	}
    else {
		_mode = RAM;
		_dataBuffer.clear();
		_currentSize = other._currentSize;
		_switchToFileMode();
		if (_currentSize > 0) {
			copy_fd_contents(other._absolutePath, _fileFd, _currentSize);
		}
	}

	return *this;
}

DataStore::~DataStore() {
	clear();
}

//  Core Behavior

/**
 * @brief Appends raw byte data to the store.
 * Automatically transitions from RAM to FILE_MODE if _bufferLimit is exceeded.
 * @param data Pointer to the buffer to append.
 * @param length Number of bytes to append.
 */
void DataStore::append(const char* data, size_t length) {
	if (data == NULL || length == 0) {
		return;
	}

	if (_mode == RAM) {
		if (_currentSize + length > _bufferLimit) {
			_switchToFileMode();
            write_all(_fileFd, data, length);
			_currentSize += length;
		}
        else {
            _dataBuffer.insert(_dataBuffer.end(), data, data + length);
            _currentSize += length;
        }
	}
    else {
		write_all(_fileFd, data, length);
		_currentSize += length;
	}

}

/**
 * @brief Convenience overload to append a std::string directly.
 */
void DataStore::append(const std::string& data) {
	append(data.c_str(), data.size());
}

/**
 * @brief Resets the store, clears the vector, and closes the temp file descriptor.
 */
void DataStore::clear() {
	_dataBuffer.clear();
	_currentSize = 0;
	_mode = RAM;
	if (_fileFd != -1) {
		::close(_fileFd);
		_fileFd = -1;
	}
	_absolutePath.clear();
}

// Getters

BufferMode DataStore::getMode() const {
	return _mode;
}

const std::vector<char>& DataStore::getVector() const {
	return _dataBuffer;
}

int DataStore::getFd() const {
	return _fileFd;
}

size_t DataStore::getSize() const {
	return _currentSize;
}

/**
 * @brief Handles the transition from RAM to a temporary file on disk.
 * Uses the immediate unlink() trick to ensure OS-level cleanup on crashes.
 */
void DataStore::_switchToFileMode() {
    if (_mode == FILE_MODE) {
        return ;
    }

    static int file_counter = 0;
    _generateTempFileName();
    write_all(_fileFd, &_dataBuffer[0], _dataBuffer.size());
    _dataBuffer.clear();
    _mode = FILE_MODE;
}

/**
 * @brief Generates a unique temporary filename (e.g., FILEPREFIX_XXXXXX).
 */
void DataStore::_generateTempFileName() {
    static int file_counter = 0;
    int fd = -1;
    std::string currentName;

    while (fd == -1) {
        std::stringstream ss;
        ss << FILEPREFIX << file_counter++;
        currentName = ss.str();
        
        fd = ::open(currentName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        
        if (fd == -1) {
            if (errno == EEXIST) {
                continue; // File exists, loop again and try the next number!
            }
            throw std::runtime_error(std::string("DataStore: open failed - ") + std::strerror(errno));
        }
    }
    
    _fileFd = fd;
    _absolutePath = currentName;
}
