#include "main.h"

int reading_config_pid = 0;
int reading_device_pid = 0;
int working = 1;
const char queue_device[] = "/td3-config-device";
const char queue_main[] = "/td3-config-main";
// Flag que se pone en 1 cuando tengo configuracion valida
int validConfig = 0;
int cantidad_de_usuarios=0;

//Puntero a la memoria compartida
shmbuf *sharedMemory_main = NULL;

//Variable de la cola de mensajes de configuracion
extern mq_data_type main_config;

/* Lista de pids hijos */
nodo *lista = NULL;

/* Funcion que copia de manera segura el contenido de la memoria compartida. Se bloquea a la espera de un nuevo valor. Devuelve EXIT_SUCCESS si todo salio bien */
int copySecureFromSharedMemory(accel_data *dest){
    static int last_semaphore_used = 2;

    if(!working) return EXIT_FAILURE;


    if(last_semaphore_used == 2){
        if(sem_wait(&(sharedMemory_main->sem1))==0){
            /* Copio la ultima lectura y luego los datos filtrados */
            uint16_t pos = sharedMemory_main->firstFreePosition?sharedMemory_main->firstFreePosition-1:SHARED_MEMORY_SIZE-1;

            dest[0].accel_xout = sharedMemory_main->accel_xout_readings[pos];
            dest[0].accel_yout = sharedMemory_main->accel_yout_readings[pos];
            dest[0].accel_zout = sharedMemory_main->accel_zout_readings[pos];

            dest[0].gyro_xout = sharedMemory_main->gyro_xout_readings[pos];
            dest[0].gyro_yout = sharedMemory_main->gyro_yout_readings[pos];
            dest[0].gyro_zout = sharedMemory_main->gyro_zout_readings[pos];

            dest[0].temp_out = sharedMemory_main->temp_out_readings[pos];


            memcpy(&dest[1], &sharedMemory_main->filtered_read, sizeof(accel_data));
            
            sem_post(&sharedMemory_main->sem1);

            last_semaphore_used = 1;
            return EXIT_SUCCESS;
        }else{
            return EXIT_FAILURE;
        }
    }else{
        if(sem_wait(&(sharedMemory_main->sem2))==0){
            /* Copio la ultima lectura y luego los datos filtrados */
            uint16_t pos = sharedMemory_main->firstFreePosition?sharedMemory_main->firstFreePosition-1:SHARED_MEMORY_SIZE-1;

            dest[0].accel_xout = sharedMemory_main->accel_xout_readings[pos];
            dest[0].accel_yout = sharedMemory_main->accel_yout_readings[pos];
            dest[0].accel_zout = sharedMemory_main->accel_zout_readings[pos];

            dest[0].gyro_xout = sharedMemory_main->gyro_xout_readings[pos];
            dest[0].gyro_yout = sharedMemory_main->gyro_yout_readings[pos];
            dest[0].gyro_zout = sharedMemory_main->gyro_zout_readings[pos];

            dest[0].temp_out = sharedMemory_main->temp_out_readings[pos];
            
            memcpy(&dest[1], &sharedMemory_main->filtered_read, sizeof(accel_data));
            
            sem_post(&sharedMemory_main->sem2);
            last_semaphore_used = 2;
            return EXIT_SUCCESS;
        }else{
            return EXIT_FAILURE;
        }
    }
    return EXIT_FAILURE;
}

/* Manejador de las señales del sistema, recibe como parametro el numero de señal */
void sighandler(int signum){
    logg(warn, "Señal SIGINT recibida. Cerrando el proceso..");
    working = 0;
}

/* Manejador de la señal SIGCHL */
void sigchildhandler(int signum){
    logg(warn, "Señal SIGCHLD recibida.");
    int remaining_pids = 1;

    while (remaining_pids && working)
    {
        int status;
        int pid = waitpid(0, &status, WNOHANG);

        if(0 == pid) break;

        if(pid == reading_device_pid){
            logg(fatal, "Proceso de lectura del dispositivo cerrado de manera inesperada... y ahora quien podra ayudarnos?");
            kill_them_all();
            wait_for_all();
            exit(EXIT_FAILURE);
        }else{
            if(remover_proceso(pid)==EXIT_SUCCESS){
                logg(client_info, "Proceso cliente hijo cerrado.");
                if(cantidad_de_usuarios>0) cantidad_de_usuarios--;
            }else    
                logg(warn, "Proceso no identificado cerrado.");
        }
    }
}
void sigusr2handler(int sig){
    if(sig != SIGUSR2) return;

    //Nueva configuracion recibida

    if(cargarConfiguracion()){
        notificarCambios();
    }

    return;
}

/* Funcion que se queda esperadno a los procesos que abrimos. Previamente se deberia mandar kill SIGINT a los procesos. */
int wait_for_all(){

    if(reading_device_pid){
        waitpid(reading_device_pid, NULL, 0);
        reading_device_pid = 0;
        logg(info, "Proceso manejador de dispositivo cerrado con exito.");
    }

    /* Bastante fea esta manera de hacerlo */
    while(NULL != lista){
        int status;
        int pid = waitpid(0, &status, 0);
        if(-1 == pid) return -1;
        remover_proceso(pid);
    }
    return 1;
}

int main(){
    nfds_t nfds;
    struct pollfd fds[2];
    int poll_num=0;
    int time_out=5000;

    int sfd=0;
    struct sockaddr_in addr;

    logg(info,"Iniciando servidor...");

    //Registro las señales de todos los procesos
    struct sigaction saction;
    saction.sa_handler = sighandler;
    sigemptyset(&saction.sa_mask);
    saction.sa_flags = 0;

    sigaction (SIGINT, &saction, NULL);
    sigaction (SIGQUIT, &saction, NULL);

    saction.sa_handler = sigchildhandler;
    sigaction (SIGCHLD, &saction, NULL);

    saction.sa_handler = sigusr2handler;
    sigaction (SIGUSR2, &saction, NULL);
    
    //Inicializo la configuracion
    if(init_config()==EXIT_FAILURE){
        logg(fatal, "Error al cargar la configuracion.");
        exit(EXIT_FAILURE);
    }
    

    //Inicializo el servidor, lo hago primero porque si hay algun error es mas facil chocar la calesita sin los procesos instanciados..
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        logg(fatal, "Error al abrir el socket.\n");
        destroy_config();
        exit(EXIT_FAILURE);
    }

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(main_config.mdata.localport);
    addr.sin_family = AF_INET;
    bzero(&addr.sin_zero, 8); 

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr))<0){
        logg(fatal, "Error al bindear el socket.\n");
        destroy_config();
        exit(EXIT_FAILURE);
    }

    if (listen(sfd, main_config.mdata.backlog) < 0){
        logg(fatal, "Error al listenear el socket.\n");
        destroy_config();
        exit(EXIT_FAILURE);
    }

    //Agrego el socket al poll()
    nfds = 1;
    fds[0].fd = sfd;       
    fds[0].events = POLLIN | POLLERR;

    //una vez que tengo la configuracion lanzo el proceso que lee el dispositivo
    int fork_res = fork();

    if(fork_res < 0){
        logg(fatal, "Error al crear el proceso hijo de lectura del dispositivo.");
        kill_them_all();
        exit(EXIT_FAILURE);
    }
  
    if(0 == fork_res){
        signal(SIGUSR2, SIG_IGN);
        run_device_reading();
        //Si salgo de run device reading es porque me mandaron sigkil o algo por el estilo
        exit(EXIT_SUCCESS);
    }

    logg(info, "Proceso de lectura de dispositivo creado con exito.");
    reading_device_pid = fork_res;

    //Activo la memoria compartida para que los hijos de los clientes la puedan utilizar
    int fd = shm_open(SHARED_MEMORY_PATH, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (-1 == fd){
        perror("error al crear el descriptor");
        exit(1);
    }

    /* Mapeo la memoria compartida en el puntero. */
    sharedMemory_main = mmap(NULL, sizeof(*sharedMemory_main), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sharedMemory_main == MAP_FAILED)
        return 0;


    
    while(working){
        poll_num = poll(fds, nfds, time_out);
        if (-1 == poll_num) {
            //El error puede deberse a que entro una señal.. hago un continue para volver a evaluar el working y si esta en cero me voy silbado bajito.
            if(errno != EINTR)
                logg(fatal, "Error en el poll :(");
            continue;
        }

        if(0 == poll_num){ //Salio por timeout..
            char s[128];
            sprintf(s, "Usuarios activos %d, muestras: %lu", cantidad_de_usuarios, sharedMemory_main->sample_count);
            logg(info, s);
            continue;
        }
        //Chequeo los descriptores
        for (int j = 0; j < nfds; j++) {
            if((fds[j].revents & POLLERR)&&(0==j)){
                logg(fatal, "Error en el Socket del server. Choco la calesita...");
                close(sfd);
                working = 0;
                break;
            }

            if((fds[j].revents & POLLIN)&&(0==j)){
                //Tengo que atender una nueva conexion :)
                struct  sockaddr_in addr_cli;
                socklen_t largo = sizeof(addr_cli);
                int client_socket = accept(sfd, (struct sockaddr*)&addr_cli, &largo);
                if (client_socket < 0){
                    logg(warn, "Error al aceptar al cliente.");
                    continue;
                }

                //Primero chequeo que la configuracion me permita aceptar la conexion.
                if(cantidad_de_usuarios >= main_config.mdata.connections){
                    logg(warn, "Cantidad maxima de usuarios alcanzada. Rechazando conexion.");
                    close(client_socket);
                    continue;
                }

                //Si estoy aca estoy listo para atender al cliente. forkeo y lo atiendo.
                fork_res = fork();

                if(fork_res < 0){
                    logg(fatal, "Error al crear el proceso hijo para atender al cliente.");
                    close(client_socket);
                    continue;
                }
            
                if(0 == fork_res){
                    signal(SIGUSR2, SIG_IGN);
                    atender_cliente(client_socket);
                    //Si salgo de atender_cliente es porque me mandaron sigkil o algo por el estilo o el cliente cerro la conexion
                    exit(EXIT_SUCCESS);
                }

                //Cierro el socket que quedo en el padre.
                close(client_socket);

                char s[128];
                sprintf(s, "Proceso (%d) de atencion al cliente (IP: %s) creado con exito.", fork_res, inet_ntoa(addr_cli.sin_addr));
                logg(client_info, s);
                //Agrego el pid a la lista para waitearlo.
                agregar_proceso(fork_res);
                cantidad_de_usuarios++;
            }
        }
    }
    
    kill_them_all();
    wait_for_all();

    //Por si me quedo algun pids de casualidad me aseguro de devolver toda la memoria.
    vaciar_lista();

    //Cierro la memoria compartida
    shm_unlink(SHARED_MEMORY_PATH);

    //Cierro la configuracion
    destroy_config();

    exit(EXIT_SUCCESS);
}

/* Funcion que manda la señal SIGINT a todos los procesos que tengamos abiertos. */
int kill_them_all(){

    if(reading_device_pid){
        kill(reading_device_pid,SIGINT);
    }

    nodo *p = lista;

    //Mato a todos los hijos.
    while(NULL != p){
        kill(p->pid,SIGINT);
        p = p->next;
    }
    
    return 1;
}

/* Funcion que se encarga de atender al cliente, deberia llamarse despues de fork. */
int atender_cliente(int sock){
    float raw_accel_xout, raw_accel_yout, raw_accel_zout;
    float mva_accel_xout, mva_accel_yout, mva_accel_zout;
    float raw_gyro_xout, raw_gyro_yout, raw_gyro_zout;
    float mva_gyro_xout, mva_gyro_yout, mva_gyro_zout;
    float raw_temp_out, mva_temp_out;

    accel_data mem[2];
    char text[512];

    nfds_t nfds;
    struct pollfd fds[1];
    int n_bytes=0;

    time_t last_time_ka = 0;

    //Agrego el socket al poll()
    nfds = 1;
    fds[0].fd = sock;       
    fds[0].events = POLLOUT | POLLERR | POLLIN;

    /* Envio el OK al cliente */
    sprintf(text, "OK");

    if(write(sock, text, strlen(text))<=0){
        logg(warn, "Error en el handshake con el cliente [OK1]. Abortando conexion.");
        working = 0;
    }else {
        /* Espero recibien el AKN del cliente */
        n_bytes = recv(sock, text, 512, 0);
        text[n_bytes]= '\0';

        if(strcmp(text, "AKN")){
            logg(warn, "Error en el handshake con el cliente [AKN]. Abortando conexion.");
            working = 0;
        }else {
            sprintf(text, "OK");
            if(write(sock, text, strlen(text))<=0){
                logg(warn, "Error en el handshake con el cliente [OK2]. Abortando conexion.");
                working = 0;
            }
        }
    }

    while(working){

        if(copySecureFromSharedMemory(mem) == EXIT_SUCCESS){

            // Compruebo si el KeepAlive esta dentro del limite de tiempo.
            if((last_time_ka) && ((time(NULL) - last_time_ka) > KA_TIMEOUT)) {
                logg(warn, "KA Timeout. Abortando conexion.");
                working = 0;
                break;
            }

            raw_accel_xout = mem[0].accel_xout / (float)FS_ACCEL;
            raw_accel_yout = mem[0].accel_yout / (float)FS_ACCEL;
            raw_accel_zout = mem[0].accel_zout / (float)FS_ACCEL;

            mva_accel_xout = mem[1].accel_xout / (float)FS_ACCEL;
            mva_accel_yout = mem[1].accel_yout / (float)FS_ACCEL;
            mva_accel_zout = mem[1].accel_zout / (float)FS_ACCEL;

            raw_gyro_xout = mem[0].gyro_xout / (float)FS_GYRO;
            raw_gyro_yout = mem[0].gyro_yout / (float)FS_GYRO;
            raw_gyro_zout = mem[0].gyro_zout / (float)FS_GYRO;

            mva_gyro_xout = mem[1].gyro_xout / (float)FS_GYRO;
            mva_gyro_yout = mem[1].gyro_yout / (float)FS_GYRO;
            mva_gyro_zout = mem[1].gyro_zout / (float)FS_GYRO;
            
            raw_temp_out = mem[0].temp_out / 340.0 + 36.53;
            mva_temp_out = mem[1].temp_out / 340.0 + 36.53;

            sprintf(text, "%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n%.5f\n", raw_accel_xout, raw_accel_yout, raw_accel_zout, raw_gyro_xout, raw_gyro_yout,
                        raw_gyro_zout ,raw_temp_out, mva_accel_xout, mva_accel_yout, mva_accel_zout, mva_gyro_xout, mva_gyro_yout, mva_gyro_zout , mva_temp_out) ;

            if(0==poll(fds,nfds,0)) continue;

            if(fds[0].revents & POLLERR){
                //El socket tiro un error, seguramente cerraron la conexion desde el cliente.
                working = 0;
                break;
            }
            if(fds[0].revents & POLLOUT){
                //El socket esta listo para ser escrito
                if(write(sock, text, strlen(text))<=0){
                    logg(warn, "Error al escribir datos al cliente. Abortando conexion.");
                    working = 0;
                    break;
                }
            }
            if(fds[0].revents & POLLIN){
                //El socket esta listo para ser leido, uso la version no bloqueante del recv.-
                if((n_bytes=recv(sock, text, 512, MSG_DONTWAIT))<0){
                    logg(warn, "Error al recibir el KeepAlive. Abortando conexion.");
                    working=0;
                }else{
                    //Fuerzo el \0 para poder comparar strings.
                    text[n_bytes] = '\0';
                    if(strcmp(text, "KA")){
                        logg(warn, "Error al recibir el KeepAlive. Abortando conexion.");
                        working=0;
                    }else{
                        //KA recibido, actualizo el contador.
                        last_time_ka = time(NULL);
                    }
                }
            }
        }else{
            working = 0;
            break;
        }
    }
    //Si sali del while es porque o el cliente cerro la conexion o el server se cae a pedazos. Cierro el socket y mato el proceso cliente.
    close(sock);
    
    logg(client_info, "Termino la comunicacion con el cliente.");
    return EXIT_SUCCESS;
}

/* Funcion que se encarga de agregar el pid a una lista de pids para poder matarlos en caso de que muera el padre. */
int agregar_proceso(int pid){

    //Si la lista esta vacia, agrego y me voy
    if(NULL == lista){
        lista = (nodo*)malloc(sizeof(nodo));

        lista->next = NULL;
        lista->pid = pid;
        return EXIT_SUCCESS;
    }

    //Si no esta vacia avanzo el puntero hasta el final de la lista.
    nodo *p = lista;
    while(NULL != p->next) p = p->next;

    //Creo un nuevo nodo
    p->next = (nodo*)malloc(sizeof(nodo));

    //Avanzo el puntero y cargo el pid.
    p = p->next;
    p->next = NULL;
    p->pid = pid;

    return EXIT_SUCCESS;
}

/* Funcion que se encarga de remover el pid de la lista de pids. */
int remover_proceso(int pid){

    nodo *p = lista;
    while((NULL != p) && (NULL != p->next) && (p->next->pid != pid)) p = p->next;

    if(NULL == p) return EXIT_FAILURE;

    //si p->next es NULL es porque estoy en el final de la lista y no encontre el PID o hay un solo elemento en la lista
    if(NULL == p->next){
        if(p->pid == pid){
            free(p);
            lista = NULL;
            return EXIT_SUCCESS;
        }

        return EXIT_FAILURE;
    } 

    if(p->next->pid == pid){
        nodo *remove = p->next;
        p->next = remove->next;
        free(remove);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
    
}

/* Esta funcion vacia la lista para cerrar el programa elegantemente */
int vaciar_lista(){
    while(NULL != lista){
        nodo *p =lista->next;
        free(lista);
        lista = p;
    }
    return EXIT_SUCCESS;
}

/* Devuelve el primer pid de la lista y remueve el nodo */
int pick_pid(){
    int pid = -1;
    if(NULL != lista){
        pid = lista->pid;
        nodo *p = lista->next;
        free(lista);
        lista = p;
    }
    return pid;
}