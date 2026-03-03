#include "../includes/CGIManager.hpp"

// Canonical Form
CGIManager::CGIManager() : _pId(-1) {}

CGIManager::CGIManager(const CGIManager& other) : _pId(other._pId), _query(other._query), _scriptArgv(other._scriptArgv), _env(other._env), _execveEnvp(nullptr), _execveArgv(nullptr)
{
}

CGIManager& CGIManager::operator=(const CGIManager& other)
{
    if (this != &other)
    {
        _pId        = other._pId;
        _query      = other._query;
        _scriptArgv = other._scriptArgv;
        _env        = other._env;
        _execveEnvp = nullptr;
        _execveArgv = nullptr;
    }
    return *this;
}

CGIManager::~CGIManager()
{
    _freeExecveArrays();
    _closePipes();
}

pid_t CGIManager::getPid() const
{
    return _pId;
}

int CGIManager::getOutputFd() const
{
    return _outPipe[0];
}


