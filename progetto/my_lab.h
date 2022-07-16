#ifndef _MY_LAB_H
#define _MY_LAB_H

#define TEST_ERROR  if(errno) {fprintf(stderr, \
                    "%s:%d - error %d - %s\n", \
                    __FILE__, __LINE__, errno, \
                    strerror(errno));}

#define SNAME "user_sem"
#define SNAME_N "nodes_sem"
#define SHM_NODES_KEY 7295
#define SHM_USERS_KEY 7437
#define USER_PATH "./users" 
#define NODES_PATH "./nodes"
#define TRANSACTION_FIFO "trans_fifo"


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
int *puser_shm;
int nodes_shm_id;
int *pnodes_shm;



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


void user_sem_set(int);/*setta il semaforo con il valore che si necessita*/
void nodes_sem_set(int);
void user_sem_del();/*elimina il semaforo*/
void nodes_sem_del();
/*void safe_close(); chiama transaction_del e sem_del*/
void load_file();/*carica file con dati iniziali*/
void test_load_file();/*test file dati iniziali*/
void shm_user_set();
void shm_nodes_set();
double set_wait();


#endif