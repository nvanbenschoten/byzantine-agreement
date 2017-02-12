#include <exception>
#include <sstream>
#include <string>

class SocketException : public std::exception {
public:
    virtual const char* what () const throw () {
        return "Could not create UDP Socket";
    }
};

class HostNotFoundException : public std::exception {
public:
    HostNotFoundException(std::string h) : host(h) {};
    virtual const char* what () const throw () {
        std::ostringstream stream;
        stream << "Could not find host " << host;
        return stream.str().c_str();
    }
private:
    std::string host;
};

class BindException : public std::exception {
public:
    virtual const char* what () const throw () {
        return "Could not bind UDP Socket";
    }
};
