#include "config_async.h"

extern int working;
extern char queue_main[];
extern char queue_device[];

//Variables globales de configuracion
mq_data_type main_config;

//Descriptores de las colas de mensaje
mqd_t qd_device=0;   // queue descriptors

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
        fprintf(f, "backlog=20\nconnections=500\nfiltercount=5\nlocalport=4444\n");
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
            dest = &main_config.mdata.backlog;
        }else if(!strcmp(s, "connections")){
            //connections
            dest = &main_config.mdata.connections;
        }else if(!strcmp(s, "filtercount")){
            //mediacount
            dest = &main_config.mdata.filtercount;
        }else if(!strcmp(s, "localport")){
            //mediacount
            dest = &main_config.mdata.localport;
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
    if(0 != qd_device){ //si por algun motivo no tengo la cola de mensajes configurada..   
        if (mq_send (qd_device, (char *)&main_config, sizeof(main_config), 0) == -1) {
            logg(fatal, "No se pudo enviar la nueva configuracion al proceso read device.");
            return 0;
        }
    }
    return 1;
}


int init_config(){
  
    logg(info, "Abriendo archivo de configuracion.");
    //Cargo los valores por defecto
    main_config.mdata.backlog = 20;
    main_config.mdata.connections = 500;
    main_config.mdata.filtercount = 5;
    main_config.mdata.localport = 4444;

    cargarConfiguracion();

    //Antes de poder notificar la configuracion creo la cola de mensajes para mandar la configuracion a los procesos.
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(main_config);
    attr.mq_curmsgs = 0;

    //Inicializo la cola de mensajes con el proceso read device
    if ((qd_device = mq_open (queue_device, O_WRONLY | O_CREAT, 0660, &attr)) == -1) {
        logg(fatal, "Error al abrir la cola de mensajes con el proceso device");
        return EXIT_FAILURE;
    }

    notificarCambios();
    logg(info, "Configuracion cargada con exito.");
    return EXIT_SUCCESS;

}

int destroy_config(){
    // to destroy the message queue
    mq_unlink(queue_device);
    if (mq_close (qd_device) == -1) {
        logg(fatal, "Error al cerrar la cola de mensajes");
        return EXIT_FAILURE;
    }

    logg(info, "Cerrando archivo de configuracion");
    return EXIT_SUCCESS;
}