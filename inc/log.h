#ifndef LOG_H
#define LOG_H 1

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define fDebug_File 0
#define fDebug_Screen 1

int logg(int log_level, char* text);
char* log_level_to_str(int level, char* str);
char* get_color(int, char*);

enum log_level {
    info = 0,
    warn,
    fatal,
    client_info
};

#endif