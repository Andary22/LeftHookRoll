/**
 * @file DataStore.cpp
 * @brief A DataStore.hpp implementation with some helping functions
 * 
 */

#include "../includes/DataStore.hpp"


/**
 * @brief Ensures all data is written to the file descriptor.
 */
bool DataStore::write_all(int fd, const char* data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = ::write(fd, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (written == 0) return false;
        offset += static_cast<size_t>(written);
    }
    return true;
}

/**
 * @brief Copies data directly from one FD to another using a buffer.
 */
bool DataStore::copy_fd_contents(int srcFd, int dstFd, size_t totalBytes) {
    static const size_t kChunkSize = 8192;
    std::vector<char> buffer(kChunkSize);
    size_t offset = 0;
    while (offset < totalBytes) {
        size_t toRead = std::min(kChunkSize, totalBytes - offset);
        ssize_t readBytes = ::pread(srcFd, &buffer[0], toRead, static_cast<off_t>(offset));
        
        if (readBytes < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (readBytes == 0) return false;

        if (!write_all(dstFd, &buffer[0], static_cast<size_t>(readBytes))) {
            return false;
        }
        offset += static_cast<size_t>(readBytes);
    }
    return true;
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
		if (!_switchToFileMode()) {
            throw std::runtime_error("DataStore: Buffer limit exceeded and failed to create temp file.");
        }
		if (_currentSize > 0) {
			if (!copy_fd_contents(other._fileFd, _fileFd, _currentSize)) {
				clear();
			}
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
		if (!_switchToFileMode()) {
            throw std::runtime_error("DataStore: Buffer limit exceeded and failed to create temp file.");
        }
		if (_currentSize > 0) {
			if (!copy_fd_contents(other._fileFd, _fileFd, _currentSize)) {
				clear();
			}
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
			if(!_switchToFileMode()) {
                throw std::runtime_error("DataStore: Buffer limit exceeded and failed to create temp file.");
            }
            if (write_all(_fileFd, data, length)) {
			    _currentSize += length;
		    }
            else {
                throw std::runtime_error("DataStore: write faile");
            }
		}
        else {
            _dataBuffer.insert(_dataBuffer.end(), data, data + length);
            _currentSize += length;
        }
	}
    else {
		if (write_all(_fileFd, data, length)) {
			_currentSize += length;
		}
        else {
            throw std::runtime_error("DataStore: write faile");
        }
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
bool DataStore::_switchToFileMode() {
	if (_mode == FILE_MODE) {
		return true;
	}

	std::string templatePath = _generateTempFileName();
	std::vector<char> pathBuffer(templatePath.begin(), templatePath.end());
	pathBuffer.push_back('\0');

	int fd = ::mkstemp(&pathBuffer[0]);
	if (fd == -1) {
		return false;
	}

	_absolutePath = &pathBuffer[0];
	::unlink(_absolutePath.c_str());

	if (!_dataBuffer.empty()) {
		if (!write_all(fd, &_dataBuffer[0], _dataBuffer.size())) {
			::close(fd);
			_absolutePath.clear();
			return false;
		}
		_dataBuffer.clear();
	}

	_fileFd = fd;
	_mode = FILE_MODE;
    return true;
}

/**
 * @brief Generates a unique temporary filename (e.g., FILEPREFIX_XXXXXX).
     */
std::string DataStore::_generateTempFileName() const {
    static int file_conter = 0;
    std::stringstream ss;
    ss << file_conter;
    file_conter++;
    return std::string(FILEPREFIX) + ss.str(); 
}
