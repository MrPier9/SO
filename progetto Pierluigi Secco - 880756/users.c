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

int transaction_tot; /*cifra totale transazione*/
float reward;        /*reward = x% del netto*/
double budget;       /*netto transazione*/
transaction my_transaction;
int try = 0;
struct sigaction sa;
int master_index;

int msg_id;
struct msg_buf
{
    long msg_type;
    transaction message;
} transaction_to_send, transaction_rejected;

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
} budget_buf;

int *list_nodes, *list_user, i, l;
struct timespec start, stop;

struct transaction_buf
{
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

int main(int argc, char *argv[])
{
    struct timespec wait_next_trans;
    int n;

    if (argc != 3)
    {
        printf("Argument error\n");
        exit(EXIT_FAILURE);
    }
    sa.sa_handler = &handle_sig;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    buffer_pre_book.list_index = 0;

    load_file();
    TEST_ERROR;
    user_sem = sem_open(SNAME, O_RDWR); /*open semaphore created in master*/
    TEST_ERROR;
    nodes_sem = sem_open(SNAME_N, O_RDWR);
    TEST_ERROR;

    start.tv_sec = atof(argv[1]);
    start.tv_nsec = atof(argv[2]);

    users_shm_id = shmget(SHM_USERS_KEY, sizeof(puser_shm) * so_users_num, 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;

    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(pnodes_shm) * so_nodes_num, 0666);
    TEST_ERROR;
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR;

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_BLOCK_SIZE, 0666);
    /*  master_book_id = shmget(MASTER_BOOK_KEY, sizeof(pmaster_book) * SO_REGISTRY_SIZE, IPC_CREAT | 0666); */
    TEST_ERROR
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    TEST_ERROR;

    sem_wait(user_sem);
    sem_post(user_sem);

    wait_next_trans.tv_sec = 0;
    budget = so_budget_init;
    reward = so_reward * 0.01;

    while (try < so_retry)
    {
        n = transaction_data();

        /* if (n == 0)
            try = 0;
        else */
        try = try + n;

        tot_trans++;
        wait_next_trans.tv_nsec = set_wait(so_max_trans_gen_nsec, so_min_trans_gen_nsec);
        nanosleep(&wait_next_trans, NULL);
    }
    /* printf("%d - try: %d\n", getpid(), try); */

    kill(getppid(), SIGUSR2);
    TEST_ERROR

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
    if (n == 0)
    {
        n += 1;
    }
    return n;
}

int random_receiver()
{
    int n;
    clock_gettime(CLOCK_REALTIME, &stop);
    srand(time(NULL) * stop.tv_nsec);
    n = rand() % so_users_num;
    return n;
}

int transaction_data()
{
    int n, node_pid, counter;

    budget = budget_ev();

    if (budget < 2)
    {
        my_transaction.amount = 0;
        my_transaction.receiver = 0;
        my_transaction.reward = 0;
        my_transaction.sender = 0;
        my_transaction.timestamp = 0;
        return 1;
    }
    else
        try = 0;

    transaction_tot = trans_val(budget);
    my_transaction.reward = transaction_tot * reward;
    my_transaction.amount = transaction_tot - my_transaction.reward;

    counter = 0;
    do
    {
        n = random_receiver();
        counter++;
        if (counter == 5)
        {
            my_transaction.amount = 0;
            my_transaction.receiver = 0;
            my_transaction.reward = 0;
            my_transaction.sender = 0;
            my_transaction.timestamp = 0;
            return 1;
        }
    } while (puser_shm[n][0] == getpid() || puser_shm[n][2] == 1);

    my_transaction.receiver = puser_shm[n][0];
    clock_gettime(CLOCK_REALTIME, &stop);
    my_transaction.timestamp = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION;
    my_transaction.sender = getpid();
    node_pid = pnodes_shm[rand() % so_nodes_num][0];
    transaction_to_send.msg_type = node_pid;
    transaction_to_send.message = my_transaction;

    msgsnd(msg_id, &transaction_to_send, sizeof(transaction_to_send), 0);
    buffer_pre_book.list[buffer_pre_book.list_index] = my_transaction;
    buffer_pre_book.list_index++;

    return 0;
}

double budget_ev()
{
    int j, k, l;
    transaction temp;
    double budget_temp = so_budget_init;

    sem_wait(nodes_sem);
    if (errno == EINTR)
    {
    }

    master_index = pnodes_shm[0][2];
    for (i = 0; i < master_index; i++)
    {
        for (j = 0; j < SO_BLOCK_SIZE; j++)
        {

            temp = pmaster_book[i][j];
            if (temp.receiver == getpid())
                budget_temp = budget_temp + temp.amount;
            if (temp.sender == getpid())
                budget_temp = budget_temp - (temp.amount + temp.reward);

            for (k = 0; k < buffer_pre_book.list_index;)
            {
                if (temp.sender == getpid() && temp.amount == buffer_pre_book.list[k].amount && temp.receiver == buffer_pre_book.list[k].receiver)
                {
                    for (l = k; l < buffer_pre_book.list_index - 1; l++)
                    {
                        buffer_pre_book.list[l] = buffer_pre_book.list[l + 1];
                    }
                    buffer_pre_book.list_index--;
                    if (buffer_pre_book.list_index < 0)
                        buffer_pre_book.list_index = 0;
                }
                else
                {
                    k++;
                }
            }
        }
    }

    for (k = 0; k < buffer_pre_book.list_index; k++)
    {
        budget_temp = budget_temp - (buffer_pre_book.list[k].amount + buffer_pre_book.list[k].reward);
    }
    sem_post(nodes_sem);

    return budget_temp;
}

void handle_sig(int signal)
{
    switch (signal)
    {
    case SIGINT:
        sem_post(user_sem);
        sem_close(nodes_sem);
        sem_close(user_sem);
        shmdt(puser_shm);
        shmdt(pnodes_shm);
        shmdt(pmaster_book);
        exit(0);

    case SIGUSR1:
        msgrcv(msg_id, &transaction_rejected, sizeof(transaction_rejected), 1, 0);
        for (i = 0; i < buffer_pre_book.list_index; i++)
        {
            if (transaction_rejected.message.timestamp == buffer_pre_book.list[i].timestamp &&
                transaction_rejected.message.sender == buffer_pre_book.list[i].sender)
            {
                for (l = i; l < buffer_pre_book.list_index - 1; l++)
                {
                    buffer_pre_book.list[i] = buffer_pre_book.list[i + 1];
                }
                buffer_pre_book.list_index--;
            }
        }
        break;
    case SIGUSR2:
        printf("%d ready\n", getpid());
        try = try + transaction_data();
        printf("%d transaction made\n", getpid());
        break;
    default:
        break;
    }
}
