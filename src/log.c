
#include "log.h"


int logg(int log_level, char* text){

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char fecha[128];
    char level[128];
    char color[] = "\033[0;31m";

    strftime(fecha, sizeof(fecha),"%d-%m-%Y %X", tm);

    if(fDebug_File){
        FILE* f = fopen("debug.log", "a");
        fprintf(f, "[%s] [%s]: %s\n", fecha, log_level_to_str(log_level, level), text);
        fclose(f);
    }

    if(fDebug_Screen){
        printf("%s[%s] [%s]\t[PID:%d]: %s\033[0m\n", get_color(log_level, color), fecha, log_level_to_str(log_level, level), getpid(), text);
    }

/*
NEGRO \033[0;30m
ROJO \033[0;31m
VERDE \033[0;32m
AMARILLO \033[0;33m
AZUL \033[0;34m
MAGENTA \033[0;35m
CYAN \033[0;36m
BLANCO \033[0;37m
NEGRO (negrita) \033[1;30m
ROJO (negrita) \033[1;31m
VERDE (negrita) \033[1;32m
AMARILLO (negrita) \033[1;33m
AZUL (negrita) \033[1;34m
MAGENTA (negrita) \033[1;35m
CYAN (negrita) \033[1;36m
BLANCO (negrita) \033[1;37m

RESET \033[0m
*/
    return 1;
}

char* get_color(int level, char* color){
    switch (level)
    {
    case info:
        strcpy(color,"\033[0;32m");
        break;
    case warn:
        strcpy(color,"\033[0;33m");
        break;
    case fatal:
        strcpy(color,"\033[0;31m");
        break;
    case client_info:
        strcpy(color,"\033[0;34m");
        break;
    default:
        strcpy(color,"\033[0;35m");
        break;
    }
    
    return color;
}



char* log_level_to_str(int level, char* str){

switch (level)
{
case info:
    strcpy(str, "INFO");
    break;
case warn:
    strcpy(str, "WARN");
    break;
case fatal:
    strcpy(str, "FAULT");
    break;
case client_info:
    strcpy(str, "CLIENT");
    break;
default:
    strcpy(str, "DEFAULT");
    break;
}

return str;
}