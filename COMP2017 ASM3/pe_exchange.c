/**
 * comp2017 - assignment 3
 * <Hangyu Ning>
 * <hnin3645>
 */

#include "pe_exchange.h"

typedef struct {
    char product[MAX_PRODUCT_LENGTH];
    long long value;
    long long positions;
} TraderProduct;

typedef struct {
    pid_t pid;
    long long fee;
    int order_id;
    TraderProduct *trader_product;
} TraderProcess;

TraderProcess *trader_processes;

typedef struct {
    int qty;
    int num_orders;
    int trader_id;
} OrderDetail;

typedef struct {
    char order_type[MAX_ORDER_TYPE_LENGTH];
    int price;
    OrderDetail *details;
    int num;
    int real_num;
} Order;

typedef struct {
    Order *orders;
    char product[MAX_PRODUCT_LENGTH];
    int order_count;
    int real_order_count;
} OrderList;

OrderList *order_lists;

void sigchld_handler(int signal) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int trader_id = -1;
        for (int i = 0; i < num_traders; i++) {
            if (trader_processes[i].pid == pid) {
                trader_id = i;
                break;
            }
        }
        printf(LOG_PREFIX" Trader %d disconnected\n", trader_id);
        disconnected_traders++;

        if (disconnected_traders == num_traders) {
            printf(LOG_PREFIX" Trading completed\n");
            printf(LOG_PREFIX" Exchange fees collected: $%lld\n",trader_processes[0].fee);
            exit_loop = 1;
        }
    }
}

void broadcast_market_order(char* order_type, char* product, int qty, int price, int trader_id) {
    char market_order[BUFFER_SIZE];
    snprintf(market_order, BUFFER_SIZE, "MARKET %s %s %d %d;", order_type, product, qty, price);
    // Broadcast market order to all traders
    for (int i = 0; i < num_traders; i++) {
        if (i != trader_id) { // Exclude the trader who made the order
            ssize_t res = write(pe_exchange[i], market_order, strlen(market_order));
            if (res == -1 && errno == EPIPE) {

            }else{
                kill(trader_processes[i].pid, SIGUSR1);
            }
            
        }
    }
}

void is_order_match(int trader_id, char* order_type, int num_orders, char* product, int* qty, int price, Order* order2, char* order2_product) {
    if (strcmp(order2->order_type, "BUY") == 0 && strcmp(order_type, "SELL") == 0 && order2->price >= price){
        for(int k = 0; k < order2->num; k++){
            char feedback[BUFFER_SIZE];
            if(*qty > order2->details[k].qty){ 
 
                int value;
                int fee;
                int match_qyt;
                
                value = order2->details[k].qty * order2->price;
                fee = (int)(value * 0.01 + 0.5);
                match_qyt = order2->details[k].qty;
                
                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,match_qyt);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }

                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,match_qyt);
                write(pe_exchange[trader_id], feedback, strlen(feedback));
                
                

                for(int p = 0; p < num_products; p++){
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, order2_product)== 0){
                        trader_processes[order2->details[k].trader_id].trader_product[p].value -= value;
                        trader_processes[order2->details[k].trader_id].trader_product[p].positions += match_qyt;
                    }
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, product)== 0){
                        trader_processes[trader_id].trader_product[p].value += value;
                        trader_processes[trader_id].trader_product[p].value -= fee;
                        trader_processes[trader_id].trader_product[p].positions -= match_qyt;
                    }
                }
                trader_processes[0].fee += fee;
                *qty -= match_qyt;
                order2->details[k].qty = 0;
                order2->details[k].num_orders = -1;
                order2->real_num -= 1;
            }
            else if(*qty == order2->details[k].qty){

                int value;
                int fee;

                value = order2->details[k].qty * order2->price;
                fee = (int)(value * 0.01 + 0.5);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,order2->details[k].qty);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }

                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,order2->details[k].qty);
                write(pe_exchange[trader_id], feedback, strlen(feedback));

                for(int p = 0; p < num_products; p++){
                    trader_processes[order2->details[k].trader_id].trader_product[p].value -= value;
                    trader_processes[order2->details[k].trader_id].trader_product[p].positions += order2->details[k].qty;

                    trader_processes[trader_id].trader_product[p].value += value;
                    trader_processes[trader_id].trader_product[p].value -= fee;
                    trader_processes[trader_id].trader_product[p].positions -= order2->details[k].qty;
                }
                trader_processes[0].fee += fee;

                *qty = 0;
                order2->details[k].qty = 0;
                order2->details[k].num_orders = -1;
                order2->real_num -= 1;
            }
            else{

                int value;
                int fee;
                int match_qyt;

                value = *qty * order2->price;
                fee = (int)(value * 0.01 + 0.5);
                match_qyt = *qty;

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,match_qyt);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }

                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,match_qyt);
                write(pe_exchange[trader_id], feedback, strlen(feedback));

                

                for(int p = 0; p < num_products; p++){
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, order2_product)== 0){
                        trader_processes[order2->details[k].trader_id].trader_product[p].value -= value;
                        trader_processes[order2->details[k].trader_id].trader_product[p].positions += match_qyt;
                    }
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, product)== 0){
                        trader_processes[trader_id].trader_product[p].value += value;
                        trader_processes[trader_id].trader_product[p].value -= fee;
                        trader_processes[trader_id].trader_product[p].positions -= match_qyt;
                    }
                }
                trader_processes[0].fee += fee;

                order2->details[k].qty -= *qty;
                *qty = 0;
                
            }
        }
    }
    if (strcmp(order2->order_type, "SELL") == 0 && strcmp(order_type, "BUY") == 0 && order2->price <= price){
        for(int k = 0; k < order2->num; k++){
            char feedback[BUFFER_SIZE];
            if(*qty > order2->details[k].qty){ 
 
                long long value;
                long long fee;
                int match_qyt;

                value = (long long)order2->details[k].qty * order2->price;
                fee = (long long)(value * 0.01 + 0.5);
                match_qyt = order2->details[k].qty;
                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%lld, fee: $%lld.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);
         
                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,match_qyt);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }
                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,match_qyt);
                write(pe_exchange[trader_id], feedback, strlen(feedback));
                
                for(int p = 0; p < num_products; p++){
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, order2_product)== 0){
                        trader_processes[order2->details[k].trader_id].trader_product[p].value += value;
                        trader_processes[order2->details[k].trader_id].trader_product[p].positions -= match_qyt;
                    }
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, product)== 0){
                        trader_processes[trader_id].trader_product[p].value -= value;
                        trader_processes[trader_id].trader_product[p].value -= fee;
                        trader_processes[trader_id].trader_product[p].positions += match_qyt;
                    }
                }
                trader_processes[0].fee += fee;
                *qty -= match_qyt;
                order2->details[k].qty = 0;
                order2->details[k].num_orders = -1;
                order2->real_num -= 1;
            }
            else if(*qty == order2->details[k].qty){

                int value;
                int fee;

                value = order2->details[k].qty * order2->price;
                fee = (int)(value * 0.01 + 0.5);

                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,order2->details[k].qty);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,order2->details[k].qty);
                write(pe_exchange[trader_id], feedback, strlen(feedback));

                for(int p = 0; p < num_products; p++){
                    trader_processes[order2->details[k].trader_id].trader_product[p].value += value;
                    trader_processes[order2->details[k].trader_id].trader_product[p].positions -= order2->details[k].qty;

                    trader_processes[trader_id].trader_product[p].value -= value;
                    trader_processes[trader_id].trader_product[p].value -= fee;
                    trader_processes[trader_id].trader_product[p].positions += order2->details[k].qty;
                }
                trader_processes[0].fee += fee;

                *qty = 0;
                order2->details[k].qty = 0;
                order2->details[k].num_orders = -1;
                order2->real_num -= 1;
            }
            else{

                int value;
                int fee;
                int match_qyt;

                value = *qty * order2->price;
                fee = (int)(value * 0.01 + 0.5);
                match_qyt = *qty;

                printf(LOG_PREFIX" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", order2->details[k].num_orders, order2->details[k].trader_id, num_orders, trader_id, value, fee);

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", order2->details[k].num_orders,match_qyt);
                ssize_t res = write(pe_exchange[order2->details[k].trader_id], feedback, strlen(feedback));

                if (res == -1 && errno == EPIPE) {

                }

                snprintf(feedback, BUFFER_SIZE, "FILL %d %d;", num_orders,match_qyt);
                write(pe_exchange[trader_id], feedback, strlen(feedback));

                

                for(int p = 0; p < num_products; p++){
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, order2_product)== 0){
                        trader_processes[order2->details[k].trader_id].trader_product[p].value += value;
                        trader_processes[order2->details[k].trader_id].trader_product[p].positions -= match_qyt;
                    }
                    if (strcmp(trader_processes[order2->details[k].trader_id].trader_product[p].product, product)== 0){
                        trader_processes[trader_id].trader_product[p].value -= value;
                        trader_processes[trader_id].trader_product[p].value -= fee;
                        trader_processes[trader_id].trader_product[p].positions += match_qyt;
                    }
                }
                trader_processes[0].fee += fee;

                order2->details[k].qty -= *qty;
                *qty = 0;
                
            }
        }
    }
}

void process_command(char *command, int trader_id) {
    // Initialize variables for parsed command
    char order_type[MAX_ORDER_TYPE_LENGTH];
    int num_orders;
    char product[MAX_PRODUCT_LENGTH];
    int qty;
    int price;
    int parsed;
    char feedback[BUFFER_SIZE];
    char *command_before_semicolon;
    
    // Parse the command

    

    command_before_semicolon = strtok(command, ";");
    printf(LOG_PREFIX" [T%d] Parsing command: <%s>\n", trader_id, command_before_semicolon);

    sscanf(command, "%s", order_type);
    bool buysell_right_format = false;
    bool cancel_format = false;
    bool amend_format = false;
    if(strcmp(order_type, "SELL") == 0 || strcmp(order_type, "BUY") == 0){
        parsed = sscanf(command, "%s %d %s %d %d", order_type, &num_orders, product, &qty, &price);
        if(parsed ==5 && trader_processes[trader_id].order_id == num_orders){
            for (int i = 0; i < num_products; i++) {
                if(strcmp(products[i], product) == 0 && 0 < qty && qty < 999999 && 0 < price && price < 999999){
                    buysell_right_format = true;
                }
            }
        }
    }
    else if (strcmp(order_type, "AMEND") == 0) {
        parsed = sscanf(command, "%s %d %d %d", order_type, &num_orders, &qty, &price);
        if(parsed == 4){
            if(0 < qty && qty < 999999 && 0 < price && price < 999999){
                for (int i = 0; i < num_products; i++) {
                    for (int j = 0; j < order_lists[i].order_count; j++) {
                        for(int k = 0; k < order_lists[i].orders[j].num; k++){
                            if (order_lists[i].orders[j].details[k].trader_id == trader_id && order_lists[i].orders[j].details[k].num_orders == num_orders && order_lists[i].orders[j].details[k].qty != 0){
                                // printf("%d\n", order_lists[i].orders[j].details[k].qty);
                                order_lists[i].orders[j].details[k].qty=qty;
                                order_lists[i].orders[j].price=price;
                                amend_format = true;
                                strcpy(order_type, order_lists[i].orders[j].order_type);
                            }
                        }
                    }
                }
            }
        } 
    }
    else if (strcmp(order_type, "CANCEL") == 0) {
        parsed = sscanf(command, "%s %d", order_type, &num_orders);
        if(parsed == 2){
            for (int i = 0; i < num_products; i++) {
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    for(int k = 0; k < order_lists[i].orders[j].num; k++){
                        if (order_lists[i].orders[j].details[k].trader_id == trader_id && order_lists[i].orders[j].details[k].num_orders == num_orders && order_lists[i].orders[j].details[k].qty != 0){
                            // printf("%d\n", order_lists[i].orders[j].details[k].qty);
                            order_lists[i].orders[j].details[k].qty=0;
                            order_lists[i].orders[j].num -= 1;
                            order_lists[i].orders[j].real_num -=1;
                            cancel_format = true;
                            strcpy(order_type, order_lists[i].orders[j].order_type);
                        }
                    }
                }
            }
        }        
    }
     
    if (buysell_right_format) {
        trader_processes[trader_id].order_id++;
        snprintf(feedback, BUFFER_SIZE, "ACCEPTED %d;", num_orders);
        write(pe_exchange[trader_id], feedback, strlen(feedback));
        if (!disconnected_traders){
            broadcast_market_order(order_type, product, qty, price, trader_id);
        }
        for (int i = 0; i < num_products; i++) {
            for (int j = 0; j < order_lists[i].order_count - 1; j++) {
                for (int k = j + 1; k < order_lists[i].order_count; k++) {
                    if (order_lists[i].orders[j].price < order_lists[i].orders[k].price) {
                        Order temp = order_lists[i].orders[j];
                        order_lists[i].orders[j] = order_lists[i].orders[k];
                        order_lists[i].orders[k] = temp;
                    }
                }
            }
            //
            if (strcmp(order_lists[i].product, product) == 0){
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    Order *order = &order_lists[i].orders[j];
                    is_order_match(trader_id, order_type, num_orders, product, &qty, price, order, order_lists[i].product);
                }
            }
        }
        if (qty != 0){
            bool found_matching_order = false;
            // Check for matching order in the order book
            for (int i = 0; i < num_products; i++) {
                if( strcmp(order_lists[i].product, product) == 0){
                    for (int j = 0; j < order_lists[i].order_count; j++) {
                        order_lists[i].real_order_count++;

                        Order *new_order = &order_lists[i].orders[j];
                        if (strcmp(new_order->order_type, order_type) == 0 &&
                            new_order->price == price ) {                    
                            // If matching new_order found, increment num and return
                            new_order->details = realloc(new_order->details, (new_order->num + 1) * sizeof(OrderDetail));
                            new_order->details[new_order->num].qty = qty;
                            new_order->details[new_order->num].num_orders = num_orders;
                            new_order->details[new_order->num].trader_id = trader_id;
                            new_order->num++;
                            new_order->real_num++;

                            found_matching_order = true;
                            break;
                        }
                    }
                }     
            }
            // If no matching order found, add a new order
            // Find the correct product in order_lists
            if (!found_matching_order) {
                for (int i = 0; i < num_products; i++) {
                    // Allocate or reallocate memory for orders
                    if (strcmp(order_lists[i].product, product) == 0){
                        if (order_lists[i].orders == NULL) {
                            order_lists[i].orders = malloc(sizeof(Order));
                        } else {
                            order_lists[i].orders = realloc(order_lists[i].orders, (order_lists[i].order_count + 1) * sizeof(Order));
                        }
                        // Check if allocation failed
                        if (order_lists[i].orders == NULL) {
                            fprintf(stderr, "Memory allocation failed\n");
                            exit(1);
                        }
                        order_lists[i].real_order_count++;

                        Order *new_order = &order_lists[i].orders[order_lists[i].order_count++];
                        strcpy(new_order->order_type, order_type);

                        new_order->details = malloc(sizeof(OrderDetail));
                        new_order->details[0].qty = qty;
                        new_order->details[0].num_orders = num_orders;
                        new_order->details[0].trader_id = trader_id;
                        new_order->price = price;
                        new_order->num = 1;
                        new_order->real_num = 1;
                    }
                }
            }
        }
        printf(LOG_PREFIX"\t--ORDERBOOK--\n");
        for (int i = 0; i < num_products; i++) {
            if (order_lists[i].real_order_count != 0){
                int buy_count = 0, sell_count = 0;
                // Count the number of buy and sell orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0 && order_lists[i].real_order_count != 0){
                        Order *order = &order_lists[i].orders[j];
                        if (strcmp(order->order_type, "BUY") == 0) {
                            buy_count++;
                        } else if (strcmp(order->order_type, "SELL") == 0) {
                            sell_count++;
                        }
                    }    
                }
                // Print product information
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: %d; Sell levels: %d\n", order_lists[i].product, buy_count, sell_count);
                // If there are any orders, print them
                // Sort orders by price in descending order
                for (int j = 0; j < order_lists[i].order_count - 1; j++) {
                    for (int k = j + 1; k < order_lists[i].order_count; k++) {
                        if (order_lists[i].orders[j].price < order_lists[i].orders[k].price) {
                            Order temp = order_lists[i].orders[j];
                            order_lists[i].orders[j] = order_lists[i].orders[k];
                            order_lists[i].orders[k] = temp;
                        }
                    }
                }
                // Print orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0){
                        Order *order = &order_lists[i].orders[j];
                        int qty_sum = 0;
                        for (int k = 0; k < order->num; k++) {
                            if(order->details[k].qty != 0){
                                qty_sum += order->details[k].qty;
                            }
                        }
                        printf(LOG_PREFIX"\t\t%s %d @ $%d (%d %s)\n", order->order_type, qty_sum, order->price, order->num, (order->num == 1) ? "order" : "orders");
                    }
                }

            }else{
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: 0; Sell levels: 0\n", order_lists[i].product);
            }     
        }
        printf(LOG_PREFIX"\t--POSITIONS--\n");
        for (int i = 0; i < num_traders; i++) {
            printf(LOG_PREFIX"\tTrader %d: ",i);
            for(int j = 0; j < num_products; j++){
                printf("%s %lld ($%lld)", trader_processes[i].trader_product[j].product, trader_processes[i].trader_product[j].positions, trader_processes[i].trader_product[j].value);
                if(j == num_products-1){
                    printf("\n");
                }else{
                    printf(", ");
                }
            }
        }
    }else if(cancel_format){
        printf(LOG_PREFIX"\t--ORDERBOOK--\n");
        for (int i = 0; i < num_products; i++) {
            if (order_lists[i].real_order_count != 0){
                int buy_count = 0, sell_count = 0;
                // Count the number of buy and sell orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0 && order_lists[i].real_order_count != 0){
                        Order *order = &order_lists[i].orders[j];
                        if (strcmp(order->order_type, "BUY") == 0) {
                            buy_count++;
                        } else if (strcmp(order->order_type, "SELL") == 0) {
                            sell_count++;
                        }
                    }    
                }
                // Print product information
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: %d; Sell levels: %d\n", order_lists[i].product, buy_count, sell_count);
                // If there are any orders, print them
                // Sort orders by price in descending order
                for (int j = 0; j < order_lists[i].order_count - 1; j++) {
                    for (int k = j + 1; k < order_lists[i].order_count; k++) {
                        if (order_lists[i].orders[j].price < order_lists[i].orders[k].price) {
                            Order temp = order_lists[i].orders[j];
                            order_lists[i].orders[j] = order_lists[i].orders[k];
                            order_lists[i].orders[k] = temp;
                        }
                    }
                }
                // Print orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0){
                        Order *order = &order_lists[i].orders[j];
                        int qty_sum = 0;
                        for (int k = 0; k < order->num; k++) {
                            if(order->details[k].qty != 0){
                                qty_sum += order->details[k].qty;
                            }
                        }
                        printf(LOG_PREFIX"\t\t%s %d @ $%d (%d %s)\n", order->order_type, qty_sum, order->price, order->num, (order->num == 1) ? "order" : "orders");
                    }
                }

            }else{
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: 0; Sell levels: 0\n", order_lists[i].product);
            }     
        }
        printf(LOG_PREFIX"\t--POSITIONS--\n");
        for (int i = 0; i < num_traders; i++) {
            printf(LOG_PREFIX"\tTrader %d: ",i);
            for(int j = 0; j < num_products; j++){
                printf("%s %lld ($%lld)", trader_processes[i].trader_product[j].product, trader_processes[i].trader_product[j].positions, trader_processes[i].trader_product[j].value);
                if(j == num_products-1){
                    printf("\n");
                }else{
                    printf(", ");
                }
            }
        }
        snprintf(feedback, BUFFER_SIZE, "CANCELLED %d;", num_orders);
        write(pe_exchange[trader_id], feedback, strlen(feedback));

        if (!disconnected_traders){
            broadcast_market_order(order_type, product, 0, 0, trader_id);
        }
    }else if(amend_format){
        printf(LOG_PREFIX"\t--ORDERBOOK--\n");
        for (int i = 0; i < num_products; i++) {
            if (order_lists[i].real_order_count != 0){
                int buy_count = 0, sell_count = 0;
                // Count the number of buy and sell orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0 && order_lists[i].real_order_count != 0){
                        Order *order = &order_lists[i].orders[j];
                        if (strcmp(order->order_type, "BUY") == 0) {
                            buy_count++;
                        } else if (strcmp(order->order_type, "SELL") == 0) {
                            sell_count++;
                        }
                    }    
                }
                // Print product information
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: %d; Sell levels: %d\n", order_lists[i].product, buy_count, sell_count);
                // If there are any orders, print them
                // Sort orders by price in descending order
                for (int j = 0; j < order_lists[i].order_count - 1; j++) {
                    for (int k = j + 1; k < order_lists[i].order_count; k++) {
                        if (order_lists[i].orders[j].price < order_lists[i].orders[k].price) {
                            Order temp = order_lists[i].orders[j];
                            order_lists[i].orders[j] = order_lists[i].orders[k];
                            order_lists[i].orders[k] = temp;
                        }
                    }
                }
                // Print orders
                for (int j = 0; j < order_lists[i].order_count; j++) {
                    if (order_lists[i].orders[j].real_num != 0){
                        Order *order = &order_lists[i].orders[j];
                        int qty_sum = 0;
                        for (int k = 0; k < order->num; k++) {
                            if(order->details[k].qty != 0){
                                qty_sum += order->details[k].qty;
                            }
                        }
                        printf(LOG_PREFIX"\t\t%s %d @ $%d (%d %s)\n", order->order_type, qty_sum, order->price, order->num, (order->num == 1) ? "order" : "orders");
                    }
                }

            }else{
                printf(LOG_PREFIX"\tProduct: %s; Buy levels: 0; Sell levels: 0\n", order_lists[i].product);
            }     
        }
        printf(LOG_PREFIX"\t--POSITIONS--\n");
        for (int i = 0; i < num_traders; i++) {
            printf(LOG_PREFIX"\tTrader %d: ",i);
            for(int j = 0; j < num_products; j++){
                printf("%s %lld ($%lld)", trader_processes[i].trader_product[j].product, trader_processes[i].trader_product[j].positions, trader_processes[i].trader_product[j].value);
                if(j == num_products-1){
                    printf("\n");
                }else{
                    printf(", ");
                }
            }
        }
        snprintf(feedback, BUFFER_SIZE, "AMENDED %d;", num_orders);
        write(pe_exchange[trader_id], feedback, strlen(feedback));
        if (!disconnected_traders){
            broadcast_market_order(order_type, product, qty, price, trader_id);
        }
    }else {
        snprintf(feedback, BUFFER_SIZE, "INVALID;");
        write(pe_exchange[trader_id], feedback, strlen(feedback));
    }
}


void sigusr1_handler(int signal, siginfo_t *info, void *ucontext) {
    // Find the trader who sent the signal
    pid_t sender_pid = info->si_pid;
    int trader_id = -1;
    for (int i = 0; i < num_traders; i++) {
        if (trader_processes[i].pid == sender_pid) {
            trader_id = i;
            break;
        }
    }
    char command[BUFFER_SIZE];
    read(pe_trader[trader_id], command, BUFFER_SIZE);

    // Process the command
    process_command(command, trader_id);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Not enough arguments\n");
        return 1;
    }
    printf(LOG_PREFIX" Starting\n");

    // TODO: Read product information from file
    FILE *products_file = fopen(argv[1], "r");
    if (!products_file) {
        perror("Error opening products file");
        return 1;
    }

    fscanf(products_file, "%d", &num_products);
    products = malloc(num_products * sizeof(char[MAX_PRODUCT_LENGTH+1]));
    printf(LOG_PREFIX" Trading %d products:", num_products);
    for (int i = 0; i < num_products; i++) {
        fscanf(products_file, "%s", products[i]);
        printf(" %s", products[i]);
    }
    printf("\n");
    fclose(products_file);
    // Print product information
    
    

    // Initialize FIFOs
    num_traders = argc - 2;

    pe_exchange = (int *) malloc(num_traders * sizeof(int));
    pe_trader = (int *) malloc(num_traders * sizeof(int));


    char *exchange_fifo_name = (char *) malloc(BUFFER_SIZE * sizeof(char));
    char *trader_fifo_name = (char *) malloc(BUFFER_SIZE * sizeof(char));

    trader_processes = (TraderProcess *) malloc(num_traders * sizeof(TraderProcess));
    
    order_lists = malloc(num_products * sizeof(OrderList));

    for (int i = 0; i < num_products; i++) {
        strcpy(order_lists[i].product, products[i]);
        order_lists[i].orders = NULL;
        order_lists[i].order_count = 0;
        order_lists[i].real_order_count = 0;
    }

    for (int i = 0; i < num_traders; i++) {
        snprintf(exchange_fifo_name, BUFFER_SIZE, FIFO_EXCHANGE, i);
        snprintf(trader_fifo_name, BUFFER_SIZE, FIFO_TRADER, i);
        printf(LOG_PREFIX" Created FIFO /tmp/pe_exchange_%d\n",i);
        printf(LOG_PREFIX" Created FIFO /tmp/pe_trader_%d\n",i);

        // Create and open FIFOs
        mkfifo(exchange_fifo_name, 0777);
        mkfifo(trader_fifo_name, 0777);

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            char trader_id_arg[BUFFER_SIZE];
            snprintf(trader_id_arg, BUFFER_SIZE, "%d", i);

            execl(argv[i + 2], argv[i + 2], trader_id_arg, NULL);
            perror("Error launching trader process");
            exit(1);
        } else if (pid < 0) {
            // Fork error
            perror("Error in fork");
            exit(1);
        }
        printf(LOG_PREFIX" Starting trader %d (%s)\n", i, argv[i + 2]);
        pe_exchange[i] = open(exchange_fifo_name, O_WRONLY); // O_NONBLOCK
        pe_trader[i] = open(trader_fifo_name, O_RDONLY );
        trader_processes[i].pid = pid;
        trader_processes[i].fee = 0;
        trader_processes[i].order_id = 0;
        trader_processes[i].trader_product = malloc(num_products * sizeof(TraderProduct));
        for (int j = 0; j < num_products; j++) {
            strcpy(trader_processes[i].trader_product[j].product, products[j]);
            trader_processes[i].trader_product[j].value = 0;
            trader_processes[i].trader_product[j].positions = 0;
        }
        printf(LOG_PREFIX" Connected to /tmp/pe_exchange_%d\n", i);
        printf(LOG_PREFIX" Connected to /tmp/pe_trader_%d\n", i);
        // Send MARKET OPEN message to traders

        write(pe_exchange[i], "MARKET OPEN;", strlen("MARKET OPEN;"));
        kill(pid, SIGUSR1);     
    }

    struct sigaction sa_chld, sa_usr1;

    // Set up SIGCHLD handler
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("Error setting up SIGCHLD handler");
        exit(1);
    }

    sa_usr1.sa_sigaction = sigusr1_handler; // Note the different field name
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_SIGINFO; // Note the SA_SIGINFO flag
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("Error setting up SIGUSR1 handler");
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);

    while (!exit_loop) {
        pause();
    }

    for (int i = 0; i < num_traders; i++) {
        close(pe_exchange[i]);
        close(pe_trader[i]);
        snprintf(exchange_fifo_name, BUFFER_SIZE, FIFO_EXCHANGE, i);
        snprintf(trader_fifo_name, BUFFER_SIZE, FIFO_TRADER, i);
        unlink(trader_fifo_name);
        unlink(exchange_fifo_name);
        free(trader_processes[i].trader_product);
    }
    for (int i = 0; i < num_products; i++) {
        for (int j = 0; j < order_lists[i].order_count; j++) {
                free(order_lists[i].orders[j].details);
            }
        free(order_lists[i].orders);
    }
    free(order_lists);

    free(exchange_fifo_name);
    free(trader_fifo_name);

    free(products);
    free(pe_exchange);
    free(pe_trader);

    return 0;
}