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
/// log
#include "logger.h"

#define PORT 3456
#define UNIX_SOCK_PATH "/tmp/lab3.sock"

///Defines opciones GUI
#define VER_CLIENTES_CONECTADOS 1

#define TAMANIO_OPCION 64

///Defines opciones de recepcion, las que me envia el cliente...
#define CLIENTE_ENVIANDO_ARCHIVO 1
#define CLIENTE_PIDIENDO_LISTADO_DE_CLIENTES 2
#define SERVER_ENVIANDO_ARCHIVO 3
#define SERVIDOR_ENVIANDO_LISTA_DE_CLIENTES 4

#define TAMANIO_MAXIMO_RUTA 256

#define ENVIO_CANTIDAD_DE_CLIENTES 256

#define ENVIO_SOCKET_SIZE 256

#define FILE_BUFFER_SIZE 256

#define MSG_TAMANIO_ARCHIVO_SIZE 1024

#define TAM_MENSAJE_LOGGER 256

int fileTansfer(int socketDescriptorCliente, int tamanioArchivo, int socketDestino);
void *atenderPeticion (void *argumentos);
void lanzarThread(int udp_socket_server);
int mostrarListadoClientesConectados();
int dispatchOpcionRecibida(int opcion, int socketCliente);
void menuGUI();
void *menu_servidor();
void enviarListado (int socketCliente);

typedef struct argumentosThread
{
    int socketDescriptor;
} strarg;

int tcp_socket_server, udp_socket_server, unix_socket_server, maxfd; ///< Descriptores tanto para TCP, UDP y Unix.
fd_set readset, tempset;

int cantidadDeClientesConectados = 0;
int cantidadDeArchivosTransferidos = 0;

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
    cantidadDeClientesConectados++;
    FD_SET(result, &readset);
    FD_SET(result, &tempset); //Agregado los clientes al tempset tambien para que los vea en el for
    maxfd = (maxfd < result) ? result : maxfd;
    FD_CLR(socket, &tempset);
    return result;
}

int read_message(int socketDescriptorCliente, int tamanioMensaje)
{
    char buffer[tamanioMensaje];
    char mensajeLog[TAM_MENSAJE_LOGGER];
    int result;
    int retorno;
    logger("funcion read_message");
    do
    {
        result = recv(socketDescriptorCliente, buffer, tamanioMensaje, 0);
    }
    while (result == -1 && errno == EINTR);
    retorno = (result > 0) ? atol(buffer) : result;
}

int fileTansfer(int socketDescriptorCliente, int tamanioArchivo, int socketDestino)
{
    char buffer[FILE_BUFFER_SIZE];
    char mensajeLog[TAM_MENSAJE_LOGGER];
    int result;
    int cantidadTotalBytes = 0;
    int retorno;
    char bufferOpcion[TAMANIO_OPCION];
    printf("\n\t\tLeyendo Archivo\n");
    memset(bufferOpcion, '\0', TAMANIO_OPCION);
    sprintf(bufferOpcion, "%d", SERVER_ENVIANDO_ARCHIVO);
    send(socketDestino, bufferOpcion, TAMANIO_OPCION, 0);
    memset(buffer, '\0', FILE_BUFFER_SIZE);
    sprintf(buffer, "%d", tamanioArchivo);
    send(socketDestino, buffer, MSG_TAMANIO_ARCHIVO_SIZE, 0);
    do
    {
        result = recv(socketDescriptorCliente, buffer, FILE_BUFFER_SIZE, 0);
        cantidadTotalBytes += result;
        send(socketDestino, buffer, result, 0);
        printf("%s", buffer);
    }
    while ((result == -1 && errno == EINTR) || ((cantidadTotalBytes < tamanioArchivo) && result != -1));
    cantidadDeArchivosTransferidos++;
    sprintf(mensajeLog, "read_Archivo: Se leyo: %s y el result fue: %d\n", buffer, result);
    logger(mensajeLog);
    retorno = (result > 0) ? cantidadTotalBytes : -1;
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
//        menuGUI();
        //mostrarListadoClientesConectados(); //testing
        //memcpy(&tempset, &readset, sizeof(tempset));

        FD_SET(tcp_socket_server, &tempset);
        FD_SET(udp_socket_server, &tempset); //Vuelvo agregar los servidores para ver si hay nuevos clientes que aceptar
        FD_SET(unix_socket_server, &tempset);

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
                //lanzarThreadUDP(udp_socket_server);
            }

            if (FD_ISSET(tcp_socket_server, &tempset))
            {
                int socketClienteAceptado = accept_new_clients(tcp_socket_server);
                printf("Cliente Nuevo\n");
            }

            if (FD_ISSET(unix_socket_server, &tempset))
            {
                //lanzarThread(unix_socket_server);
            }
//            printf("\t\tBuscando clientes en el tempset\n\n");
            for (j=0; j<maxfd+1; j++)
            {
//                printf("%d |", j);
                if (FD_ISSET(j, &tempset)) //En este momento estan solo los clientes. Los servidores los elimine en accept
                {
                    lanzarThread(j);
                    FD_CLR(j, &tempset); //Elimino el cliente que acabo de atender.
                }      // end if (FD_ISSET(j, &tempset))
            }      // end for (j=0;...)
//            printf("\n");
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

    pthread_t v_thread_menu_servidor;
    pthread_create (&v_thread_menu_servidor, NULL, menu_servidor, NULL);

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

void menuGUI()
{
    printf("\t\t----------------MENU------------------\n\n");
    printf("1-Mostrar la cantidad de clientes online\n");
    printf("2-Mostrar cantidad de archivos descargados\n");
    printf("3-Mostrar la cantidad de bytes transmitidos\n");
    printf("4-Mostrar lista de clientes conectados\n");
    printf("5-Salir\n");
}

//THREAD para manejar el menu del servidor
void *menu_servidor()
{
    char option;
    char ignored;
    do
    {
        menuGUI();
        option = fgetc(stdin);
        do
        {
            ignored = fgetc(stdin);
        }
        while ((ignored != '\n') && (ignored != EOF));

        switch (option)
        {
        case '1': //Muestro la cantidad de clientes online
            printf("Cantidad de clientes conectados: %d\n", cantidadDeClientesConectados);
            printf("\n\nPresione cualquier tecla para continuar\n");
            getchar();
            break;
        case '2': //Muestro la cantidad de archivos descargados
            printf("Cantidad de archivos transferidos: %d\n", cantidadDeArchivosTransferidos);
            printf("\n\nPresione cualquier tecla para continuar\n");
            getchar();
            break;
        case '3': //Muestro la cantidad de bytes transmitidos
            break;
        case '4': //Muestro la lista de usuarios
            mostrarListadoClientesConectados();
            break;
        case '5': //Cierro el servidor(esto hay que mejorarlo)
            exit( 1 );
            break;
        }
    }
    while (1);
}

int mostrarListadoClientesConectados()
{
    int i, serverCount = 0, contCli = 0;
    printf("\t\tLista de clientes conectados\n\n");
    for(i = 0; i < maxfd+1; i++)
    {
        if(FD_ISSET(i, &readset))
        {
            if(serverCount>2)
            {
                printf("%d |", i);
                contCli++;
            }

            serverCount++;
        }
    }
    printf("\n");
    return contCli;
}

void lanzarThread(int socket_server)
{
    pthread_t unThread;
    pthread_attr_t atributos;

    if (pthread_attr_init(&atributos) != 0)
    {
        logger("Problema al inicializar los atributos del thread");
        exit(EXIT_FAILURE);
    }

    strarg *argumentos;
    argumentos = (strarg*)calloc(1, sizeof(strarg));
    argumentos->socketDescriptor = socket_server;

    if((pthread_create(&unThread, &atributos, atenderPeticion, (void *)argumentos)) != 0)
    {
        logger("Problema al iniciar el thread");
    }
}

void *atenderPeticion (void *argumentos)
{
    logger("funcion atenderPeticion");

    int opcion;
    char mensaje[256];

    strarg *argumentosDelThread = (strarg*)argumentos;

    do
    {
        opcion = read_message(argumentosDelThread->socketDescriptor, TAMANIO_OPCION);
        printf("Me llego la opcion %d por parte del cliente\n", opcion);
        if(opcion == -1)
            sprintf(mensaje, "ERROR: Cliente %d desconectado abruptamente. Cerrando thread\n", argumentosDelThread->socketDescriptor);
        else if (opcion == 0)
            sprintf(mensaje, "Cliente %d desconectado de forma normal. Cerrando thread\n", argumentosDelThread->socketDescriptor);
        else
            dispatchOpcionRecibida(opcion, argumentosDelThread->socketDescriptor);

        logger(mensaje);
    }
    while(opcion > 0);  //TODO El corte debería ser el error de read_message, por ejemplo que se desconecto el cliente es un 0...
    FD_CLR(argumentosDelThread->socketDescriptor, &readset);
    return NULL;
}

int dispatchOpcionRecibida(int opcion, int socketCliente)
{
    int descriptorDestino;
    long int tamanioArchivoRecibido;
    long int tamanioLeido = 0;
    char bufferOpcionEnvioLista[TAMANIO_OPCION];
    char bufferCantidadDeClientes[ENVIO_CANTIDAD_DE_CLIENTES];
    switch(opcion)
    {
    case CLIENTE_ENVIANDO_ARCHIVO:
        descriptorDestino = read_message(socketCliente, ENVIO_SOCKET_SIZE);
        tamanioArchivoRecibido = read_message(socketCliente, MSG_TAMANIO_ARCHIVO_SIZE);
        tamanioLeido = fileTansfer(socketCliente, tamanioArchivoRecibido, descriptorDestino);
        break;
    case CLIENTE_PIDIENDO_LISTADO_DE_CLIENTES:
        memset(bufferOpcionEnvioLista, '\0', TAMANIO_OPCION);
        sprintf(bufferOpcionEnvioLista, "%d", SERVIDOR_ENVIANDO_LISTA_DE_CLIENTES);
        send(socketCliente, bufferOpcionEnvioLista, TAMANIO_OPCION, 0);
        int cantClientes = mostrarListadoClientesConectados();
        memset(bufferCantidadDeClientes, '\0', ENVIO_CANTIDAD_DE_CLIENTES);
        sprintf(bufferCantidadDeClientes, "%d", cantClientes);
        send(socketCliente, bufferCantidadDeClientes, ENVIO_CANTIDAD_DE_CLIENTES, 0);
        enviarListado(socketCliente);
        //escribo cantidad de clientes que hay en el conjunto
        //recorro el conjunto READSET y voy enviando socketDescriptor tras socketDescriptor
        break;
    }
}

void enviarListado (int socketCliente)
{
    int i, serverCount = 0;
    char bufferSocketCliente[TAMANIO_OPCION];
    for(i = 0; i < maxfd+1; i++)
    {
        if(FD_ISSET(i, &readset))
        {
            if(serverCount>2)
            {
                memset(bufferSocketCliente, '\0', TAMANIO_OPCION);
                sprintf(bufferSocketCliente, "%d", i);
                send(socketCliente, bufferSocketCliente, TAMANIO_OPCION, 0);
            }

            serverCount++;
        }
    }
}


