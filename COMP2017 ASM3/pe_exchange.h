#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"
int num_traders = 0;
int num_products;
int disconnected_traders = 0;
int exit_loop = 0;
int *pe_exchange = NULL;
int *pe_trader = NULL;
int num_orders = 0;
char (*products)[MAX_PRODUCT_LENGTH+1];
#define LOG_PREFIX "[PEX]"

#endif
