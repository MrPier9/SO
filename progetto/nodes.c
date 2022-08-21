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
int tp_len;
int trans_counter = 0;
time_t duration;
struct timespec wait_next_trans;
struct sigaction sa;

int msg_id;
struct msg_buf{
    long msg_type;
    transaction message;
} transaction_rec;

int mb_index_id;
struct msg_buf_mb{
    long msg_type;
    int index;
} mb_index;

int *list_nodes, *list_user, i;
double  my_reward = 0;
transaction tp_block[SO_BLOCK_SIZE];

struct timespec start, stop;

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
    int flag = 0;
    double n;


    if (argc != 3){
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = &handle_sig;
    sigaction(SIGCONT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    wait_next_trans.tv_sec = 0;
    start.tv_sec = atof(argv[1]);
    start.tv_nsec = atof(argv[2]);

    load_file();
    tp_len = 0;

    users_shm_id = shmget(SHM_USERS_KEY, sizeof(int) * so_users_num, 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;

    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(int) * so_nodes_num, 0666);
    TEST_ERROR;
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR;

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_REGISTRY_SIZE, 0666);
    TEST_ERROR
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR

    user_sem = sem_open(SNAME, O_RDWR);
    TEST_ERROR;
    nodes_sem = sem_open(SNAME_N, O_RDWR);
    TEST_ERROR;

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR;
    mb_index_id = msgget(MSG_INDEX_KEY, 0666);
    TEST_ERROR

    list_user = malloc(sizeof(int)*so_users_num);
    list_nodes = malloc(sizeof(int) * so_nodes_num);

    for(i = 0; i < so_users_num; i++){
        list_user[i] = puser_shm[i];
    }
    for(i = 0; i < so_nodes_num; i++){
        list_nodes[i] = pnodes_shm[i];
    }


    shmdt(puser_shm);
    shmdt(pnodes_shm);
    while (trans_counter < so_tp_size){
        if(tp_len < SO_BLOCK_SIZE-1){
            read_trans();
            /*my_tp.tp_len[my_tp.len] = my_transaction;*/
            tp_len++;
        }else if(tp_len == SO_BLOCK_SIZE-1){
            clock_gettime(CLOCK_REALTIME, &stop);
            tp_block[SO_BLOCK_SIZE-1].amount = my_reward;
            tp_block[SO_BLOCK_SIZE-1].receiver = getpid();
            tp_block[SO_BLOCK_SIZE-1].sender = -1;
            tp_block[SO_BLOCK_SIZE-1].reward = 0;
            tp_block[SO_BLOCK_SIZE-1].timestamp = (stop.tv_sec - start.tv_sec) +
                    (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
            sem_wait(nodes_sem);
            /*for(i = 0; i < so_tp_size; i++){
                printf("    timestamp: %f", (double)tp_block[i].timestamp);
                printf("    transaction sent: %.2f\n", tp_block[i].amount + tp_block[i].reward);
                printf("    sent to: %d\n", tp_block[i].receiver);
                printf("    sent by: %d", tp_block[i].sender);
                printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                        tp_block[i].amount, tp_block[i].reward);
                printf("    sent to node: %d\n", \
                        getpid());
                printf("--------------------------------------------\n");
            }*/
            msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
            for(i = 0; i < SO_BLOCK_SIZE; i++) {
                pmaster_book[mb_index.index][i] = tp_block[i];
            }
            mb_index.index++;
            msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
            sem_post(nodes_sem);
            tp_len = 0;
        }
    }

    close(trans_fd);
    sem_close(nodes_sem);
    sem_close(user_sem);
    shmdt(puser_shm);
    shmdt(pnodes_shm);
    shmdt(pmaster_book);
    exit(0);

    return 0;
}

void read_trans()
{
    msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);
    TEST_ERROR;
    my_transaction = transaction_rec.message;
    my_reward = my_reward + my_transaction.reward;
    /*sem_wait(nodes_sem);
    printf("nodes %d = %f reward\n", getpid(), my_reward);
    printf("    timestamp: %f", (double)my_transaction.timestamp);
    printf("    transaction sent: %.2f\n", my_transaction.amount + my_transaction.reward);
    printf("    sent to: %d\n", my_transaction.receiver);
    printf("    sent by: %d", my_transaction.sender);
    printf("    transaction amount: %.2f\n    reward: %.2f\n",
           my_transaction.amount, my_transaction.reward);
    printf("    sent to node: %d\n", getpid());
    printf("--------------------------------------------\n");*/
    tp_block[tp_len] = my_transaction;
    /*printf("trans in position %d", my_tp.len);
    printf("trans read %d\n", my_tp.len);
    printf("    timestamp: %f", (double)my_tp.tp_len[my_tp.len].timestamp);
    printf("    transaction sent: %.2f\n", my_tp.tp_len[my_tp.len].amount + my_tp.tp_len[my_tp.len].reward);
    printf("    sent to: %d\n", my_tp.tp_len[my_tp.len].receiver);
    printf("    sent by: %d", my_tp.tp_len[my_tp.len].sender);
    printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                        my_tp.tp_len[my_tp.len].amount, my_tp.tp_len[my_tp.len].reward);
    printf("    sent to node: %d\n", \
                        getpid());
    printf("--------------------------------------------\n");
    sem_post(nodes_sem);*/
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
        sem_close(nodes_sem);
        sem_close(user_sem);
        shmdt(puser_shm);
        shmdt(pnodes_shm);
        shmdt(pmaster_book);
        exit(0);
        break;
    default:
        break;
    }
}