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
#include "my_lab.h"

pid_t pid;
struct sigaction sa;
int user_done = 0;

void make_users();
void make_nodes();
void handle_sigint(int);

int main(){
    /*int i;*/
    
    
    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    
    mkfifo(TRANSACTION_FIFO, 0777);
    TEST_ERROR;
    if(errno == 17){
        exit(EXIT_FAILURE);
    }
    load_file();
    test_load_file();
    /*ser_sem_del();
    printf("user semaphore handled\n");*/
    user_sem_set(1);
    /*nodes_sem_del();*/
    nodes_sem_set(1);
    shm_user_set();
    shm_nodes_set();
    
    printf("\n\nStarting simulation\n\n");

    make_nodes();
    make_users();

    /*TEST_ERROR;*/

    sleep(so_sim_sec);

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

    user_sem_del();
    nodes_sem_del();
    shmdt(puser_shm);
    shmctl(users_shm_id, IPC_RMID, NULL);
    shmdt(pnodes_shm);
    shmctl(nodes_shm_id, IPC_RMID, NULL);
    system("rm trans_fifo");
    printf("\n\n\nnormal ending simulation\n\n\n");
    sleep(1);

    return 0;
}

void make_users(){
    int i;
    char *str_budget_init;
    char * str_reward;
    char * args_user[] = {USER_PATH, NULL, NULL, NULL};

    str_budget_init = malloc(sizeof(int) * 10);
    str_reward = malloc(sizeof(int) * 3);

    sprintf(str_budget_init, "%d", so_budget_init);
    sprintf(str_reward, "%d", so_reward);

    args_user[1] = str_budget_init;
    args_user[2] = str_reward;

    /*free(str_budget_init);
    free(str_reward);*/

    for(i = 0; i < so_users_num; i++){
        pid = fork();
        if(pid < 0){
            TEST_ERROR;
            exit(EXIT_FAILURE);
        }else if(pid == 0){
            sem_wait(user_sem);
            puser_shm[i] = getpid();
            sem_post(user_sem);
            execve(USER_PATH, args_user, NULL);
            TEST_ERROR;
        }
    }

}

void make_nodes(){
    int i;
    char *args_nodes[] = {NODES_PATH, NULL};

    for(i = 0; i < so_nodes_num; i++){
        pid = fork();
        if(pid < 0){
            TEST_ERROR;
            exit(EXIT_FAILURE);
        }else if(pid == 0){
            pnodes_shm[i] = getpid();
            execve(NODES_PATH, args_nodes, NULL);
            TEST_ERROR
        }
    }
}

void handle_sigint(int signal){
    int i;
    switch (signal){
    case SIGINT:
        for(i = 0; i < so_users_num; i++){
        kill(puser_shm[i], SIGINT);
        }
        for(i = 0; i < so_nodes_num; i++){
            kill(pnodes_shm[i], SIGINT);
        }
        user_sem_del();
        nodes_sem_del();
        shmdt(puser_shm);
        shmctl(users_shm_id, IPC_RMID, NULL);
        shmdt(pnodes_shm);
        shmctl(nodes_shm_id, IPC_RMID, NULL);
        system("rm trans_fifo");

        printf("\n\n\nforced ending simulation...\n\n\n");
        sleep(1);

        exit(0);
        break;
    case SIGUSR1:
        for(i = 0; i < so_users_num; i++){
        kill(puser_shm[i], SIGINT);
        }
        for(i = 0; i < so_nodes_num; i++){
            kill(pnodes_shm[i], SIGINT);
        }
        user_sem_del();
        nodes_sem_del();
        shmdt(puser_shm);
        shmctl(users_shm_id, IPC_RMID, NULL);
        shmdt(pnodes_shm);
        shmctl(nodes_shm_id, IPC_RMID, NULL);
        system("rm trans_fifo");

        printf("\n\n\nending simulation due to node...\n\n\n");
        sleep(1);

        exit(0);
        break;
    case SIGUSR2:
        user_done++;
        printf("\n\n\nuser done: %d\n\n\n", user_done);
        if(user_done == so_users_num){
            for(i = 0; i < so_nodes_num; i++){
                kill(pnodes_shm[i], SIGINT);
             }
        }
        break;
    default:
        break;
    }
    
}


