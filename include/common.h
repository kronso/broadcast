#ifndef COMMON_H
#define COMMON_H

#if __x86_64__
    #error "unsupported architecture and bit mode"
#endif

// #if !defined(_WIN32) && (!defined(__linux__) || !defined(__APPLE__))
//     #error "unsupported operating system"
// #endif

// Top macro should be used, but this one
// shows colours for editor :)
#if defined(_WIN32)
    #define WIN_PLATFORM
#elif defined(__linux__) || defined(__APPLE__)
    #define LINUX_PLATFORM
#else
    #error "unsupported operating system"
#endif

#ifdef WIN_PLATFORM
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    // #pragma comment(lib, "user32.lib");
    // #pragma comment(lib, "Ws2_32.lib");
    #undef INET_ADDRSTRLEN
    #define INET_ADDRSTRLEN 16
#endif
#ifdef LINUX_PLATFORM
    #define _GNU_SOURCE
    #include <unistd.h>
    #include <pthread.h>

    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <poll.h>
    
    typedef int SOCKET; // windows uses uint ptr for sockets
    typedef struct sockaddr SOCKADDR;
    typedef struct sockaddr_in SOCKADDR_IN;
    // typedef struct addrinfo* PADDRINFOA;
    typedef struct addrinfo ADDRINFO;

    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET SOCKET_ERROR
#endif

void close_socket(SOCKET socket);
void sleep_ms(unsigned int milliseconds);

#endif