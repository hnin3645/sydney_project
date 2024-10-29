#include "pe_trader.h"

volatile sig_atomic_t received_signal = 0;

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        received_signal = 1;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    int trader_id = atoi(argv[1]);
    char *exchange_fifo_name = (char *) malloc(BUFFER_SIZE * sizeof(char));
    char *trader_fifo_name = (char *) malloc(BUFFER_SIZE * sizeof(char));

    snprintf(exchange_fifo_name, BUFFER_SIZE, FIFO_EXCHANGE, trader_id);
    snprintf(trader_fifo_name, BUFFER_SIZE, FIFO_TRADER, trader_id);
	
    int pe_exchange = open(exchange_fifo_name, O_RDONLY);
    int pe_trader = open(trader_fifo_name, O_WRONLY);

    if (pe_exchange < 0 || pe_trader < 0) {
        perror("Error opening FIFOs");
        return 1;
    }

    char *buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGUSR1, &sa, NULL);
    

    while (1) {
        pause();
        if (received_signal) {
            received_signal = 0;

            int n = read(pe_exchange, buffer, BUFFER_SIZE - 1);

            if (n > 0) {
                buffer[n] = '\0';

                char product[MAX_PRODUCT_LENGTH];
                int qty, price;

                if (sscanf(buffer, "MARKET SELL %s %d %d;", product, &qty, &price) == 3) {
                    if (qty >= MAX_QTY) {
                        break;
                    }

                    snprintf(buffer, BUFFER_SIZE, "BUY %d %s %d %d;", trader_order_id, product, qty, price);
                    trader_order_id ++;
                    write(pe_trader, buffer, strlen(buffer));
                    kill(getppid(), SIGUSR1);
                }
            }
        }
    }

    close(pe_exchange);
    close(pe_trader);

    // Free dynamically allocated memory
    free(exchange_fifo_name);
    free(trader_fifo_name);
    free(buffer);

    return 0;
}