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
double budget;          /*netto transazione*/
transaction my_transaction;
int trans_fd;
int try = 0;
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

int msg_budget_id;
struct msg_buf_budget{
    long msg_type;
    int pid;
    int terminated;
    double budget;
} budget_buf;

int *list_nodes, *list_user, i;
struct timespec start, stop;

/*
 *It randomly calculate the value of the transaction
 */
int trans_val(int);
/*
 *It sets the transaction value, its net and reward
 */
int transaction_data();
/*
 *It randomly calculates the receiver of the transaction
 */
int random_receiver();
/*
 * It handles the signal received from other processes
 */
void handle_sig(int);

int budget_ev();

int main(int argc, char *argv[]){
    struct timespec wait_next_trans;


    if (argc != 3){
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = &handle_sig;
    sigaction(SIGINT, &sa, NULL);
    budget_buf.terminated = 0;

    sleep(1);

    load_file();
    /*printf("file loaded\n");*/
    TEST_ERROR;
    user_sem = sem_open(SNAME, O_RDWR); /*open semaphore created in master*/
    TEST_ERROR;
    nodes_sem = sem_open(SNAME_N, O_RDWR);
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

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_REGISTRY_SIZE, 0666);
    TEST_ERROR
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR;
    mb_index_id = msgget(MSG_INDEX_KEY, 0666);
    TEST_ERROR
    msg_budget_id = msgget(MSG_BUDGET_KEY, 0666);
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

    while (try < so_retry){
        /*printf("making transaction\n");*/
        try = try + transaction_data();
        wait_next_trans.tv_nsec = set_wait();
        nanosleep(&wait_next_trans, NULL);
    }

    budget_buf.msg_type = 1;
    budget_buf.terminated = 1;
    budget_buf.pid = getpid();
    budget_buf.budget = budget;
    msgsnd(msg_budget_id, &budget_buf, sizeof(budget_buf), 0);

    printf("\nuser %d terminating with retry %d and budget %.2f\n", getpid(), try, budget);
    close(trans_fd);
    sem_close(nodes_sem);
    sem_close(user_sem);
    shmdt(puser_shm);
    shmdt(pnodes_shm);
    shmdt(pmaster_book);
    exit(0);

    return 0;
}

int trans_val(int random_amount)
{
    int n;
    srand(time(NULL) * getpid());
    n = rand() % random_amount;
    if (n == 0){
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

int transaction_data(){
    int n, c = 0;
    c = budget_ev();
    /*printf("budget calcolated %d\n", c);*/
    /*printf("pid: %d - budget init= %d\n", getpid(), budget);*/
    transaction_tot = trans_val(budget);
    my_transaction.reward = transaction_tot * reward;
    my_transaction.amount = transaction_tot - my_transaction.reward;

    if(c == 0) budget -= transaction_tot;
    else if(c > 0) budget = so_budget_init - c;
    else budget = so_budget_init + c;
    budget_buf.msg_type = 1;
    budget_buf.pid = getpid();
    budget_buf.budget = budget;
    msgsnd(msg_budget_id, &budget_buf, sizeof(budget_buf), 0);
    kill(getppid(), SIGUSR2);
    if(budget < 2) return 1;
    else try = 0;
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
    return 0;
}

int budget_ev(){
    int j;
    transaction temp;
    int budget_temp = 0;

    sem_wait(nodes_sem);
    msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
    msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
    /*printf("\nindex %d\n", mb_index.index);*/
    for(i = 0; i < mb_index.index; i++){
        for(j = 0; j < SO_BLOCK_SIZE; j++) {
            temp = pmaster_book[i][j];
            if(temp.receiver == getpid()) budget_temp = budget_temp + temp.amount;
            if(temp.sender == getpid()) budget_temp = budget_temp - (temp.amount + temp.reward);
        }
    }
    sem_post(nodes_sem);
    return budget_temp;
}

void handle_sig(int signal){
    switch (signal) {
        case SIGINT:
            close(trans_fd);
            sem_close(nodes_sem);
            sem_close(user_sem);
            shmdt(puser_shm);
            shmdt(pnodes_shm);
            shmdt(pmaster_book);
            exit(0);

        default:
            break;
    }
}
