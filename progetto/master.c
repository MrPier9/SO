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
#include <limits.h>
#include <sys/msg.h>
#include "my_lab.h"

pid_t pid;
struct sigaction sa;
struct sigaction sa_user;
int user_done = 0;
int master_index = 0;
/*int (*user_arr)[3];
int (*nodes_arr)[2];*/
int msg_id;
int mb_index_id;
int msg_budget_id;
struct timespec start, stop;

struct msg_buf_mb{
    long msg_type;
    int index;
} mb_index;

struct msg_buf_budget{
    long msg_type;
    int pid;
} budget_buf;

/*
 * It makes and call user processes
 */
void make_users();

/*
 * It makes and call nodes processes
 */
void make_nodes();
/*
 * It handles the signal received from other processes
 */
void handle_sigint(int);
void handle_user_term(int, siginfo_t *, void *);
/*
 * It calcolate the budget reading transaction from master_book.
 * First argument is for pid of user or node, the second to indicate which one is it
 * 0 for user
 * 1 for node
 */
int read_budget(int, int);

int main(){
    int i, duration;

    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    /*sigaction(SIGUSR1, &sa, NULL);*/
    /*sigaction(SIGUSR2, &sa, NULL);*/
    sa_user.sa_flags = SA_SIGINFO;
    sa_user.sa_sigaction = &handle_user_term;
    sigaction(SIGUSR2, &sa_user, 0);

    load_file();
    test_load_file();
    user_sem_set(1);
    nodes_sem_set(1);
    shm_user_set();
    shm_nodes_set();
    mb_index_id = msgget(MSG_INDEX_KEY, 0666 | IPC_CREAT | IPC_EXCL);
    TEST_ERROR
    msg_id = msgget(MSG_QUEUE_KEY, 0666 | IPC_CREAT | IPC_EXCL);
    TEST_ERROR;
    msg_budget_id = msgget(MSG_BUDGET_KEY, 0666 | IPC_CREAT | IPC_EXCL);
    TEST_ERROR

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_REGISTRY_SIZE, IPC_CREAT | 0666);
    TEST_ERROR;
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR;

    /*nodes_arr = malloc(sizeof(nodes_arr) * so_nodes_num);
    user_arr = malloc(sizeof(user_arr) * so_users_num);*/


    mb_index.msg_type = 1;
    mb_index.index = 0;
    msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);

    clock_gettime(CLOCK_REALTIME, &start);
    printf("\n\nStarting simulation\n\n");

    make_nodes();
    make_users();

    /*TEST_ERROR;*/
    sleep(1);

    sem_wait(nodes_sem);
    for (i = 0; i < so_users_num; i++){
        puser_shm[i][1] = 0;
        puser_shm[i][2] = 0;
    }
    for (i = 0; i < so_nodes_num; i++){
        pnodes_shm[i][1] = 0;
    }
    sem_post(nodes_sem);

    do{
        sem_wait(nodes_sem);
        msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
        msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);

        for (i = 0; i < so_users_num; i++){
            puser_shm[i][1] = read_budget(puser_shm[i][0], 0);
            if(puser_shm[i][2] == 0) {
                printf("working user %d with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
            }else{
                printf("non working user %d with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
            }
        }
        for (i = 0; i < so_nodes_num; i++){
            pnodes_shm[i][1] = read_budget(pnodes_shm[i][0], 1);
            printf("working nodes %d with budget %d\n", pnodes_shm[i][0], pnodes_shm[i][1]);
        }
        sem_post(nodes_sem);
        printf("\n");

        sleep(1);
        clock_gettime(CLOCK_REALTIME, &stop);
        duration = ((stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION);
        printf("\n\nduration %d\n\n", duration);
    } while (duration < so_sim_sec && user_done < so_users_num && master_index < SO_REGISTRY_SIZE);

    /*while(waitpid(-1, NULL, 0)){ master aspetta per ogni user di finire
        if(errno == ECHILD){
            break;
        }
    }*/

    /*for(i = 0; i < so_users_num; i++){
        printf("user pid n:%d - %d\n", i + 1, puser_shm[i]);
    }

    for(i = 0; i < so_nodes_num; i++){
        printf("nodes pid n:%d - %d\n", i + 1, pnodes_shm[i]);
    }*/
    for (i = 0; i < so_users_num; i++){
        kill(puser_shm[i][0], SIGINT);
    }
    for (i = 0; i < so_nodes_num; i++){
        kill(pnodes_shm[i][0], SIGINT);
    }
    while(waitpid(-1, NULL, 0)) { /*master aspetta per ogni user di finire*/
        if (errno == ECHILD) {
            break;
        }
    }

    printf("\n\n        at ending simulation\n");
    for (i = 0; i < so_users_num; i++){
        if(puser_shm[i][2] == 0) {
            printf("user %d with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
        }else{
            printf("user %d already terminated with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
        }
    }
    for (i = 0; i < so_nodes_num; i++){
        printf("nodes %d with budget %d\n", pnodes_shm[i][0], pnodes_shm[i][1]);
    }
    msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
    printf("number of blocks in master book - %d\n", mb_index.index);
    printf("\n");

    user_sem_del();
    nodes_sem_del();
    shmdt(puser_shm);
    shmctl(users_shm_id, IPC_RMID, NULL);
    shmdt(pnodes_shm);
    shmdt(pmaster_book);
    shmctl(nodes_shm_id, IPC_RMID, NULL);
    shmctl(master_book_id, IPC_RMID, NULL);
    msgctl(mb_index_id, IPC_RMID,NULL);
    msgctl(msg_id, IPC_RMID, NULL);
    msgctl(msg_budget_id, IPC_RMID, NULL);
    if(user_done == so_users_num) printf("\n\n\nending simulation due to user\n\n\n");
    else if(master_index == SO_REGISTRY_SIZE) printf("\n\n\nending simulation due to master book full\n\n\n");
    else printf("\n\n\nending simulation due to time\n\n\n");

    return 0;
}

void make_users(){
    int i;
    char *str_start_s;
    char *str_start_n;
    char *args_user[] = {USER_PATH, NULL, NULL, NULL};

    str_start_s = malloc(sizeof(double));
    str_start_n = malloc(sizeof(double));

    sprintf(str_start_s, "%ld", start.tv_sec);
    sprintf(str_start_n, "%ld", start.tv_nsec);

    args_user[1] = str_start_s;
    args_user[2] = str_start_n;

    /*free(str_budget_init);
    free(str_reward);*/

    for (i = 0; i < so_users_num; i++){
        pid = fork();
        if (pid < 0){
            TEST_ERROR;
            exit(EXIT_FAILURE);
        }
        else if (pid == 0){
            puser_shm[i][0] = getpid();
            execve(USER_PATH, args_user, NULL);
            TEST_ERROR;
        }
    }
}

void make_nodes()
{
    int i;
    char *str_start_s;
    char *str_start_n;
    char *args_nodes[] = {NODES_PATH, NULL, NULL, NULL};

    str_start_s = malloc(sizeof(double));
    str_start_n = malloc(sizeof(double));

    sprintf(str_start_s, "%ld", start.tv_sec);
    sprintf(str_start_n, "%ld", start.tv_nsec);

    args_nodes[1] = str_start_s;
    args_nodes[2] = str_start_n;

    for (i = 0; i < so_nodes_num; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            TEST_ERROR;
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            pnodes_shm[i][0] = getpid();
            execve(NODES_PATH, args_nodes, NULL);
            TEST_ERROR
        }
    }
}

int read_budget(int pid, int type){
    int i, j, pid_budget;
    if(type == 0)  pid_budget = so_budget_init;
    else pid_budget = 0;

    msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
    msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
    master_index = mb_index.index;
    for(i = 0; i < master_index; i++){
        for(j = 0; j < SO_BLOCK_SIZE; j++) {
            if(pmaster_book[i][j].receiver == pid){
                pid_budget = pmaster_book[i][j].amount + pid_budget;
            }
            if(pmaster_book[i][j].sender == pid){
                pid_budget =  pid_budget - (pmaster_book[i][j].amount + pmaster_book[i][j].reward);
            }
        }
    }

    return pid_budget;
}


void handle_sigint(int signal){
    int i;
    switch (signal){
    case SIGINT:
        for (i = 0; i < so_users_num; i++){
            kill(puser_shm[i][0], SIGINT);
        }
        for (i = 0; i < so_nodes_num; i++){
            kill(pnodes_shm[i][0], SIGINT);
        }

        while(waitpid(-1, NULL, 0)) { /*master aspetta per ogni user di finire*/
            if (errno == ECHILD) {
                break;
            }
        }

        printf("\n\nat ending simulation\n");
        for (i = 0; i < so_users_num; i++){
            if(puser_shm[i][2] == 0) {
                printf("user %d with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
            }else{
                printf("user %d already terminated with budget %d\n", (int) puser_shm[i][0], puser_shm[i][1]);
                }
            }
            for (i = 0; i < so_nodes_num; i++){
                printf("nodes %d with budget %d\n", pnodes_shm[i][0], pnodes_shm[i][1]);
            }
            printf("\n");

            user_sem_del();
            nodes_sem_del();
            shmdt(puser_shm);
            shmctl(users_shm_id, IPC_RMID, NULL);
            shmdt(pnodes_shm);
            shmdt(pmaster_book);
            shmctl(nodes_shm_id, IPC_RMID, NULL);
            shmctl(master_book_id, IPC_RMID, NULL);
            msgctl(msg_id, IPC_RMID, NULL);
            msgctl(mb_index_id, IPC_RMID,NULL);
            msgctl(msg_budget_id, IPC_RMID, NULL);
        printf("\n\n\nforced ending simulation...\n\n\n");
        sleep(1);

        exit(0);
    default:
        exit(EXIT_FAILURE);
    }
}
void handle_user_term(int signal, siginfo_t *si, void *data){
    int i;
    (void)signal;
    (void)data;
    for(i = 0; i < so_users_num; i++){
        if(puser_shm[i][0] == si->si_pid) {
            puser_shm[i][2] = 1;
        }
    }
    user_done++;
}


