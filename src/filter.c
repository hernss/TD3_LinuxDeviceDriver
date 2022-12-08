#include "filter.h"

/* Esta funcion calcula la media movil de la longitud pasada como parametro, hace uso de la memoria compartida es por esto que debe llamarse dentro de la proteccion de los semaforos */
int16_t recalcularFiltro(int16_t * origen, uint16_t window, uint16_t last_value, uint16_t size){

    long acumulador = 0;

    if(0 == window) return 0; //Para evitar la division por 0
    if(window > size) window = size; //Para evitar dar vueltas por el buffer


    if(last_value >= window){
        for(int i=last_value - window;i<last_value;i++){
            acumulador += origen[i];
        }
        acumulador /= window;
        return (int16_t)acumulador;
    }

    for(int i=0;i<last_value;i++){
        acumulador += origen[i];
    }

    for(int i=size-window+last_value;i<size;i++){
        acumulador += origen[i];
    }

    acumulador /= window;
    return (int16_t)acumulador;
}
