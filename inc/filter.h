#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/inotify.h>

int16_t recalcularFiltro(int16_t * origen, uint16_t window, uint16_t last_value, uint16_t size);