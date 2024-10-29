#ifndef PE_COMMON_H
#define PE_COMMON_H

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L
#define MAX_PRODUCT_LENGTH 16
#define MAX_ORDER_TYPE_LENGTH 10
#define BUFFER_SIZE 256
#define MAX_QTY 1000


#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"
#define FEE_PERCENTAGE 1

#endif