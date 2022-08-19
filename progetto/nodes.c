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
#include <sys/msg.h>
#include <limits.h>
#include "my_lab.h"

transaction my_transaction;
int trans_fd;
time_t duration;
struct timespec wait_next_trans;
struct sigaction sa;
int msg_id;
struct msg_buf{
    long msg_type;
    transaction message;
} transaction_rec;
int *list_nodes, *list_user, i;

typedef struct{
    transaction *tp_len;
    int len;
} tp_block;

/*
 * It handles the signal received from other processes
 */
void handle_sig(int);
/*
 * It reads the transaction received and store it
 */
void read_trans();

int main(int argc, char *argv[])
{
    double n;
    tp_block my_tp;

    sa.sa_handler = &handle_sig;
    sigaction(SIGCONT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    wait_next_trans.tv_sec = 0;

    /*sleep(10);*/printf("nodes ready\n");
    load_file();
    my_tp.tp_len = (transaction *) malloc(sizeof(transaction) * so_tp_size);
    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR;
    users_shm_id = shmget(SHM_USERS_KEY, sizeof(int) * so_users_num, 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;
    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(int) * so_nodes_num, 0666);
    TEST_ERROR;
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR;
    user_sem = sem_open(SNAME, O_RDWR);
    TEST_ERROR;
    nodes_sem = sem_open(SNAME_N, O_RDWR);
    TEST_ERROR;
    users_shm_id = shmget(SHM_USERS_KEY, sizeof(int) * so_users_num, 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;

    for(i = 0; i < so_users_num; i++){
        list_user[i] = puser_shm[i];
    }
    for(i = 0; i < so_nodes_num; i++){
        list_nodes[i] = pnodes_shm[i];
    }
    shmdt(puser_shm);
    shmdt(pnodes_shm);
    printf("\n\npre while\n");
    while (1)
    {
        printf("\nnodes ready to rec\n");
        read_trans();
    }
/*
    duration = time(NULL);
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
    sem_close(user_sem);

    return 0;
}

void read_trans()
{
    msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);
    TEST_ERROR;
    my_transaction = transaction_rec.message;
    printf("    timestamp: %f", (double)my_transaction.timestamp);
    printf("    transaction sent: %.2f\n", my_transaction.amount + my_transaction.reward);
    printf("    sent to: %d\n", my_transaction.receiver);
    printf("    sent by: %d", my_transaction.sender);
    printf("    transaction amount: %.2f\n    reward: %.2f\n",
           my_transaction.amount, my_transaction.reward);
    printf("    sent to node: %d\n", getpid());
    printf("--------------------------------------------\n");
    sem_post(nodes_sem);
    wait_next_trans.tv_nsec = set_wait();
    nanosleep(&wait_next_trans, NULL);
}

void handle_sig(int signal)
{
    switch (signal)
    {
    case SIGCONT:
        /*read_trans();*/
        break;
    case SIGINT:
        close(trans_fd);
        kill(getppid(), SIGUSR1);
        sem_close(nodes_sem);
        sem_close(user_sem);
        shmdt(puser_shm);
        shmdt(pnodes_shm);
        exit(0);
        break;
    default:
        break;
    }
}