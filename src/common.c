#include "common.h"

void close_socket(SOCKET socket)
{
#ifdef WIN_PLATFORM
    closesocket(socket);
#endif
#ifdef LINUX_PLATFORM
    close(socket);
#endif
}

void sleep_ms(unsigned int milliseconds)
{
#ifdef WIN_PLATFORM
    Sleep(milliseconds);
#endif
#ifdef LINUX_PLATFORM
    usleep(milliseconds * 1000);
#endif 
}       