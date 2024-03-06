#include "../include/common.h"
#include "../include/debug.h"

int get_error(enum ERROR_TYPE err_type)
{
    // Assume that system is Linux since errno is always used
    // then check if it's Windows.
    int err_code = errno;
    switch (err_type)
    {
        case SYSTEM_ERR: 
#ifdef WIN_PLATFORM
        err_code = GetLastError();
#endif
        break;

        case SOCKET_ERR: case GAI_ERR: 
#ifdef WIN_PLATFORM
        err_code = WSAGetLastError();
#endif

        break;
        default: err_code = INVALID_ERROR_TYPE; break;
    }
    return err_code;
}
char* error_str(enum ERROR_TYPE err_type, int err_code)
{
    // Assume the system is Linux
    char* err_msg = strerror(err_code);
#ifdef WIN_PLATFORM
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   
                        NULL,                                                                                          
                        err_code,                                                                                      
                        0,                                                                                             
                        (LPSTR)&err_msg,                                                                               
                        0,                                                                                             
                        NULL);
#endif
    if (err_type == GAI_ERR)
        err_msg = gai_strerror(err_code);
    return err_msg;
}