///@brief Modo de uso: gcc  -o server server.c -ggdb -Wall

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
/// sockets =>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
/// select =>
#include <sys/time.h>
#include <sys/select.h>
/// threads
#include <pthread.h>
#include "logger.h"
#define PORT 3456
#define UNIX_SOCK_PATH "/tmp/lab3.sock"

void *atenderPeticion (void *argumentos);
void lanzarThread(int udp_socket_server);

typedef struct argumentosThread
{
    int socketDescriptor;
    int loggerDescriptor;
}strarg;

int tcp_socket_server, udp_socket_server, unix_socket_server, maxfd; ///< Descriptores tanto para TCP, UDP y Unix.
fd_set readset, tempset;

///@brief Crea el socket, Configura las opciones, enlaza el puerto a la interface y pone el socket a la escucha de conexiones entrantes.
int start_tcp_server()
{
    logger("starting TCP server");
    // Create the server-side socket
    tcp_socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_socket_server < 0)
    {
        perror("TCP socket");
        return 0;
    }
    maxfd = tcp_socket_server;

    int flag = 1;
    int result = setsockopt( tcp_socket_server, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int) );
    if (result < 0)
    {
        perror("TCP setsockopt(TCP_NODELAY)");
        return 0;
    }

    result = setsockopt( tcp_socket_server, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int) );
    if (result < 0)
    {
        perror("TCP setsockopt(SO_REUSEADDR)");
        return 0;
    }

    result = setsockopt( tcp_socket_server, SOL_SOCKET, SO_KEEPALIVE, (char *)&flag, sizeof(int) );
    if (result < 0)
    {
        perror("TCP setsockopt(SO_KEEPALIVE)");
        return 0;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the appropriate port and interface (INADDR_ANY)
    if ( bind( tcp_socket_server, (struct sockaddr *)&address, sizeof(address) ) < 0 )
    {
        perror("TCP bind");
        return 0;
    }

    // Listen for incoming connection(s)
    if ( listen( tcp_socket_server, 1 ) < 0 )
    {
        perror("TCP listen");
        return 0;
    }

    logger("TCP server started");
    return 1;
}

///mirar la funcion @see start_tcp_server
int start_udp_server()
{
    logger("starting UDP server");
    udp_socket_server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_server < 0)
    {
        perror("UDP socket");
        return 0;
    }
    maxfd = udp_socket_server;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the appropriate port and interface (INADDR_ANY)
    if ( bind( udp_socket_server, (struct sockaddr *)&address, sizeof(address) ) < 0 )
    {
        perror("UDP bind");
        return 0;
    }

    logger("UDP server started");
    return 1;
}
///mirar la funcion @see start_tcp_server
int start_unix_socket_server()
{
    logger("starting Unix server");
    unix_socket_server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_socket_server < 0)
    {
        perror("UDP socket");
        return 0;
    }
    maxfd = unix_socket_server;

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, UNIX_SOCK_PATH);
    unlink(local.sun_path);
    size_t len = strlen(local.sun_path) + sizeof(local.sun_family);

    // Bind socket to the appropriate port and interface (INADDR_ANY)
    if ( bind( unix_socket_server, (struct sockaddr *)&local, len ) < 0 )
    {
        perror("Unix bind");
        return 0;
    }

    // Listen for incoming connection(s)
    if ( listen( unix_socket_server, 1 ) < 0 )
    {
        perror("Unix listen");
        return 0;
    }

    logger("Unix server started");
    return 1;
}

///@brief Libera los recursos del socket
void stop_tcp_server()
{
    close(tcp_socket_server);
    logger("TCP server stopped");
}
///mirar la funcion @see stop_tcp_server
void stop_udp_server()
{
    close(udp_socket_server);
    logger("UDP server stopped");
}
///mirar la funcion @see stop_tcp_server
void stop_unix_socket_server()
{
    close(unix_socket_server);
    logger("Unix server stopped");
}

/**
    @brief Acepta un nuevo cliente para el socket especificado por parametro.
    @param socket descriptor de cualquier tipo de socket.
*/
int accept_new_clients(int socket)
{
    logger("New client in queue");
    struct sockaddr_in addr;
    int len = sizeof(addr);
    int result = accept(socket, (struct sockaddr *)&addr, (socklen_t*)&len);
    if (result == -1)
        // These are not errors according to the manpage.
        if (!(errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == ENONET || errno == EHOSTUNREACH || errno == EOPNOTSUPP || errno == ENETUNREACH))
            perror("accept error");
    FD_SET(result, &readset);
    maxfd = (maxfd < result) ? result : maxfd;
    FD_CLR(socket, &tempset);
    return result;
}

void read_udp_message()
{
    logger("New UDP message");
    // recvfrom(s, buf, BUFLEN, 0, &si_other, &slen)
}

void read_message()
{
    logger("New message");
    // do {
    //    result = recv(j, buffer, MAX_BUFFER_SIZE, 0);
    // } while (result == -1 && errno == EINTR);
}

void listen_and_accept_new_clients()
{
    int j, result;
    struct timeval tv;

    FD_ZERO(&readset);
    FD_SET(tcp_socket_server, &readset);
    FD_SET(udp_socket_server, &readset);
    FD_SET(unix_socket_server, &readset);

    do
    {
        memcpy(&tempset, &readset, sizeof(tempset));
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        result = select(maxfd + 1, &tempset, NULL, NULL, &tv);

        if (result == 0)
        {
            logger("select() timed out!\n");
        }
        else if (result < 0 && errno != EINTR)
        {
            perror("select()");
        }
        else if (result > 0)
        {
            if (FD_ISSET(udp_socket_server, &tempset))
            {
                lanzarThreadUDP(udp_socket_server);
            }

            if (FD_ISSET(tcp_socket_server, &tempset))
            {
                lanzarThread(tcp_socket_server);
            }

            if (FD_ISSET(unix_socket_server, &tempset))
            {
                lanzarThread(unix_socket_server);
            }

//            for (j=0; j<maxfd+1; j++)
//            {
//                if (FD_ISSET(j, &tempset))
//                {
//                    lanzarThread(j);
//                    FD_CLR(j, &tempset);
//                }      // end if (FD_ISSET(j, &tempset))
//            }      // end for (j=0;...)
        }
            // end else if (result > 0)
    }
    while (1);

}


void stop_main()
{
    stop_tcp_server();
    stop_udp_server();
    stop_unix_socket_server();
}

// main function
int main(int argc, char *argv[])
{
    inicializarLogger();
    logger("Loading server.....");

    if (start_tcp_server() && start_udp_server() && start_unix_socket_server())
    {
        logger("Server started successfully");
    }
    else
    {
        stop_main();
        return EXIT_FAILURE;
    }

    listen_and_accept_new_clients();
    stop_main();
    cerrarLogger();
    return EXIT_SUCCESS;
}

void lanzarThread(int udp_socket_server)
{
    printf("Cualquier cosa\n");
    pthread_t unThread;
    pthread_attr_t atributos;

    if (pthread_attr_init(&atributos) != 0)
    {
        perror("Problema al inicializar los atributos del thread");
        logger("Problema al inicializar los atributos del thread");
        exit(EXIT_FAILURE);
    }

    strarg *argumentos;
    argumentos = (strarg*)calloc(1, sizeof(strarg));
    argumentos->socketDescriptor = udp_socket_server;

    if((pthread_create(&unThread, &atributos, atenderPeticion, (void *)argumentos)) != 0)
    {
        perror("Problema al iniciar el thread");
        logger("Problema al iniciar el thread");
    }
}

void *atenderPeticion (void *argumentos)
{
    strarg *argumentosDelThread = (strarg*)argumentos;
    logger("La peticion fue atendida correctamente");
    printf("La peticion fue atendida correctamente\n");
    accept_new_clients(argumentosDelThread->socketDescriptor);
    return NULL;
}

void *atenderPeticionUDP (void *argumentos)
{

}
