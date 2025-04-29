#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    // Initialize Winsock
    #define INIT_NETWORKING() \
    do { \
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { \
    fprintf(stderr, "WSAStartup failed\n"); \
    exit(EXIT_FAILURE); \
    } \
    } while (0)

    // Cleanup Winsock
    #define CLEANUP_NETWORKING() \
    do { WSACleanup(); } while (0)

    #define CLOSE_SOCKET(s) closesocket(s)

#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>

    #define INIT_NETWORKING()     ((void)0)
    #define CLEANUP_NETWORKING()  ((void)0)
    #define CLOSE_SOCKET(s)       close(s)
#endif

#endif // PLATFORM_H
