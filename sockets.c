#include "common.h"
#define DEBUG
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char gateway[INET_ADDRSTRLEN];
char host_ipv4[INET_ADDRSTRLEN];
char broadcast[INET_ADDRSTRLEN];
char subnet_mask[INET_ADDRSTRLEN];

void get_relevant_addr(void)
{
#ifdef WIN_PLATFORM
    PIP_ADAPTER_ADDRESSES p_adapters = NULL;
    ULONG adapters_sz = 0; 
    ULONG res;
    do {
        res = GetAdaptersAddresses(AF_INET, 0, NULL, p_adapters, &adapters_sz);
        if (res == ERROR_BUFFER_OVERFLOW)
        {
            p_adapters = malloc(adapters_sz);
            if (p_adapters == NULL)
            {
                fprintf(stderr, "Not enough memory to store adapter addresses.\n");
                free(p_adapters);
                exit(EXIT_FAILURE);
            }
            res = GetAdaptersAddresses(AF_INET, 0, NULL, p_adapters, &adapters_sz);
        }
        else
        {
            fprintf(stderr, "Failed to get adapater addresses.\n");
            exit(EXIT_FAILURE);
        }
    } while (res != ERROR_SUCCESS);
    PIP_ADAPTER_ADDRESSES ptr = p_adapters;
    for (; ptr != NULL; ptr = ptr->Next)
    {   
        /* Evidence of an ISP - user has modem */
        // DHCP - Dynamic host configuration protocol
        // provides an IP address, subnet mask and default gateway to DHCP client
        // DDNS - Dynamic domain name service
        // automatically updates IP addresses
        if (ptr->Dhcpv4Enabled && ptr->DdnsEnabled && ptr->OperStatus == IfOperStatusUp)
        {
            if (ptr->FirstDnsServerAddress)
            {
                PSOCKADDR_IN sock_addr = (PSOCKADDR_IN)ptr->FirstDnsServerAddress->Address.lpSockaddr;
                inet_ntop(AF_INET, &sock_addr->sin_addr, gateway, INET_ADDRSTRLEN);
                
                // Get first IP assigned 
                sock_addr = (PSOCKADDR_IN)ptr->FirstUnicastAddress->Address.lpSockaddr;
                inet_ntop(AF_INET, &sock_addr->sin_addr, host_ipv4, INET_ADDRSTRLEN);

                // Get subnet mask and do bitwise op to get directed broadcast
                ULONG mask;
                ConvertLengthToIpv4Mask(ptr->FirstUnicastAddress->OnLinkPrefixLength, &mask);
                ULONG broadcast_addr = (sock_addr->sin_addr.s_addr & INADDR_BROADCAST) | ~mask;
                inet_ntop(AF_INET, &mask, subnet_mask, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &broadcast_addr, broadcast, INET_ADDRSTRLEN);
                break;
            }
        }
    }
    free(p_adapters);
#endif
#ifdef LINUX_PLATFORM
    // Get gateway (didn't work on __APPLE__)
    FILE* fptr;
    // route -n | grep 'UG[ \t]' | awk '{print $2}'
    fptr = popen("ip route | grep default | awk '{print $3}'", "r");
    fgets(gateway, INET_ADDRSTRLEN, fptr);
    pclose(fptr);

    struct sockaddr_in* in;
    struct ifaddrs* ip_info, *ip_ptr;
    getifaddrs(&ip_info);
    
    int flags = IFF_BROADCAST;
    for (ip_ptr = ip_info; ip_ptr != NULL; ip_ptr = ip_ptr->ifa_next)
    {
        if (ip_ptr->ifa_flags & flags && ip_ptr->ifa_addr->sa_family == AF_INET)
        {
            if (ip_ptr->ifa_addr)
            {
                in = (struct sockaddr_in *)ip_ptr->ifa_addr;
                inet_ntop(AF_INET, &in->sin_addr, host_ipv4, INET_ADDRSTRLEN);
            }
            if (ip_ptr->ifa_broadaddr)
            {
                in = (struct sockaddr_in *)ip_ptr->ifa_broadaddr;
                inet_ntop(AF_INET, &in->sin_addr, broadcast, INET_ADDRSTRLEN);
            }
            if (ip_ptr->ifa_netmask)
            {
                in = (struct sockaddr_in *)ip_ptr->ifa_netmask;
                inet_ntop(AF_INET, &in->sin_addr, subnet_mask, INET_ADDRSTRLEN);
            }
        }
    }
    freeifaddrs(ip_info);
#endif
}
#define UNAME_MAX 9
#define LAN_PORT_SERVER "4444"
#define PLAYER_LIMIT 4

typedef enum _State
{
    SEND_BROADCAST = 1,
    END_BROADCAST
} State;
State state = SEND_BROADCAST;

struct PlayerInfo
{
    char uname[UNAME_MAX]; 
    char server_ip[INET_ADDRSTRLEN]; 
    unsigned short port; 

    struct sockaddr client_addr;
    socklen_t client_sz;

    State state;
};
struct PlayerInfo players[PLAYER_LIMIT];
short nplayers = 0;


void join_server(void)
{
    SOCKET socket_fd;
    int res;
    struct addrinfo* result, hints;
    hints = (struct addrinfo)
    {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP,
    };
    // Changed to nodename to host_ipv4 instead of broadcast for Windows
#ifdef WIN_PLATFORM
    ASSERT(getaddrinfo(host_ipv4, LAN_PORT_SERVER, &hints, &result) != 0, GAI_ERR);
#endif
#ifdef LINUX_PLATFORM
    ASSERT(getaddrinfo(broadcast, LAN_PORT_SERVER, &hints, &result) != 0, GAI_ERR);
#endif

    socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    ASSERT(socket_fd != INVALID_SOCKET, SOCKET_ERR);
    res = bind(socket_fd, result->ai_addr, result->ai_addrlen);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    res = recvfrom(socket_fd, (char *)&players[1], sizeof(struct PlayerInfo), 0, result->ai_addr, &result->ai_addrlen);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    nplayers++;
    // shutdown(socket_fd, SHUT_RD);
    close_socket(socket_fd);
    // Bind to the server ip that was given from the broadcast
    // to send over this client's player info
    char port_str[6] = { 0 };
    sprintf(port_str, "%d", players[1].port);
    ASSERT(getaddrinfo(players[1].server_ip, port_str, &hints, &result) != 0, GAI_ERR);

    socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    ASSERT(socket_fd != INVALID_SOCKET, SOCKET_ERR);       
    
    SOCKET client_fd;
    socklen_t addr_sz;
    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_port = 0,
    };
    res = inet_pton(AF_INET, host_ipv4, &client_addr.sin_addr);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    addr_sz = sizeof(client_addr);

    client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    // bind is necessary for ephemeral port
    res = bind(client_fd, (struct sockaddr *)&client_addr, addr_sz);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    // getsockname returns the ephemeral port allocated by the kernel.
    /**
     * REMOVE THIS AND CHANGE TO players[0].port = players[1].port
     * IF YOU WANT TO ONLY ALLOW ONE INSTANCE ON A DEVICE!!!!?!?!>
    */
    ASSERT(getsockname(client_fd, (struct sockaddr *)&client_addr, &addr_sz) != SOCKET_ERROR, SOCKET_ERR);
    players[0].port = ntohs(client_addr.sin_port);
    players[0].client_addr = *((struct sockaddr*)&client_addr);
    players[0].client_sz = addr_sz;

    res = sendto(socket_fd, (char *)&players[0], sizeof(struct PlayerInfo), 0, result->ai_addr, result->ai_addrlen);
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    nplayers++;

    freeaddrinfo(result);
    close_socket(socket_fd);
    // shutdown(socket_fd, SHUT_WR);
    while (nplayers != PLAYER_LIMIT && state == SEND_BROADCAST)
    {
        res = recv(client_fd, (char *)&players[nplayers], sizeof(struct PlayerInfo), 0);
        ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
        if (players[nplayers].state == END_BROADCAST)
        {
            nplayers--;
            break;
        }
        printf("Username: %s\n", players[nplayers].uname);
        printf("IP: %s\n", players[nplayers].server_ip);   
        nplayers++;
    }
    printf("\nGame starting...\n");
    // shutdown(socket_fd, SHUT_RD);
    close_socket(client_fd);
}

void start_server(void)
{
    SOCKET server_fd;
    int res;
    socklen_t addr_sz;
    struct sockaddr_in server_addr =
    {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr.s_addr = INADDR_ANY
    };
    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT(server_fd != INVALID_SOCKET, SOCKET_ERR);

    res = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);

    // Ephemeral port assigned and return with getsockname
    addr_sz = sizeof(server_addr);
    ASSERT(getsockname(server_fd, (struct sockaddr *)&server_addr, &addr_sz) != -1, SOCKET_ERR);
    players[0].port = ntohs(server_addr.sin_port);
    nplayers++;

    struct pollfd fds =
    {
        .fd = server_fd,
        .events = POLLIN,
    }; 
    unsigned long nfds = 1;

    printf("\nServer started...\n");
    while (1)
    {
#ifdef WIN_PLATFORM
        res = WSAPoll(&fds, nfds, -1);
#endif
#ifdef LINUX_PLATFORM
        res = poll(&fds, nfds, 0);
#endif
        ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
        if (res == 0)
            continue;

        if (nplayers != PLAYER_LIMIT && state == SEND_BROADCAST)
        {
            if (fds.revents & POLLIN)
            {
                res = recv(server_fd, (char *)&players[nplayers], sizeof(struct PlayerInfo), 0);
                ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
                if (res == 0)
                   printf("\nClient disconected...\n"); 
                else
                {
                // MacOS - sa_len (__uint8_t) and sa_family (__uint_8t)
                // Windows and Linux - ONLY sa_family (unsigned short int / __uint16_t)
                // Windows and Linux will combine the values of sa_len and sa_family from MacOS
                // So we shift  8bits to get the correct value for sa_family;
                #if defined(WIN_PLATFORM) || defined(__linux__)
                    players[nplayers].client_addr.sa_family >>= 8;
                #endif
                    printf("%s has joined with port %d\n", players[nplayers].uname, players[nplayers].port);
                    nplayers++;
                }

                // Send to all clients PlayerInfo to the client that joined
                // Send all clients PlayerInfo from the server to the client that joined
                for (int i = nplayers - 2; i != 0; i--)
                {
                    res = sendto(server_fd, (char *)&players[nplayers - 1], sizeof(struct PlayerInfo), 0, &players[i].client_addr, players[i].client_sz);
                    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
                    printf("Sent from %s to %s...\n", players[nplayers - 1].uname, players[i].uname);
                    
                    res = sendto(server_fd, (char *)&players[i], sizeof(struct PlayerInfo), 0, &players[nplayers - 1].client_addr, players[nplayers - 1].client_sz);
                    ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
                    printf("Sent from %s to %s...\n", players[i].uname, players[nplayers - 1].uname);
                }
            }
            if (fds.revents & POLLHUP)
            {
                printf("Client disconected...\n"); // ??
            }
        }
        // Where the game begins...
        if (nplayers == PLAYER_LIMIT || state == END_BROADCAST)
        {
            int input_size = 0;
            state = (state == SEND_BROADCAST) ? END_BROADCAST: state;
            if (fds.revents & POLLIN)
            {
            }
            if (fds.revents & POLLHUP)
            {
                printf("Client disconected...\n"); // ??
            }
        }
    }
    // shutdown(server_fd, SHUT_RDWR);
    close_socket(server_fd);
}

void handle_server(void)
{
#ifdef WIN_PLATFORM
    HANDLE th;
    th = CreateThread(NULL, 0, (LPVOID)&start_server, NULL, 0, NULL);
#endif
#ifdef LINUX_PLATFORM
    pthread_t th;
    pthread_create(&th, NULL, (void *)&start_server, NULL);
#endif
}

void send_broadcast(void)
{
    SOCKET server_fd;
    int res;
    struct sockaddr_in server_addr =
    {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(LAN_PORT_SERVER)),
    };
    // Directed broadcast
    ASSERT(inet_pton(AF_INET, broadcast, &server_addr.sin_addr) != SOCKET_ERROR, SOCKET_ERR);

    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT(server_fd != INVALID_SOCKET, SOCKET_ERR);

    res = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_BROADCAST, &res, sizeof(res));
    
    printf("\nSending Broadcast...\n");
    while (nplayers != PLAYER_LIMIT && state == SEND_BROADCAST)
    {
        res = sendto(server_fd, (char *)&players[0], sizeof(struct PlayerInfo), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
        // send every 500 miliseconds (keep it around 100 to 500ms)
        sleep_ms(500);
    }  
    // After broadcast is over, send to all clients its state
    // so they can continue with the game.
    for (int i = 1; i < nplayers; i++)
    {
        players[i].state = state;
        res = sendto(server_fd, (char *)&players[i], sizeof(struct PlayerInfo), 0, &players[i].client_addr, players[i].client_sz);    
        ASSERT(res != SOCKET_ERROR, SOCKET_ERR);
    }
    printf("\nGame Starting...\n");
    close_socket(server_fd);
}   

void handle_broadcast(void)
{   
#ifdef WIN_PLATFORM
    HANDLE th;
    th = CreateThread(NULL, 0, (LPVOID)&send_broadcast, NULL, 0, NULL);
#endif
#ifdef LINUX_PLATFORM
    pthread_t th;
    pthread_create(&th, NULL, (void *)&send_broadcast, NULL);
#endif

}
void handle_client(void)
{
#ifdef WIN_PLATFORM
    HANDLE th;
    th = CreateThread(NULL, 0, (LPVOID)&join_server, NULL, 0, NULL);
#endif
#ifdef LINUX_PLATFORM
    pthread_t th;
    pthread_create(&th, NULL, (void *)&join_server, NULL);
#endif
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <uname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
#ifdef WIN_PLATFORM
    WSADATA wsa_data;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err != 0)
    {
        fprintf(stderr, "WSAStartup failed with err code: %d\n", err);
        exit(EXIT_FAILURE);
    }
#endif
    get_relevant_addr();
    printf("%s\n", broadcast);
    printf("%s\n", host_ipv4);
    printf("%s\n", gateway);

    memcpy(players[0].uname, argv[1], UNAME_MAX);
    memcpy(players[0].server_ip, host_ipv4, INET_ADDRSTRLEN);
    players[0].state = state;

    printf("c - Create Server\n");
    printf("j - Join Server\n");

    char c;
    while (state == SEND_BROADCAST)
    {
        c = getchar();      
        if (c == 'c')
        {
            handle_server();
            handle_broadcast(); 
        }
        else if (c == 'j')
            handle_client();
        else if (c == 'e')
            state = END_BROADCAST;
    }
    sleep_ms(1000000);
#ifdef WIN_PLATFORM
    WSACleanup();
#endif
    return 0;
}