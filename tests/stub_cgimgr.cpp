#include "../includes/CGIManager.hpp"

CGIManager::CGIManager() : _pId(-1), _query(), _scriptArgv(), _env(), _execveEnvp(NULL), _execveArgv(NULL) {
    _outPipe[0] = -1;
    _outPipe[1] = -1;
}

CGIManager::CGIManager(const CGIManager& other)
    : _pId(other._pId), _query(other._query), _scriptArgv(other._scriptArgv),
      _env(other._env), _execveEnvp(NULL), _execveArgv(NULL) {
    _outPipe[0] = -1;
    _outPipe[1] = -1;
}

CGIManager& CGIManager::operator=(const CGIManager& other) {
    if (this != &other) {
        _pId       = other._pId;
        _query     = other._query;
        _scriptArgv = other._scriptArgv;
        _env       = other._env;
    }
    return *this;
}

CGIManager::~CGIManager() {}

pid_t  CGIManager::getPid()      const { return _pId; }
int    CGIManager::getOutputFd() const { return _outPipe[0]; }

void CGIManager::prepare(const Request&, const std::string&) {}
void CGIManager::execute(int) {}
bool CGIManager::isDone() { return true; }

void CGIManager::_buildEnvMap(const Request&, const std::string&) {}
void CGIManager::_prepExecveArrays() {}
void CGIManager::_freeExecveArrays() {}
void CGIManager::_closePipes() {}
