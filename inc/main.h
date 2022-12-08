
#ifndef MAIN_H
#define MAIN_H 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "log.h"
#include "config_async.h"
#include "device.h"

int kill_them_all();
int wait_for_all();
void sighandler(int);
void sigusr2handler(int);
int copySecureFromSharedMemory(accel_data *dest);
int atender_cliente(int sock);
int agregar_proceso(int pid);
int remover_proceso(int pid);
int vaciar_lista();
int pick_pid();

typedef struct _nodo{
    int pid;
    struct _nodo *next;
} nodo;

//Client time out in seconds
#define KA_TIMEOUT 60


#endif