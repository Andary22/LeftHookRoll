#include "../includes/DataStore.hpp"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>



DataStore::DataStore(): _mode(RAM), _bufferLimit(BufferLimit), _currentSize(0), _dataBuffer(), _fileFd(-1), _absolutePath()
{
}

DataStore::~DataStore() {
	clear();
}

void DataStore::append(const char* data, size_t length) {
	if (data == NULL || length == 0) {
		return;
	}

	if (_mode == RAM) {
		if (_currentSize + length > _bufferLimit) {
			_switchToFileMode();
            write_to_fd(_fileFd, data, length);
			_currentSize += length;
		}
        else {
            _dataBuffer.insert(_dataBuffer.end(), data, data + length);
            _currentSize += length;
        }
	}
    else {
		write_to_fd(_fileFd, data, length);
		_currentSize += length;
	}

}

void DataStore::append(const std::string& data) {
	append(data.c_str(), data.size());
}

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

std::string DataStore::_generateTempFileName() const {
    static int file_cont = 0;
    std::ostringstream ss;
    ss << file_cont;
    return std::string(FILEPREFIX) + ss.str();
}


