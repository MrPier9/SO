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

int transaction_tot; /*cifra totale transazione*/
float reward;        /*reward = x% del netto*/
int budget;          /*netto transazione*/
transaction my_transaction;
int trans_fd;
struct sigaction sa;
int msg_id;
struct msg_buf{
    long msg_type;
    transaction message;
} transaction_to_send;

int mb_index_id;
struct msg_buf_mb{
    long msg_type;
    int index;
} mb_index;

int *list_nodes, *list_user, i;
struct timespec start, stop;

/*
 *It randomly calculate the value of the transaction
 */
int trans_val(int);
/*
 *It sets the transaction value, its net and reward
 */
void transaction_data();
/*
 *It randomly calculates the receiver of the transaction
 */
int random_receiver();
/*
 * It handles the signal received from other processes
 */
void handle_sig(int);

int main(int argc, char *argv[])
{
    struct timespec wait_next_trans;

    if (argc != 3){
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = &handle_sig;
    sigaction(SIGINT, &sa, NULL);

    sleep(1);

    load_file();
    /*printf("file loaded\n");*/
    TEST_ERROR;
    user_sem = sem_open(SNAME, O_RDWR); /*open semaphore created in master*/
    TEST_ERROR;
    /*printf("user sem set\n");*/
    start.tv_sec = atof(argv[1]);
    start.tv_nsec = atof(argv[2]);

    /*reward = atoi(argv[2]) * 0.01;*/
    users_shm_id = shmget(SHM_USERS_KEY, sizeof(int) * so_users_num, 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;

    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(int) * so_nodes_num, 0666);
    TEST_ERROR;
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR;
    /*printf("shm set\n");*/

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR;
    mb_index_id = msgget(MSG_INDEX_KEY, 0666);
    TEST_ERROR

    /*printf("msg set\n");*/
    list_nodes = malloc(sizeof(int) * so_nodes_num);
    list_user = malloc(sizeof(int) * so_users_num);
    /*printf("pre semwait\n");*/
    for(i = 0; i < so_users_num; i++){
        list_user[i] = puser_shm[i];
    }
    for(i = 0; i < so_nodes_num; i++){
        list_nodes[i] = pnodes_shm[i];
    }
    shmdt(puser_shm);
    shmdt(pnodes_shm);
    /*printf("pid copied\n");*/

    wait_next_trans.tv_sec = 0;
    budget = so_budget_init;
    reward = so_reward * 0.01;

    while (budget >= 2){
        /*printf("making transaction\n");*/
        transaction_data();
        wait_next_trans.tv_nsec = set_wait();
        nanosleep(&wait_next_trans, NULL);
    }

    /*if (budget < 2)
    {
        printf("\nuser %d budget -> %d, bout to send\n", getpid(), budget);
    }*/
    sem_close(user_sem);

    return 0;
}

int trans_val(int random_amount)
{
    int n;
    srand(time(NULL) * getpid());
    n = rand() % random_amount;
    if (n == 0)
    {
        n += 1;
    }
    return n;
}

int random_receiver()
{
    int n;
    n = rand() % so_users_num;
    return n;
}

void transaction_data()
{
    int n;

    /*clock_gettime(CLOCK_REALTIME, &start);*/

    /*printf("pid: %d - budget init= %d\n", getpid(), budget);*/
    transaction_tot = trans_val(budget);
    my_transaction.reward = transaction_tot * reward;
    my_transaction.amount = transaction_tot - my_transaction.reward;
    budget -= transaction_tot;
    do {
        n = random_receiver();
    }while (n == getpid());
    my_transaction.receiver = list_user[n];

    clock_gettime(CLOCK_REALTIME, &stop);
    TEST_ERROR;
    my_transaction.timestamp = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
    my_transaction.sender = getpid();

    /*trans_fd = open(TRANSACTION_FIFO, O_WRONLY);
    TEST_ERROR;
    write(trans_fd, &my_transaction, sizeof(my_transaction));
    TEST_ERROR;
    close(trans_fd);
    sem_wait(user_sem);

    printf("    timestamp: %f", (double)my_transaction.timestamp);
    printf("    transaction sent: %d\n", transaction_tot);
    printf("    sent to: %d\n", my_transaction.receiver);
    printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                my_transaction.amount, my_transaction.reward);
    printf("    sent to node: %d\n  new budget: %d\n", \
                pnodes_shm[rand() % so_nodes_num], budget);
    printf("--------------------------------------------\n");
*/

    transaction_to_send.msg_type = list_nodes[rand() % so_nodes_num];
    transaction_to_send.message = my_transaction;
    msgsnd(msg_id, &transaction_to_send, sizeof(transaction_to_send), 0);
    TEST_ERROR;
}

void handle_sig(int signal)
{
    sem_close(user_sem);
    shmdt(puser_shm);
    exit(0);
}
