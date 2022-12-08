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

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

typedef struct 
{
    int backlog;
    int connectios;
    int readtime;
    int mediacount;
    int localport;
} config_data;

typedef struct msgbuf {
    long mtype;       /* message type, must be > 0 */
    config_data mdata;    /* message data */
} mq_data_type;

int run_config_reading();
int notificarCambios();
int cargarConfiguracion();




#endif