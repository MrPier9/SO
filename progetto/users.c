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
} transaction_to_send, transaction_rejected;

int mb_index_id;
struct msg_buf_mb{
    long msg_type;
    int index;
} mb_index;

int msg_budget_id;
struct msg_buf_budget{
    long msg_type;
    int pid;
} budget_buf;

int *list_nodes, *list_user, i, l;
struct timespec start, stop;

struct transaction_buf{
    transaction list[100];
    int list_index;
} buffer_pre_book;

int tot_trans = 0;

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

double budget_ev();

int main(int argc, char *argv[]){
    struct timespec wait_next_trans;
    int n;

    if (argc != 3){
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }
    sa.sa_handler = &handle_sig;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    buffer_pre_book.list_index = 0;

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
        n = transaction_data();
        try = try + n;
        /*if(try >= 1) printf("%d try = %d\n",getpid(), try);*/
        tot_trans++;
        wait_next_trans.tv_nsec = set_wait(so_max_trans_gen_nsec, so_min_trans_gen_nsec);
        nanosleep(&wait_next_trans, NULL);
    }

    /*budget_buf.msg_type = 2;
    budget_buf.pid = getpid();
    msgsnd(mb_index_id, &budget_buf, sizeof(budget_buf), 0);*/
    kill(getppid(), SIGUSR2);

    /*for(i = 0; i < buffer_pre_book.list_index; i++) {
        printf("--------------------------------------------\n");
        printf("    timestamp: %f",  (double)buffer_pre_book.list[i].timestamp);
        printf("    transaction sent: %.2f\n", buffer_pre_book.list[i].amount + buffer_pre_book.list[i].reward);
        printf("    sent to: %d\n", buffer_pre_book.list[i].receiver);
        printf("    sent by: %d", buffer_pre_book.list[i].sender);
        printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                         buffer_pre_book.list[i].amount, buffer_pre_book.list[i].reward);
               printf("--------------------------------------------\n");
    }*/

    printf("\ntot transaction made by %d - %d - budget%.2f\n", getpid(), tot_trans + 1, budget);
    close(trans_fd);
    sem_close(nodes_sem);
    sem_close(user_sem);
    shmdt(puser_shm);
    shmdt(pnodes_shm);
    shmdt(pmaster_book);
    exit(0);

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
    int n, node_pid;
    /*printf("pre evaluation\n");*/
    budget = budget_ev();
    /*printf("%.2f\n", budget);
    printf("prewait\n");*/
    /*sem_wait(user_sem);*/
    /*printf("postwait\n");*/
    transaction_tot = trans_val(budget);
    my_transaction.reward = transaction_tot * reward;
    my_transaction.amount = transaction_tot - my_transaction.reward;

    /*printf("budget %.2f\n", budget);
    budget_buf.msg_type = 1;
    budget_buf.pid = getpid();
    budget_buf.budget = budget;
    msgsnd(msg_budget_id, &budget_buf, sizeof(budget_buf), 0);
    kill(getppid(), SIGUSR2);*/
    if(budget < 2) return 1;
    else try = 0;
    /*printf("budget > 2\n");*/
    do {
        n = random_receiver();
    }while (n == getpid());
    my_transaction.receiver = list_user[n];

    clock_gettime(CLOCK_REALTIME, &stop);
    TEST_ERROR;
    my_transaction.timestamp = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
    my_transaction.sender = getpid();
    /*printf("--------------------------------------------\n");
    printf("    timestamp: %f",  (double)my_transaction.timestamp);
    printf("    transaction sent: %.2f\n", my_transaction.amount + my_transaction.reward);
    printf("    sent to: %d\n", my_transaction.receiver);
    printf("    sent by: %d", my_transaction.sender);
    printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                         my_transaction.amount, my_transaction.reward);
    printf("--------------------------------------------\n");
    sem_post(user_sem);*/

    node_pid = list_nodes[rand() % so_nodes_num];

    transaction_to_send.msg_type = node_pid;
    transaction_to_send.message = my_transaction;
    msgsnd(msg_id, &transaction_to_send, sizeof(transaction_to_send), 0);
    TEST_ERROR;
    /*printf("transaction sent\n");*/
    buffer_pre_book.list[buffer_pre_book.list_index] = my_transaction;
    buffer_pre_book.list_index++;
    /*printf("transaction added to buf\n");*/
    return 0;
}

double budget_ev(){
    int j, k, l;
    transaction temp;
    double budget_temp = so_budget_init;
    /*printf("pre nodes sem\n");*/
    sem_wait(nodes_sem);

    msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
    msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
    /*printf("user index %d\n", mb_index.index);*/
    for(i = 0; i < mb_index.index; i++){
        for(j = 0; j < SO_BLOCK_SIZE; j++) {
            temp = pmaster_book[i][j];
            /*printf("temp read\n");*/
            if(temp.receiver == getpid()) budget_temp = budget_temp + temp.amount;
            if(temp.sender == getpid()) budget_temp = budget_temp - (temp.amount + temp.reward);
            for(k = 0; k < buffer_pre_book.list_index; k++){
                if(temp.sender == getpid() && temp.amount == buffer_pre_book.list[k].amount && temp.receiver == buffer_pre_book.list[k].receiver){
                    /*printf("--------------------transaction eliminated from buf------------------------\n");
                    printf("    timestamp: %f",  (double)buffer_pre_book.list[k].timestamp);
                    printf("    transaction sent: %.2f\n", buffer_pre_book.list[k].amount + buffer_pre_book.list[k].reward);
                    printf("    sent to: %d\n", buffer_pre_book.list[k].receiver);
                    printf("    sent by: %d", buffer_pre_book.list[k].sender);
                    printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                         buffer_pre_book.list[k].amount, buffer_pre_book.list[k].reward);
                    printf("--------------------------------------------\n");*/
                    for(l = k ; l < buffer_pre_book.list_index - 1; l++){
                        buffer_pre_book.list[k] = buffer_pre_book.list[k + 1];
                    }
                    buffer_pre_book.list_index--;
                    if(buffer_pre_book.list_index < 0) buffer_pre_book.list_index = 0;
                }
            }
        }
    }
    /*printf("len buffer %d\n", buffer_pre_book.list_index);*/
    for(k = 0; k < buffer_pre_book.list_index; k++){
        budget_temp = budget_temp - (buffer_pre_book.list[k].amount + buffer_pre_book.list[k].reward);
        /*printf("calcolating budget after buffer\n");*/
    }
    /*printf("budget %.2f\n", budget_temp);*/
    sem_post(nodes_sem);
    /*printf("after sem post\n");*/
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

        case SIGUSR1:
            msgrcv(msg_id, &transaction_rejected, sizeof(transaction_rejected), 1, 0);
            for(i = 0; i < buffer_pre_book.list_index; i++){
                if(transaction_rejected.message.timestamp == buffer_pre_book.list[i].timestamp &&
                    transaction_rejected.message.sender == buffer_pre_book.list[i].sender){
                    for(l = i ; l < buffer_pre_book.list_index - 1; l++){
                        buffer_pre_book.list[i] = buffer_pre_book.list[i + 1];
                    }
                    buffer_pre_book.list_index--;
                }
            }
            break;
        default:
            break;
    }
}
