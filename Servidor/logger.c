///@file logger.c
///@author Toti.
///@brief Modulo de loggeo

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

int fdLogger; ///< Descriptor para el log.

///@brief Esta funcion se encarga abrir o crear el log si no existe
int inicializarLogger ()
{
    char rutaCompleta[256];
    sprintf(rutaCompleta,"./logs/log%d.log", getpid());
    printf("%s\n",rutaCompleta);
    if ((fdLogger = open(rutaCompleta, O_CREAT|O_APPEND|O_WRONLY, 0777)) == -1)
    {
        perror("Error al crear o abrir el log");
        return EXIT_FAILURE;
    }
}

///@brief Loggea tanto en el servidor como en el cliente en un archivo de texto.
void logger(const char *text)
{
    time_t tiempo = time(0);
    struct tm *tlocal = localtime(&tiempo);
    char fechaHora[128];
    strftime(fechaHora, 128, "%d/%m/%y %H:%M:%S", tlocal);
    char buffer[256];
    sprintf(buffer, "%s [%ld] %s\n", fechaHora, getpid(), text);
    if (write(fdLogger, buffer, strlen(buffer)) == -1)
    {
        perror("Error al escribir en el log");
        exit(1);
    }
}

void cerrarLogger()
{
    close(fdLogger);
}
