#define _GNU_SOURCE_
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <limits.h>
#include "my_lab.h"

transaction my_transaction;
int trans_fd;
time_t duration;
struct timespec wait_next_trans;
struct sigaction sa;

void handle_sig(int);
void read_trans();

int main(int argc, char* argv[]){
    double n;
    
    sa.sa_handler = &handle_sig;
    sigaction(SIGCONT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    
    wait_next_trans.tv_sec = 0;
    
    load_file();
    nodes_sem = sem_open(SNAME_N, O_RDWR);
    
    while(1){
        pause();
    }
    /*duration = time(NULL);
    while(time(NULL) < duration + 5){
        sleep(10);
        sem_wait(nodes_sem);
        trans_fd = open(TRANSACTION_FIFO, O_RDONLY);
        TEST_ERROR;
        read(trans_fd, &my_transaction, sizeof(my_transaction));
        TEST_ERROR;
        close(trans_fd);
        printf("    timestamp: %f", (double)my_transaction.timestamp);
            printf("    transaction sent: %.2f\n", my_transaction.amount + my_transaction.reward);
            printf("    sent to: %d\n", my_transaction.receiver);
            printf("    sent by: %d", my_transaction.sender);
            printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                        my_transaction.amount, my_transaction.reward);
            printf("    sent to node: %d\n", \
                        getpid());
            printf("--------------------------------------------\n");
        sem_post(nodes_sem);
        wait_next_trans.tv_nsec = set_wait();
        nanosleep(&wait_next_trans, NULL);
    }*/

    close(trans_fd);
    kill(getppid(), SIGUSR1);
    sem_close(nodes_sem);
    return 0;
}

void read_trans(){
    sem_wait(nodes_sem);
    TEST_ERROR;
    trans_fd = open(TRANSACTION_FIFO, O_RDONLY);
    TEST_ERROR;
    read(trans_fd, &my_transaction, sizeof(my_transaction));
    TEST_ERROR;
    close(trans_fd);
    printf("    timestamp: %f", (double)my_transaction.timestamp);
    printf("    transaction sent: %.2f\n", my_transaction.amount + my_transaction.reward);
    printf("    sent to: %d\n", my_transaction.receiver);
    printf("    sent by: %d", my_transaction.sender);
    printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                my_transaction.amount, my_transaction.reward);
    printf("    sent to node: %d\n", getpid());
    printf("--------------------------------------------\n");
    sem_post(nodes_sem);
    wait_next_trans.tv_nsec = set_wait();
    nanosleep(&wait_next_trans, NULL);
}

void handle_sig(int signal){
    switch (signal)
    {
    case SIGCONT:
        read_trans();
        break;
    case SIGINT:
        close(trans_fd);
        kill(getppid(), SIGUSR1);
        sem_close(nodes_sem);
        exit(0);
    break;
    default:
        break;
    }
    
}