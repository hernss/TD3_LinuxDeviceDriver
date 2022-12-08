#ifndef CONFIG_H
#define CONFIG_H 1

#include "log.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <mqueue.h>
#include <errno.h>
#include <poll.h>

#define CONFIG_PATH "servidor.conf"
#define BUFFSIZE 1024

typedef struct 
{
    int backlog;
    int connections;
    int filtercount;
    int localport;
} config_data;

typedef struct msgbuf {
    long mtype;       /* message type, must be > 0 */
    config_data mdata;    /* message data */
} mq_data_type;

int init_config();
int destroy_config();
int notificarCambios();
int cargarConfiguracion();




#endif