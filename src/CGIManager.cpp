#include "../includes/CGIManager.hpp"
#include "../includes/Request.hpp"
#include "../includes/AllowedMethods.hpp"
#include "../includes/FatalExceptions.hpp"

#include <sys/wait.h>
#include <csignal>
#include <ctime>
#include <cerrno>


//There's a zombie on your lawn...
std::set<pid_t> CGIManager::_activePids;
#ifndef MAX_ACTIVE_CGI_CHILDREN
# define MAX_ACTIVE_CGI_CHILDREN  64;
#endif

namespace CGIUtils
{
    void closeInheritedFds()
    {
        // 3 to skip the standard fds.(in,out,err)
        for (int fd = 3; fd < 1024; ++fd)
        {
            if (fcntl(fd, F_GETFD) != -1)
                close(fd);
        }
    }

    std::string resolveScriptDirectory(const std::string& scriptPath)
    {
        size_t slashPos = scriptPath.find_last_of('/');
        if (slashPos == std::string::npos)
            return ".";
        if (slashPos == 0)
            return "/";
        return scriptPath.substr(0, slashPos);
    }

    std::string resolveScriptExecArg(const std::string& scriptPath)
    {
        if (!scriptPath.empty() && scriptPath[0] == '/')
            return scriptPath;

        size_t slashPos = scriptPath.find_last_of('/');
        if (slashPos == std::string::npos)
            return scriptPath;
        return scriptPath.substr(slashPos + 1);
    }

    void prepareChildExecutionContext(const std::vector<std::string>& scriptArgv, char** execveArgv)
    {
        if (scriptArgv.empty())
            throw FatalException("CGI child fatal: missing script path");

        const std::string& scriptPath = scriptArgv.back();
        std::string scriptDir = resolveScriptDirectory(scriptPath);
        if (chdir(scriptDir.c_str()) == -1)
            throw FatalException("CGI child fatal: chdir() failed");

        std::string scriptExecArg = resolveScriptExecArg(scriptPath);
        if (scriptArgv.size() > 1)
            std::strcpy(execveArgv[1], scriptExecArg.c_str());
        else
            std::strcpy(execveArgv[0], scriptExecArg.c_str());
    }
}

// Canonical Form

CGIManager::CGIManager() : _pId(-1), _execveEnvp(NULL), _execveArgv(NULL)
{
    _outPipe[0] = -1;
    _outPipe[1] = -1;
}

CGIManager::CGIManager(const CGIManager& other): _pId(other._pId),  _query(other._query),  _scriptArgv(other._scriptArgv),  _env(other._env),  _execveEnvp(NULL),  _execveArgv(NULL)
{
    _outPipe[0] = -1;
    _outPipe[1] = -1;
}

CGIManager& CGIManager::operator=(const CGIManager& other)
{
    if (this != &other)
    {
        _freeExecveArrays();
        _closePipes();
        _pId        = other._pId;
        _query      = other._query;
        _scriptArgv = other._scriptArgv;
        _env        = other._env;
        _execveEnvp = NULL;
        _execveArgv = NULL;
        _outPipe[0] = -1;
        _outPipe[1] = -1;
    }
    return *this;
}

CGIManager::~CGIManager()
{
    _freeExecveArrays();
    _closePipes();
}

// Getters

pid_t CGIManager::getPid() const
{
    return _pId;
}

int CGIManager::getOutputFd() const
{
    return _outPipe[0];
}

// Public Behaviour

void CGIManager::prepare(const Request& request, const std::string& scriptPath, const std::string& interpreterOverride)
{
    _buildEnvMap(request, scriptPath);

    _scriptArgv.clear();
    std::string interp;
    if (!interpreterOverride.empty())
        interp = interpreterOverride;
    if (!interp.empty())
        _scriptArgv.push_back(interp);
    _scriptArgv.push_back(scriptPath);

    _prepExecveArrays();
}

void CGIManager::execute(int inputFd)
{
    if (_isSpawnLimitReached())
        throw ClientException(503, "CGIManager::execute: max active CGI children reached");

    if (pipe(_outPipe) == -1)
        throw ClientException(500, "CGIManager::execute: pipe() failed");

    _pId = fork();
    if (_pId < 0)
    {
        _closePipes();
        throw ClientException(500, "CGIManager::execute: fork() failed");
    }

    if (_pId == 0)
    {
        CGIUtils::prepareChildExecutionContext(_scriptArgv, _execveArgv);
        if (inputFd >= 0)
        {
            if (dup2(inputFd, STDIN_FILENO) == -1)
                throw FatalException("CGI child fatal: dup2(stdin) failed");
        }
        close(_outPipe[0]);
        if (dup2(_outPipe[1], STDOUT_FILENO) == -1)
            throw FatalException("CGI child fatal: dup2(stdout) failed");
        close(_outPipe[1]);

		CGIUtils::closeInheritedFds();

        execve(_execveArgv[0], _execveArgv, _execveEnvp);
        throw FatalException("CGI child fatal: execve() failed");
    }
    else
    {
        _registerPid(_pId);
        close(_outPipe[1]);
        _outPipe[1] = -1;
    }
}

bool CGIManager::isDone()
{
    if (_pId <= 0)
        return true;

    int status = 0;
    pid_t result = waitpid(_pId, &status, WNOHANG);
    if (result == 0)
        return false; // still running

    // Process has finished - unregister it
    _unregisterPid(_pId);
    _pId = -1;
    return true;
}

// ─── Private Helpers ───────────────────────────────────────────────────────

void CGIManager::_buildEnvMap(const Request& request, const std::string& scriptPath)
{
    _env.clear();

    _env["REQUEST_METHOD"]  = AllowedMethods::methodToString(request.getMethod());
    _env["QUERY_STRING"]    = request.getQuery();
    _env["SCRIPT_FILENAME"] = scriptPath;
    _env["SCRIPT_NAME"]     = request.getURL();
    _env["PATH_INFO"]       = request.getURL();
    _env["SERVER_PROTOCOL"] = request.getProtocol();
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["REDIRECT_STATUS"] = "200";

    // Content headers (only meaningful for POST)
    std::string ct = request.getHeader("content-type");
    if (!ct.empty())
        _env["CONTENT_TYPE"] = ct;

    std::string cl = request.getHeader("content-length");
    _env["CONTENT_LENGTH"] = (cl.empty()? _env["CONTENT_LENGTH"] = "0" : _env["CONTENT_LENGTH"] = cl);

    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        std::string key = it->first;
        for (size_t i = 0; i < key.size(); ++i)
        {
            if (key[i] == '-')
                key[i] = '_';
            else
                key[i] = std::toupper(key[i]);
        }
        _env["HTTP_" + key] = it->second;
    }
}

void CGIManager::_prepExecveArrays()
{
    _freeExecveArrays();

    _execveArgv = new char*[_scriptArgv.size() + 1];
    for (size_t i = 0; i < _scriptArgv.size(); ++i)
    {
        _execveArgv[i] = new char[_scriptArgv[i].size() + 1];
        std::strcpy(_execveArgv[i], _scriptArgv[i].c_str());
    }
    _execveArgv[_scriptArgv.size()] = NULL;
    _execveEnvp = new char*[_env.size() + 1];
    size_t idx = 0;
    for (std::map<std::string, std::string>::const_iterator it = _env.begin();
         it != _env.end(); ++it, ++idx)
    {
        std::string entry = it->first + "=" + it->second;
        _execveEnvp[idx] = new char[entry.size() + 1];
        std::strcpy(_execveEnvp[idx], entry.c_str());
    }
    _execveEnvp[_env.size()] = NULL;
}

void CGIManager::_freeExecveArrays()
{
    if (_execveArgv)
    {
        for (size_t i = 0; _execveArgv[i]; ++i)
            delete[] _execveArgv[i];
        delete[] _execveArgv;
        _execveArgv = NULL;
    }
    if (_execveEnvp)
    {
        for (size_t i = 0; _execveEnvp[i]; ++i)
            delete[] _execveEnvp[i];
        delete[] _execveEnvp;
        _execveEnvp = NULL;
    }
}

void CGIManager::_closePipes()
{
    if (_outPipe[0] != -1)
    {
        close(_outPipe[0]);
        _outPipe[0] = -1;
    }
    if (_outPipe[1] != -1)
    {
        close(_outPipe[1]);
        _outPipe[1] = -1;
    }
}


void CGIManager::_registerPid(pid_t pid)
{
    if (pid > 0)
        _activePids.insert(pid);
}

void CGIManager::_reapFinishedActivePids()
{
    if (_activePids.empty())
        return;

    std::set<pid_t> stillRunning;
    for (std::set<pid_t>::iterator it = _activePids.begin(); it != _activePids.end(); ++it)
    {
        int status = 0;
        pid_t result = waitpid(*it, &status, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR))
            stillRunning.insert(*it);
    }
    _activePids = stillRunning;
}

bool CGIManager::_isSpawnLimitReached()
{
    _reapFinishedActivePids();
    return _activePids.size() >= MAX_ACTIVE_CGI_CHILDREN;
}

void CGIManager::_unregisterPid(pid_t pid)
{
    CGIManager::_activePids.erase(pid);
}

void CGIManager::cleanupAllProcesses()
{
    if (CGIManager::_activePids.empty())
        return;

    for (std::set<pid_t>::iterator it = CGIManager::_activePids.begin(); it != CGIManager::_activePids.end(); ++it)
    {
        kill(*it, SIGTERM);
    }

    time_t startTime = time(NULL);
    const int GRACE_PERIOD = 5;

    while (!CGIManager::_activePids.empty() && (time(NULL) - startTime) < GRACE_PERIOD)
    {
        std::set<pid_t> remaining;
        for (std::set<pid_t>::iterator it = CGIManager::_activePids.begin(); it != CGIManager::_activePids.end(); ++it)
        {
            int status;
            pid_t result = waitpid(*it, &status, WNOHANG);
            if (result == 0)
                remaining.insert(*it);
        }
        CGIManager::_activePids = remaining;

        if (!CGIManager::_activePids.empty())
            usleep(100000);
    }

    if (!CGIManager::_activePids.empty())
    {
        for (std::set<pid_t>::iterator it = CGIManager::_activePids.begin(); it != CGIManager::_activePids.end(); ++it)
        {
            kill(*it, SIGKILL);
        }
    }

    for (std::set<pid_t>::iterator it = CGIManager::_activePids.begin(); it != CGIManager::_activePids.end(); ++it)
    {
        int status;
        waitpid(*it, &status, 0);
    }

    CGIManager::_activePids.clear();
}
