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
int user_done = 0;
int *user_arr;
int *nodes_arr;
int msg_id;
int mb_index_id;
struct timespec start, stop;

struct msg_buf_mb{
    long msg_type;
    int index;
} mb_index;

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

int main()
{
    int i,j , duration;


    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    load_file();
    test_load_file();
    user_sem_set(1);
    nodes_sem_set(1);
    shm_user_set();
    shm_nodes_set();
    msg_id = msgget(MSG_QUEUE_KEY, 0666 | IPC_CREAT | IPC_EXCL);
    TEST_ERROR;
    mb_index_id = msgget(MSG_INDEX_KEY, 0666 | IPC_CREAT | IPC_EXCL);
    TEST_ERROR;

    master_book_id = shmget(MASTER_BOOK_KEY, sizeof(master_book_page) * SO_REGISTRY_SIZE, IPC_CREAT | 0666);
    TEST_ERROR;
    pmaster_book = shmat(master_book_id, NULL, 0);
    TEST_ERROR;


    user_arr = (int *)malloc(sizeof(int) * so_users_num);
    nodes_arr = (int *)malloc(sizeof(int) * so_nodes_num);

    mb_index.msg_type = 1;
    mb_index.index = 0;
    msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);

    clock_gettime(CLOCK_REALTIME, &start);
    printf("\n\nStarting simulation\n\n");

    make_nodes();
    make_users();

    /*TEST_ERROR;*/
    sleep(5);
    sem_wait(user_sem);
    for (i = 0; i < so_users_num; i++)
    {
        user_arr[i] = puser_shm[i];
    }
    sem_post(user_sem);

    sem_wait(nodes_sem);
    for (i = 0; i < so_nodes_num; i++)
    {
        nodes_arr[i] = pnodes_shm[i];
    }
    sem_post(nodes_sem);


    do{
        /*for (i = 0; i < so_users_num; i++)
        {
            kill(user_arr[i], SIGSTOP);
        }
        for (i = 0; i < so_nodes_num; i++)
        {
            kill(nodes_arr[i], SIGSTOP);
        }

        for (i = 0; i < so_users_num; i++)
        {
            kill(user_arr[i], SIGSTOP);
            printf("working user %d with budget %d\n", user_arr[i], so_budget_init);
        }
        for (i = 0; i < so_nodes_num; i++)
        {
            printf("working nodes %d with budget %d\n", nodes_arr[i], so_budget_init);
        }
        printf("\n");
*/
        sem_wait(nodes_sem);
        msgrcv(mb_index_id, &mb_index, sizeof(mb_index), 1, 0);
        msgsnd(mb_index_id, &mb_index , sizeof(mb_index), 0);
        for(i = 0; i < mb_index.index; i++){
            for(j = 0; j < SO_BLOCK_SIZE; i++){
                printf("    timestamp: %f", (double)pmaster_book[i][j].timestamp);
                printf("    transaction sent: %.2f\n", pmaster_book[i][j].amount + pmaster_book[i][j].reward);
                printf("    sent to: %d\n", pmaster_book[i][j].receiver);
                printf("    sent by: %d", pmaster_book[i][j].sender);
                printf("    transaction amount: %.2f\n    reward: %.2f\n", \
                        pmaster_book[i][j].amount, pmaster_book[i][j].reward);
                printf("--------------------------------------------\n");
            }
            printf("\n");
        }
        sem_post(nodes_sem);
        sleep(1);
        clock_gettime(CLOCK_REALTIME, &stop);
        duration = ((stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / (double)BILLION);
        printf("%d\n", duration);
        for (i = 0; i < so_users_num; i++)
        {
            kill(user_arr[i], SIGCONT);
        }
        for (i = 0; i < so_nodes_num; i++)
        {
            kill(nodes_arr[i], SIGCONT);
        }
    } while (duration < so_sim_sec);

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
    for (i = 0; i < so_users_num; i++)
    {
        kill(user_arr[i], SIGINT);
    }
    for (i = 0; i < so_nodes_num; i++)
    {
        kill(nodes_arr[i], SIGINT);
    }


    user_sem_del();
    nodes_sem_del();
    /*shmdt(puser_shm);*/
    shmctl(users_shm_id, IPC_RMID, NULL);
    /*shmdt(pnodes_shm);*/
    shmctl(nodes_shm_id, IPC_RMID, NULL);
    shmctl(master_book_id, IPC_RMID, NULL);
    msgctl(msg_id, IPC_RMID, NULL);
    msgctl(mb_index_id, IPC_RMID,NULL);
    printf("\n\n\nnormal ending simulation\n\n\n");
    sleep(1);

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

    for (i = 0; i < so_users_num; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            TEST_ERROR;
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            /*sem_wait(user_sem);*/
            puser_shm[i] = getpid();
            /*sem_post(user_sem);*/
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
            pnodes_shm[i] = getpid();
            execve(NODES_PATH, args_nodes, NULL);
            TEST_ERROR
        }
    }
}

void handle_sigint(int signal){
    int i;
    switch (signal)
    {
    case SIGINT:
        for (i = 0; i < so_users_num; i++)
        {
            kill(puser_shm[i], SIGINT);
        }
        for (i = 0; i < so_nodes_num; i++)
        {
            kill(pnodes_shm[i], SIGINT);
        }
        user_sem_del();
        nodes_sem_del();
        shmdt(puser_shm);
        shmctl(users_shm_id, IPC_RMID, NULL);
        shmdt(pnodes_shm);
        shmctl(nodes_shm_id, IPC_RMID, NULL);
        shmctl(master_book_id, IPC_RMID, NULL);
        msgctl(msg_id, IPC_RMID, NULL);
        msgctl(mb_index_id, IPC_RMID, NULL);

        printf("\n\n\nforced ending simulation...\n\n\n");
        sleep(1);

        exit(0);
        break;
    case SIGUSR1:
        for (i = 0; i < so_users_num; i++)
        {
            kill(puser_shm[i], SIGINT);
        }
        for (i = 0; i < so_nodes_num; i++)
        {
            kill(pnodes_shm[i], SIGINT);
        }
        user_sem_del();
        nodes_sem_del();
        shmdt(puser_shm);
        shmctl(users_shm_id, IPC_RMID, NULL);
        shmdt(pnodes_shm);
        shmctl(nodes_shm_id, IPC_RMID, NULL);
        shmctl(master_book_id, IPC_RMID, NULL);
        msgctl(msg_id, IPC_RMID, NULL);
        msgctl(mb_index_id, IPC_RMID, NULL);

        printf("\n\n\nending simulation due to node...\n\n\n");
        sleep(1);
        exit(0);
        break;
    case SIGUSR2:
        user_done++;
        printf("\n\n\nuser done: %d\n\n\n", user_done);
        if (user_done == so_users_num)
        {
            for (i = 0; i < so_nodes_num; i++)
            {
                kill(pnodes_shm[i], SIGINT);
            }
        }
        break;
    default:
        exit(EXIT_FAILURE);
        break;
    }
}
