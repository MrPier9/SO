#define _GNU_SOURCE_
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "my_lab.h"

transaction my_transaction;
int trans_fd;
int tp_len;
int trans_counter = 0;
time_t duration;
struct timespec wait_next_trans;
struct sigaction sa;
int master_index;

int msg_id;
struct msg_buf
{
    long msg_type;
    transaction message;
} transaction_rec;

int mb_index_id;
struct msg_buf_mb
{
    long msg_type;
    int index;
} mb_index;

int msg_budget_id;
struct msg_buf_budget
{
    long msg_type;
    int pid;
    double budget;
} budget_buf;

int *list_nodes, *list_user, i;
double my_reward = 0;
double total_reward = 0;
transaction tp_block[SO_BLOCK_SIZE];

struct timespec start, stop;
struct timespec wait_writing_mb;

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

    if (argc != 3)
    {
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = &handle_sig;
    sigaction(SIGINT, &sa, NULL);

    wait_next_trans.tv_sec = 0;
    start.tv_sec = atof(argv[1]);
    start.tv_nsec = atof(argv[2]);

    load_file();
    tp_len = 0;

    users_shm_id = shmget(SHM_USERS_KEY, sizeof(puser_shm) * so_users_num, 0666);
    TEST_ERROR
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR

    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(pnodes_shm) * so_nodes_num, 0666);
    TEST_ERROR
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_BLOCK_SIZE, 0666);
    TEST_ERROR
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR

    user_sem = sem_open(SNAME, O_RDWR);
    TEST_ERROR
    nodes_sem = sem_open(SNAME_N, O_RDWR);
    TEST_ERROR

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR

    wait_writing_mb.tv_sec = 0;

    while (trans_counter < so_tp_size)
    {
        if (tp_len < SO_BLOCK_SIZE - 1)
        {
            read_trans();
            trans_counter++;
        }
        else if (tp_len == SO_BLOCK_SIZE - 1)
        {
            sem_wait(nodes_sem);

            clock_gettime(CLOCK_REALTIME, &stop);
            tp_block[SO_BLOCK_SIZE - 1].amount = my_reward;
            tp_block[SO_BLOCK_SIZE - 1].receiver = getpid();
            tp_block[SO_BLOCK_SIZE - 1].sender = -1;
            tp_block[SO_BLOCK_SIZE - 1].reward = 0;
            tp_block[SO_BLOCK_SIZE - 1].timestamp = (stop.tv_sec - start.tv_sec) +
                                                    (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;

            wait_writing_mb.tv_nsec = set_wait(so_max_trans_proc_nsec, so_min_trans_proc_nsec);
            nanosleep(&wait_writing_mb, NULL);

            master_index = pnodes_shm[0][2];
            for (i = 0; i < SO_BLOCK_SIZE; i++)
            {
                pmaster_book[master_index][i] = tp_block[i];
                tp_block[i].reward = 0;
                tp_block[i].amount = 0;
                tp_block[i].sender = 0;
                tp_block[i].timestamp = 0;
                tp_block[i].receiver = 0;
            }
            master_index++;

            for (i = 0; i < so_nodes_num; i++)
                pnodes_shm[i][2] = master_index;

            sem_post(nodes_sem);

            tp_len = 0;
            my_reward = 0;
        }
    }
    printf("nodes space finished\n");
    while (1)
    {
        msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);
        transaction_rec.msg_type = 1;
        msgsnd(msg_id, &transaction_rec, sizeof(transaction_rec), 0);
        kill(transaction_rec.message.sender, SIGUSR1);
    }
}

void read_trans()
{
    msgrcv(msg_id, &transaction_rec, sizeof(transaction_rec), getpid(), 0);

    my_transaction = transaction_rec.message;
    my_reward = my_reward + my_transaction.reward;
    total_reward = total_reward + my_transaction.reward;
    tp_block[tp_len] = my_transaction;
    tp_len++;
}

void handle_sig(int signal)
{
    switch (signal)
    {
    case SIGINT:
        printf("node %d - transactions elaborated %d - transaction still in transaction pool %d\n",
               getpid(), trans_counter, tp_len);
        sem_post(nodes_sem);
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