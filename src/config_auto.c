#include "config_auto.h"

extern int working;
extern char queue_main[];
extern char queue_device[];

//Variables globales de configuracion
mq_data_type config;

//Descriptores de las colas de mensaje
mqd_t qd_device=0, qd_main=0;   // queue descriptors

/* Carga la configuracion desde el archivo de configuracion, si el archivo no existe crea uno con los valores por defecto y los lee. Si no hay cambios devuelve 0, sino devuele la cantidad de parametros que cambiaron */
int cargarConfiguracion(){


    char buff[BUFFSIZE];
    FILE* f = fopen(CONFIG_PATH, "r");

    if(f == NULL){
        logg(info, "No existe el archivo de configuracion, creando uno por defecto...");
        //Creo un archivo vacio
        f= fopen(CONFIG_PATH,"w+");

        if(f == NULL){
            logg(fatal, "No se pudo crear el archivo de configuracion, el programa no puede seguir su ejecucion.");
            exit(-1);
        }

        fprintf(f, "# Este archivo fue generado de forma automatica por el sistema.\n");
        fprintf(f, "# El simbolo # al inicio de la linea invalida la misma y no es teniada en cuenta.\n");
        fprintf(f, "# Los parametros configurables son los siguientes: backlog connections readtime mediacount.\n");
        fprintf(f, "# El formato de cada linea sera del estido clave=valor cualquier otro formato sera descartado.\n");
        fprintf(f, "backlog=2\nconnections=1000\nreadtime=1\nmediacount=5\nlocalport=4444\n");
        fclose(f);

        f = fopen(CONFIG_PATH, "r");

        if(f == NULL){
            logg(fatal, "No se pudo crear el archivo de configuracion, el programa no puede seguir su ejecucion.");
            exit(-1);
        }
    }
    
    int hay_cambio = 0;
    //recorro el archivo linea a linea en busqueda de la configuracion
    while(fgets(buff, BUFFSIZE, f) != NULL){
        if(buff[0] == '#') continue;            //Me salto la linea si el caracter # esta al inicio de la linea

        char* s = strtok(buff, "=");
        int* dest=NULL;
        if(!strcmp(s, "backlog")){
            //backog
            dest = &config.mdata.backlog;
        }else if(!strcmp(s, "connections")){
            //connections
            dest = &config.mdata.connectios;
        }else if(!strcmp(s, "readtime")){
            //readtime
            dest = &config.mdata.readtime;
        }else if(!strcmp(s, "mediacount")){
            //mediacount
            dest = &config.mdata.mediacount;
        }else if(!strcmp(s, "localport")){
            //mediacount
            dest = &config.mdata.localport;
        }else{
            dest = NULL;
        }

        if(dest != NULL){           
            s = strtok(NULL, "=");
            if(*dest != atoi(s)){
                *dest = atoi(s);
                hay_cambio++;
            }
        }
    }

    fclose(f);

    return hay_cambio;
}

/* Notifica la configuracion a los procesos hijos que estan escuchando por las colas de mensajes */
int notificarCambios(){
    if(0 != qd_device) //si por algun motivo no tengo la cola de mensajes configurada..   
        if (mq_send (qd_device, (char *)&config, sizeof(config), 0) == -1) {
            logg(fatal, "No se pudo enviar la nueva configuracion al proceso read device.");
            return 0;
        }
    
    if(0 != qd_main) //si por algun motivo no tengo la cola de mensajes configurada..   
        if (mq_send (qd_main, (char *)&config, sizeof(config), 0) == -1) {
            logg(fatal, "No se pudo enviar la nueva configuracion al proceso main.");
            return 0;
        }
    
    return 1;
}


int run_config_reading(){
   

    logg(info, "Abriendo archivo de configuracion.");
    //Cargo los valores por defecto
    config.mdata.backlog = 2;
    config.mdata.connectios = 1000;
    config.mdata.readtime = 1;
    config.mdata.mediacount = 5;
    config.mdata.localport = 4444;

    cargarConfiguracion();

    //Antes de poder notificar la configuracion creo la cola de mensajes para mandar la configuracion a los procesos.
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(config);
    attr.mq_curmsgs = 0;
    
    //Inicializo la cola de mensajes con el proceso main
    if ((qd_main = mq_open (queue_main, O_WRONLY | O_CREAT, 0660, &attr)) == -1) {
        logg(fatal, "Error al abrir la cola de mensajes con el proceso main");
        return EXIT_FAILURE;
    }
    //Inicializo la cola de mensajes con el proceso read device
    if ((qd_device = mq_open (queue_device, O_WRONLY | O_CREAT, 0660, &attr)) == -1) {
        logg(fatal, "Error al abrir la cola de mensajes con el proceso device");
        return EXIT_FAILURE;
    }

    notificarCambios();
    logg(info, "Configuracion cargada con exito.");

    //Creo la instancia de inotify
    size_t len;
    int fd, poll_num;
    int wd;
    char buffer[4096];
    const struct inotify_event *event;
    nfds_t nfds;
    struct pollfd fds[1];

    fd = inotify_init1(IN_NONBLOCK);

    if(fd<0){
        logg(fatal, "No se puede crear la instancia inotify. No se controlan los cambios en la configuracion");
        working = -1;
    } else {
        //Suscribo el archivo de configuracion al inotify
        wd = inotify_add_watch(fd, CONFIG_PATH, IN_MODIFY);

        if(wd < 0){
            logg(fatal, "No se puede crear la instancia inotify. No se controlan los cambios en la configuracion");
            working = -1;
        }

        //Configuro el poll
        nfds = 1;
        fds[0].fd = fd;                 /* Inotify input */
        fds[0].events = POLLIN;
    }

    while(1 == working){
        poll_num = poll(fds, nfds, 1000);
        if (-1 == poll_num) {
            logg(fatal, "Error en el poll :(");
            continue;
        }

        //Si devolvio cero no tengo nada para hacer, vuelvo a empezar el while y me bloqueo en el poll
        if (0 == poll_num) continue;

        if (fds[0].revents & POLLIN) {
            len = read(fd, buffer, EVENT_BUF_LEN);

            if (len <= 0){
                //Si entre una senial me hace desbloquear le read con -1.. le doy un segundo para que la procese y luego continuo el while.
                //si la señal fue un sigint ya deberia haber cambiado el working y finalizar el proceso
                sleep(1);
                continue;
            }
            
            for (char *ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
                event = (const struct inotify_event *) ptr;

                if ( event->mask & IN_MODIFY ) {
                    logg(info, "Se detecto cambios en el archivo de configuracion. Actualizando datos.");
                    if(cargarConfiguracion())   //Si hay cambios en el archivo cargar configuracion devuelve 1 sino devuelve 0
                        notificarCambios();
                }
            }
        }
        sleep(1);
    }

    //Si working esta en -1 es porque hubo una condicion de error en el inotify y no estoy trackeando los cambios en el archivo de configuracion.
    //Me quedo aca esperando hasta que la llamada a SIGINT me ponga working en 0
    while(-1 == working)
    {
        sleep(1);
    }

    //Cierro la instancia de inotify
    inotify_rm_watch(fd, wd);
    close(fd);
    
    // to destroy the message queue
    mq_unlink(queue_main);
    mq_unlink(queue_device);
    if (mq_close (qd_device) == -1) {
        logg(fatal, "Error al cerrar la cola de mensajes");
        return EXIT_FAILURE;
    }
    if (mq_close (qd_main) == -1) {
        logg(fatal, "Error al cerrar la cola de mensajes");
        return EXIT_FAILURE;
    }

    logg(info, "Cerrando archivo de configuracion");
    return EXIT_SUCCESS;
}