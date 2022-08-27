#ifndef _MY_LAB_H
#define _MY_LAB_H

#define TEST_ERROR                         \
    if (errno)                             \
    {                                      \
        fprintf(stderr,                    \
                "%s:%d - error %d - %s\n", \
                __FILE__, __LINE__, errno, \
                strerror(errno));          \
    }

#define SNAME "user_sem"
#define SNAME_N "nodes_sem"
#define SHM_NODES_KEY 7295
#define SHM_USERS_KEY 7437
#define MASTER_BOOK_KEY 7552
#define MSG_QUEUE_KEY 7354
#define MSG_INDEX_KEY 9722
#define MSG_BUDGET_KEY 9229
#define USER_PATH "./users"
#define NODES_PATH "./nodes"
#define TRANSACTION_FIFO "trans_fifo"
#define BILLION 1000000000L
#define SO_REGISTRY_SIZE 100
#define SO_BLOCK_SIZE 10

typedef struct{
    double timestamp;
    pid_t sender;
    pid_t receiver;
    float amount;
    float reward;
} transaction;

transaction *transaction_value;

int *user_sem_id;
unsigned int sem_value;
sem_t *user_sem;
int *nodes_sem_id;
sem_t *nodes_sem;

FILE *my_f;
int users_shm_id;
int (*puser_shm)[3];/*0 pid - 1 budget - 2 state*/
int nodes_shm_id;
int (*pnodes_shm)[3];/*0 pid - 1 budget - index master_book*/
int master_book_id;
transaction (*pmaster_book)[SO_REGISTRY_SIZE]/*[SO_BLOCK_SIZE]*/;
transaction master_book_page[SO_BLOCK_SIZE];

int so_users_num;
int so_nodes_num;
int so_budget_init;
int so_reward;
unsigned long so_min_trans_gen_nsec;
unsigned long so_max_trans_gen_nsec;
int so_retry;
int so_tp_size;
unsigned long so_min_trans_proc_nsec;
unsigned long so_max_trans_proc_nsec;
int so_sim_sec;
int so_num_friends;
int so_hops;

/*
 * It builds the semaphore for the user processes.
 */
void user_sem_set(int);

/*
 *It builds the semaphore for the nodes processes
 */
void nodes_sem_set(int);

/*
 * It unlink and then close user processes semaphore
 */
void user_sem_del();

/*
 * It unlink and then close nodes processes semaphore
 */
void nodes_sem_del();
/*void safe_close(); chiama transaction_del e sem_del*/
void load_file();      /*carica file con dati iniziali*/
void test_load_file(); /*test file dati iniziali*/

/*
 * It sets the shared memory for the user processes
 */
void shm_user_set();

/*
 * It sets the shared memory for the nodes processes
 */
void shm_nodes_set();

/*
 * It sets the timer for the next transaction creation
 */
double set_wait(unsigned long, unsigned long);

#endif