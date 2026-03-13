#include "../includes/CGIManager.hpp"
#include "../includes/Request.hpp"
#include "../includes/AllowedMethods.hpp"

#include <sys/wait.h>
#include <csignal>
#include <ctime>

//There's a zombie on your lawn...
std::set<pid_t> CGIManager::_activePids;

namespace CGIUtils
{
    /**
     * @brief Determines the interpreter binary for common script extensions.
     * @return Empty string if the script is a native executable.
     */
    std::string interpreterForScript(const std::string& scriptPath)
    {
        std::string::size_type dot = scriptPath.rfind('.');
        if (dot == std::string::npos)
            return "";
        std::string ext = scriptPath.substr(dot);
        if (ext == ".py")
            return "/usr/bin/python3";
        if (ext == ".pl")
            return "/usr/bin/perl";
        if (ext == ".rb")
            return "/usr/bin/ruby";
        if (ext == ".sh")
            return "/bin/sh";
        return "";
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
    else
        interp = CGIUtils::interpreterForScript(scriptPath);
    if (!interp.empty())
        _scriptArgv.push_back(interp);
    _scriptArgv.push_back(scriptPath);

    _prepExecveArrays();
}

void CGIManager::execute(int inputFd)
{
    if (pipe(_outPipe) == -1)
        throw std::runtime_error("CGIManager::execute: pipe() failed");

    _pId = fork();
    if (_pId < 0)
    {
        _closePipes();
        throw std::runtime_error("CGIManager::execute: fork() failed");
    }

    if (_pId == 0)
    {
        if (inputFd >= 0)
        {
            if (dup2(inputFd, STDIN_FILENO) == -1)
                _exit(1);
        }
        close(_outPipe[0]);
        if (dup2(_outPipe[1], STDOUT_FILENO) == -1)
            _exit(1);
        close(_outPipe[1]);

        execve(_execveArgv[0], _execveArgv, _execveEnvp);
        _exit(1);
    }
    // Parent process - register the PID for tracking
    _registerPid(_pId);
    close(_outPipe[1]);
    _outPipe[1] = -1;
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
