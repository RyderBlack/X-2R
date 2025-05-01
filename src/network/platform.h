#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") // Optional: Auto-link Winsock on MSVC
    typedef int socklen_t;
#define CLOSE_SOCKET(s) closesocket(s)
#define INIT_NETWORKING() \
WSADATA wsaData; \
if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { \
fprintf(stderr, "WSAStartup failed.\n"); \
exit(EXIT_FAILURE); \
}
#define CLEANUP_NETWORKING() WSACleanup()
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define CLOSE_SOCKET(s) close(s)
#define INIT_NETWORKING()  // No-op on Unix
#define CLEANUP_NETWORKING() // No-op on Unix
#endif

#endif // PLATFORM_H