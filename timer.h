#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_TIMER_COUNT 1000

typedef enum
{
    TIMER_SINGLE_SHOT = 0, /*Periodic Timer*/
    TIMER_PERIODIC         /*Single Shot Timer*/
} t_timer;

typedef void (*time_handler)(void);

int initialize();
size_t start_timer(unsigned int interval, time_handler handler, t_timer type, void *user_data);
void stop_timer(size_t timer_id);
void finalize();