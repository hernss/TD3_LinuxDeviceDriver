#ifndef DEVICE_H
#define DEVICE_H 1

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

#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>


#include <errno.h>
#include <poll.h>

#include "config_async.h"
#include "filter.h"

#define DEVICE_PATH "/dev/urandom"
#define SHARED_MEMORY_SIZE 512
#define SHARED_MEMORY_PATH "/td3_shared_memory"

typedef struct MPU6050_REGS { 
    int16_t gyro_xout; 
    int16_t gyro_yout; 
    int16_t gyro_zout; 
    int16_t temp_out;
    int16_t accel_xout; 
    int16_t accel_yout; 
    int16_t accel_zout; 
} accel_data; 

#define FS_GYRO 2.0
#define FS_ACCEL 250.0


typedef struct  {
    sem_t  sem1;            /* POSIX unnamed semaphore */
    sem_t  sem2;            /* POSIX unnamed semaphore */
    size_t firstFreePosition;               /* ubiacion del primer lugar disponible para escribir */
    //accel_data readings[SHARED_MEMORY_SIZE];               /* buffer de las ultimas lecturas */
    int16_t accel_xout_readings[SHARED_MEMORY_SIZE];
    int16_t accel_yout_readings[SHARED_MEMORY_SIZE];
    int16_t accel_zout_readings[SHARED_MEMORY_SIZE];
    int16_t gyro_xout_readings[SHARED_MEMORY_SIZE];
    int16_t gyro_yout_readings[SHARED_MEMORY_SIZE];
    int16_t gyro_zout_readings[SHARED_MEMORY_SIZE];
    int16_t temp_out_readings[SHARED_MEMORY_SIZE];
    accel_data filtered_read;               /* Variable donde almaceno el dato filtrado */
    unsigned long sample_count;
} shmbuf;

int sharedMemoryInit();
int publishData(accel_data data);
int run_device_reading();
int checkAndUpdate(mq_data_type aux_cfg);
#endif