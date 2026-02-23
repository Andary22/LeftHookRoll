#include "LocationConf.hpp"

LocationConf::LocationConf() : _autoIndex(false) {}

LocationConf::LocationConf(const LocationConf& other)
	: _path(other._path),
	  _root(other._root),
	  _allowedMethods(other._allowedMethods),
	  _returnURL(other._returnURL),
	  _returnCode(other._returnCode),
	  _autoIndex(other._autoIndex),
	  _defaultPage(other._defaultPage),
	  _storageLocation(other._storageLocation)
{}

LocationConf& LocationConf::operator=(const LocationConf& other)
{//
	if (this != &other)
	{
		_path            = other._path;
		_root            = other._root;
		_allowedMethods  = other._allowedMethods;
		_returnURL       = other._returnURL;
		_returnCode      = other._returnCode;
		_autoIndex       = other._autoIndex;
		_defaultPage     = other._defaultPage;
		_storageLocation = other._storageLocation;
	}
	return *this;
}

LocationConf::~LocationConf() {}


const std::string& LocationConf::getPath() const
{
    return _path; 
}
const std::string& LocationConf::getRoot() const          
{
    return _root; 
}
const AllowedMethods& LocationConf::getAllowedMethods() const 
{ 
    return _allowedMethods; 
}
const std::string& LocationConf::getReturnURL() const      
{ 
    return _returnURL; 
}
const std::string& LocationConf::getReturnCode() const     
{ 
    return _returnCode; 
}
bool               LocationConf::getAutoIndex() const      
{ 
    return _autoIndex; 
}
const std::string& LocationConf::getDefaultPage() const    
{ 
    return _defaultPage; 
}
const std::string& LocationConf::getStorageLocation() const 
{ 
    return _storageLocation; 
}

void LocationConf::setPath(const std::string& path)                      
{ 
    _path = path; 
}
void LocationConf::setRoot(const std::string& root)                      
{ 
    _root = root; 
}
void LocationConf::setReturnURL(const std::string& url)                  
{ 
    _returnURL = url; 
}
void LocationConf::setReturnCode(const std::string& code)                
{ 
    _returnCode = code; 
}
void LocationConf::setAutoIndex(bool autoIndex)                          
{ 
    _autoIndex = autoIndex; 
}
void LocationConf::setDefaultPage(const std::string& defaultPage)        
{ 
    _defaultPage = defaultPage; 
}
void LocationConf::setStorageLocation(const std::string& storageLocation)
{ 
    _storageLocation = storageLocation; 
}

void LocationConf::addAllowedMethod(HTTPMethod method)
{
	_allowedMethods.addMethod(method);
}

bool LocationConf::isMethodAllowed(HTTPMethod method) const
{
	return _allowedMethods.isAllowed(method);
}
