#ifndef UTILS_H
#define UTILS_H

#include <string>

#ifdef __LINUX__
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR   -1
    #define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))

    typedef int socket_t;
    typedef pthread_t tid_t;
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;
#endif

#ifdef _MSC_VER
    #define __WIN32__
#endif

#ifdef __WIN32__
    #define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
	typedef long long unsigned int socket_t;
    typedef unsigned long tid_t;
    typedef long long int ssize_t;
    typedef void* thread_t;
    typedef void* mutex_t;

    #ifdef _MSC_VER
        #pragma warning(disable : 4996)
    #endif
#endif

typedef unsigned int uint_t;

const uint_t BUFFER_SIZE = 1024;

const uint_t MIN_PORT_ALLOWED = 5000;

const uint_t MAX_PORT_ALLOWED = 5050;

std::string getStringNullOrLength(uint8_t* buffer, uint_t len);

bool isPortValid(uint_t port);

bool convertAddr(const std::string& addr, struct sockaddr_in* out);

uint_t stou(const std::string& str, size_t* idx = 0, int base = 10);

#ifdef __WIN32__
std::wstring widen(const std::string& input);
#endif

#endif
