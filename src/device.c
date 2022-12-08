#include "device.h"

extern int working;
extern char queue_device[];

mq_data_type config;
int configValid=0;

shmbuf *sharedMemory = NULL;

/* Device life cycle

LEER DISP Y ESCRIBIR EN BUFFER

while(){
    POST($1)

    LEER DISP Y ESCRIBIR EN BUFFER

    WAIT($1)
    POST($2)

    LEER DISP Y ESCRIBIR EN BUFFER

    WAIT($2)
}

*/

/* Esta funcion publica el nuevo valor en la memoria compartida */
int publishData(accel_data data){
    if(NULL == sharedMemory){ return EXIT_FAILURE;}

    if(SHARED_MEMORY_SIZE == sharedMemory->firstFreePosition){
        sharedMemory->firstFreePosition = 0;
    }

    if(sem_trywait(&sharedMemory->sem1)<0){
        if(EAGAIN == errno){
            //No logro tomar el semaforo, entonces significa que tengo que tomar el semaforo 2 y postear este para desbloquear los procesos que estan esperando leer
            if(sem_wait(&sharedMemory->sem2)<0){
                if(EINTR == errno) return EXIT_FAILURE;
            }
            uint16_t pos = sharedMemory->firstFreePosition++;

            /* Copio los datos a la shared memory */
            sharedMemory->accel_xout_readings[pos] = data.accel_xout;
            sharedMemory->accel_yout_readings[pos] = data.accel_yout;
            sharedMemory->accel_zout_readings[pos] = data.accel_zout;

            sharedMemory->gyro_xout_readings[pos] = data.gyro_xout;
            sharedMemory->gyro_yout_readings[pos] = data.gyro_yout;
            sharedMemory->gyro_zout_readings[pos] = data.gyro_zout;

            sharedMemory->temp_out_readings[pos] = data.temp_out;

            /* recalculo las medias moviles */
           
            sharedMemory->filtered_read.accel_xout = recalcularFiltro(sharedMemory->accel_xout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
            sharedMemory->filtered_read.accel_yout = recalcularFiltro(sharedMemory->accel_yout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
            sharedMemory->filtered_read.accel_zout = recalcularFiltro(sharedMemory->accel_zout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

            sharedMemory->filtered_read.gyro_xout = recalcularFiltro(sharedMemory->gyro_xout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
            sharedMemory->filtered_read.gyro_yout = recalcularFiltro(sharedMemory->gyro_yout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
            sharedMemory->filtered_read.gyro_zout = recalcularFiltro(sharedMemory->gyro_zout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

            sharedMemory->filtered_read.temp_out = recalcularFiltro(sharedMemory->temp_out_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

            /* Aumento el contador de muestras para debug */
            sharedMemory->sample_count++;
            sem_post(&sharedMemory->sem1);
        }else{
            return EXIT_FAILURE;
        }
    }else{
        //Pude tomar el semaforo 1, entonces escribo en la memoria y posteo el semadoro 2;
        uint16_t pos = sharedMemory->firstFreePosition++;

        /* Copio los datos a la shared memory */
        sharedMemory->accel_xout_readings[pos] = data.accel_xout;
        sharedMemory->accel_yout_readings[pos] = data.accel_yout;
        sharedMemory->accel_zout_readings[pos] = data.accel_zout;

        sharedMemory->gyro_xout_readings[pos] = data.gyro_xout;
        sharedMemory->gyro_yout_readings[pos] = data.gyro_yout;
        sharedMemory->gyro_zout_readings[pos] = data.gyro_zout;

        sharedMemory->temp_out_readings[pos] = data.temp_out;

        /* recalculo las medias moviles */
        
        sharedMemory->filtered_read.accel_xout = recalcularFiltro(sharedMemory->accel_xout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
        sharedMemory->filtered_read.accel_yout = recalcularFiltro(sharedMemory->accel_yout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
        sharedMemory->filtered_read.accel_zout = recalcularFiltro(sharedMemory->accel_zout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

        sharedMemory->filtered_read.gyro_xout = recalcularFiltro(sharedMemory->gyro_xout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
        sharedMemory->filtered_read.gyro_yout = recalcularFiltro(sharedMemory->gyro_yout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);
        sharedMemory->filtered_read.gyro_zout = recalcularFiltro(sharedMemory->gyro_zout_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

        sharedMemory->filtered_read.temp_out = recalcularFiltro(sharedMemory->temp_out_readings, config.mdata.filtercount, pos, SHARED_MEMORY_SIZE);

        /* Aumento el contador de muestras para debug */
        sharedMemory->sample_count++;
        sem_post(&sharedMemory->sem2);
    }
    return EXIT_SUCCESS;
}

/* Chequea que la configuracion sea distinta a la que tenemos guardada localmente, si es asi la modifica */
int checkAndUpdate(mq_data_type aux_cfg){
    
    int hay_cambio = 0;
    if(config.mdata.backlog != aux_cfg.mdata.backlog){
        config.mdata.backlog = aux_cfg.mdata.backlog;
        hay_cambio = 1;
    }

    if(config.mdata.connections != aux_cfg.mdata.connections){
        config.mdata.connections = aux_cfg.mdata.connections;
        hay_cambio = 1;
    }

    if(aux_cfg.mdata.filtercount > SHARED_MEMORY_SIZE){
        aux_cfg.mdata.filtercount = SHARED_MEMORY_SIZE;
        hay_cambio = 1;
    }

    if(config.mdata.filtercount != aux_cfg.mdata.filtercount){
        config.mdata.filtercount = aux_cfg.mdata.filtercount;
        hay_cambio = 1;
    }

    return hay_cambio;
}

/* Inicializacion de la memoria compartida, si hay algun problema retorna 0 */
int sharedMemoryInit(){
    /* Create shared memory object and set its size to the size of our structure. */
    int fd = shm_open(SHARED_MEMORY_PATH, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    if (-1 == fd){
        perror("Error al abrir el descriptor.\n");
        return 0;
    }
    if (ftruncate(fd, sizeof(shmbuf)) == -1){
        perror("Error al modificar el tamaño del area de memoria compartida.\n");
        return 0;
    }

    /* Map the object into the caller's address space. */
    sharedMemory = mmap(NULL, sizeof(*sharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sharedMemory == MAP_FAILED){
        perror("Error al mapear la memoria a la variable local.\n");
        return 0;
    }

    /* Initialize semaphores as process-shared, with value 0. */
    if (sem_init(&sharedMemory->sem1, 1, 0) == -1)
        return 0;
    if (sem_init(&sharedMemory->sem2, 1, 0) == -1)
        return 0;

    /* Inicializco la memoria compartida */
    for (int j = 0; j < SHARED_MEMORY_SIZE; j++){
        sharedMemory->accel_xout_readings[j] = 0;
        sharedMemory->accel_yout_readings[j] = 0;
        sharedMemory->accel_zout_readings[j] = 0;
        sharedMemory->gyro_xout_readings[j] = 0;
        sharedMemory->gyro_yout_readings[j] = 0;
        sharedMemory->gyro_zout_readings[j] = 0;
        sharedMemory->temp_out_readings[j] = 0;
    }


    sharedMemory->filtered_read.accel_xout = 0;
    sharedMemory->filtered_read.accel_yout = 0;
    sharedMemory->filtered_read.accel_zout = 0;
    sharedMemory->filtered_read.gyro_xout = 0;
    sharedMemory->filtered_read.gyro_yout = 0;
    sharedMemory->filtered_read.gyro_zout = 0;
    sharedMemory->filtered_read.temp_out = 0;

    sharedMemory->firstFreePosition=0;
    sharedMemory->sample_count = 0;

    if (sem_post(&sharedMemory->sem2) == -1){
        perror("Error al habilitar el semaforo.\n");
        return 0;
    }

    return 1;
}


int run_device_reading(){
    nfds_t nfds;
    struct pollfd fds[2];
    int poll_num=0;
    int time_out=50;
    mq_data_type config_aux;

    logg(info, "Iniciando manejador del dispositivo.");

    //Limpio la memoria compartida para no tener basura.
    if(!sharedMemoryInit()){
        //Produjo algun error la inicializacion de la memoria compartida.. chocar la calesita..
        return EXIT_FAILURE;
    }

    //Inicializo la cola de mensajes
    mqd_t qd_device;
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(config);
    attr.mq_curmsgs = 0;

    if ((qd_device = mq_open(queue_device, O_RDONLY | O_CREAT | O_NONBLOCK, 0660, &attr)) == -1) {
        logg(fatal, "Error en el mq_receive() :(");
        return EXIT_FAILURE;
    }

    //Abro el dispositivo para leer

    int device_fd = open("/dev/mpu6050_td3", O_RDONLY);
    if (device_fd < 0)
    {
        logg(fatal, "Error al abrir el dispositivo.");
        return EXIT_FAILURE;
    }

    nfds = 2;
    fds[0].fd = qd_device;       
    fds[0].events = POLLIN;

    fds[1].fd = device_fd;
    fds[1].events = POLLIN;


    while(working){

        poll_num = poll(fds, nfds, time_out);
        if (-1 == poll_num) {
            //El error puede deberse a que entro una señal.. hago un continue para volver a evaluar el working y si esta en cero me voy silbado bajito.
            if(errno != EINTR)
                logg(fatal, "Error en el poll :(");
            continue;
        }
        if(0 == poll_num){ //Salio por timeout..
            logg(warn, "Se produjo un timeout en el poll del device.. raroooo...");
            continue;
        }
        
        //Chequeo los descriptores
        for (int j = 0; j < nfds; j++) {
            if ((fds[j].revents & POLLIN)&&(0==j)) {    //Me fijo si el descriptor de configuracion recibio algun dato nuevo..
                if (mq_receive (qd_device, (char*)&config_aux, sizeof(config_aux), NULL) == -1) {
                    logg(fatal, "Error en el mq_receive() :(");
                    continue;
                }
                //Me fijo si la nueva configuracion cambio algo respecto a la anterior y si es asi devuelvo true y actualizo la config.
                if(checkAndUpdate(config_aux)){
                    logg(info, "Nueva configuracion detectada, actualizando...");
                    //Aca deberia reiniciar el lector de dispositivo para adaptarse a la nueva configuracion
                    configValid = 1;                            //Levanto el flag de que tengo config valida asi empiezo a capturar datos.

                    //printf("NEW CONFIG:\n\tbacklog: %u\n\tconnections: %u\n\treadtime: %u\n\tmediacount: %u\n", config.mdata.backlog, config.mdata.connectios, config.mdata.readtime, config.mdata.mediacount);
                }
            }
            if ((fds[j].revents & POLLIN)&&(1==j)) {
                if(!configValid) continue; //Si todavia no me llego la configuracion no puedo leer nada
                accel_data data_read;
                ssize_t result = read(device_fd, &data_read, sizeof(data_read));
                
                if (result > 0)
                {   
                    //data_read.temp_out = (int16_t)((float)data_read.temp_out/(float)340+36.56);
                    //printf("Temp: %d\tGyro (X,Y,Z): [%d, %d, %d]\tAcell (X,Y,Z): [%d, %d, %d]\n", data_read.temp_out, data_read.gyro_xout, data_read.gyro_yout, data_read.gyro_zout, data_read.accel_xout, data_read.accel_yout,data_read.accel_zout);
                    if(EXIT_FAILURE == publishData(data_read)){
                        //Si se produzco una falla se debio a algun problema en los semaforos. el sistema no puede seguir funcionando..
                        working = 0;
                        break;
                    }
                }
                continue;
            }
        }
    }

    //Mato la cola de mensajes
    mq_unlink(queue_device);
    if (mq_close (qd_device) == -1) {
        perror ("Client: mq_close");
    }

    //Cierro el dispositivo.
    close(device_fd);
    
    //Cierro la memoria compartida
    shm_unlink(SHARED_MEMORY_PATH);
    
    logg(info, "Cerrando el manejador del dispositivo.");
    return EXIT_SUCCESS;
}

