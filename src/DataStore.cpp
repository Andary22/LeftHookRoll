#include "../includes/DataStore.hpp"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


DataStore::DataStore(): _mode(RAM), _bufferLimit(/*BufferLimit*/), _currentSize(0), _dataBuffer(), _fileFd(-1), _absolutePath()
{
}

DataStore::~DataStore() {
	clear();
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
