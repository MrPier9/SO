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

int msg_budget_id;
struct msg_buf_budget{
    long msg_type;
    int pid;
    double budget;
} budget_buf;

int *list_nodes, *list_user, i;
double  my_reward = 0;
double total_reward = 0;
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

int main(int argc, char *argv[]){
    struct timespec wait_writing_mb;

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
    msg_budget_id = msgget(MSG_BUDGET_KEY, 0666);
    TEST_ERROR

    list_user = malloc(sizeof(int)*so_users_num);
    list_nodes = malloc(sizeof(int) * so_nodes_num);

    wait_writing_mb.tv_sec = 0;

    for(i = 0; i < so_users_num; i++){
        list_user[i] = puser_shm[i];
    }
    for(i = 0; i < so_nodes_num; i++){
        list_nodes[i] = pnodes_shm[i];
    }
    shmdt(puser_shm);
    shmdt(pnodes_shm);

    while (trans_counter < so_tp_size){
        /*printf("trans counter %d: %d\n", getpid(), trans_counter);*/
        if(tp_len < SO_BLOCK_SIZE-1){
            read_trans();
            trans_counter++;
        }else if(tp_len == SO_BLOCK_SIZE-1){
            clock_gettime(CLOCK_REALTIME, &stop);
            tp_block[SO_BLOCK_SIZE-1].amount = my_reward;
            tp_block[SO_BLOCK_SIZE-1].receiver = getpid();
            tp_block[SO_BLOCK_SIZE-1].sender = -1;
            tp_block[SO_BLOCK_SIZE-1].reward = 0;
            tp_block[SO_BLOCK_SIZE-1].timestamp = (stop.tv_sec - start.tv_sec) +
                    (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;

            wait_writing_mb.tv_nsec = set_wait(so_max_trans_proc_nsec,so_min_trans_proc_nsec);
            nanosleep(&wait_writing_mb, NULL);
            sem_wait(nodes_sem);
            msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
            for(i = 0; i < SO_BLOCK_SIZE; i++) {
                pmaster_book[mb_index.index][i] = tp_block[i];

                tp_block[i].reward = 0;
                tp_block[i].amount = 0;
                tp_block[i].sender = 0;
                tp_block[i].timestamp = 0;
                tp_block[i].receiver = 0;
            }
            mb_index.index++;
            msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
            /*budget_buf.msg_type = 2;
            budget_buf.pid = getpid();
            budget_buf.budget = total_reward;

            msgsnd(msg_budget_id, &budget_buf, sizeof(budget_buf), 0);
            kill(getppid(), SIGUSR1);*/
            sem_post(nodes_sem);
            tp_len = 0;

            my_reward = 0;
        }
    }

    while(1){
        msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);
        transaction_rec.msg_type = 1;
        msgsnd(msg_id, &transaction_rec, sizeof(transaction_rec), 0);
        kill(transaction_rec.message.sender, SIGUSR1);
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

void read_trans(){
    msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);
    TEST_ERROR
    my_transaction = transaction_rec.message;
    my_reward = my_reward + my_transaction.reward;
    total_reward = total_reward + my_transaction.reward;
    tp_block[tp_len] = my_transaction;
    wait_next_trans.tv_nsec = set_wait(so_max_trans_gen_nsec,so_min_trans_gen_nsec);
    nanosleep(&wait_next_trans, NULL);
    tp_len++;
}

void handle_sig(int signal)
{
    switch (signal){
    case SIGCONT:
        /*read_trans();*/
        break;
    case SIGINT:
        sem_wait(nodes_sem);

        printf("\n\nnode %d\n", getpid());
        printf("transactions elaborated %d - transaction still in transaction pool %d\n", trans_counter, tp_len);
        printf("--------------------------------------------\n");
        for(i = 0; i < tp_len; i++){/* could be til tp_len, to try | SO_BLOCK_SIZE */
            printf("    timestamp: %f",  (double)tp_block[i].timestamp);
            printf("    transaction sent: %.2f\n", tp_block[i].amount + tp_block[i].reward);
            printf("    sent to: %d\n", tp_block[i].receiver);
            printf("    sent by: %d", tp_block[i].sender);
            printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                        tp_block[i].amount, tp_block[i].reward);
            printf("--------------------------------------------\n");
        }
        sem_post(nodes_sem);

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