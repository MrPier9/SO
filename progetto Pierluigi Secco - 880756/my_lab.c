#define _GNU_SOURCE_
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/shm.h>
#include "my_lab.h"

void user_sem_set(int sem_value)
{
    user_sem = sem_open(SNAME, O_CREAT | O_EXCL, 0644, sem_value);
    TEST_ERROR;
}

void nodes_sem_set(int sem_value)
{
    nodes_sem = sem_open(SNAME_N, O_CREAT | O_EXCL, 0644, sem_value);
    if (errno)
    {
        TEST_ERROR;
    }
}

void user_sem_del()
{
    sem_unlink(SNAME);
    sem_close(user_sem);
    sem_destroy(user_sem);
}

void nodes_sem_del()
{
    sem_unlink(SNAME_N);
    sem_close(nodes_sem);
    sem_destroy(nodes_sem);
}

void shm_user_set()
{
    users_shm_id = shmget(SHM_USERS_KEY, sizeof(puser_shm) * so_users_num, IPC_CREAT | 0666);
    TEST_ERROR;
    puser_shm = shmat(users_shm_id, NULL, 0);
    TEST_ERROR;
}

void shm_nodes_set()
{
    nodes_shm_id = shmget(SHM_NODES_KEY, sizeof(pnodes_shm) * so_nodes_num, IPC_CREAT | 0666);
    TEST_ERROR;
    pnodes_shm = shmat(nodes_shm_id, NULL, 0);
    TEST_ERROR;
}

double set_wait(unsigned long max, unsigned long min)
{
    srand(time(NULL));
    return rand() % (max - min + 1) + min;
}

/*
 * It loads the file with the data
 */
void load_file()
{
    char load[25];
    my_f = fopen("master_data.txt", "r");
    if (my_f == NULL)
    {
        TEST_ERROR;
        exit(EXIT_FAILURE);
    }

    while (fscanf(my_f, "%s", load) != EOF)
    {

        if (strcmp(load, "SO_USERS_NUM") == 0)
        {
            fscanf(my_f, "%d ", &so_users_num);
        }
        else if (strcmp(load, "SO_NODES_NUM") == 0)
        {
            fscanf(my_f, " %d ", &so_nodes_num);
        }
        else if (strcmp(load, "SO_BUDGET_INIT") == 0)
        {
            fscanf(my_f, " %d", &so_budget_init);
        }
        else if (strcmp(load, "SO_REWARD") == 0)
        {
            fscanf(my_f, " %d", &so_reward);
        }
        else if (strcmp(load, "SO_MIN_TRANS_GEN_NSEC") == 0)
        {
            fscanf(my_f, " %lu", &so_min_trans_gen_nsec);
        }
        else if (strcmp(load, "SO_MAX_TRANS_GEN_NSEC") == 0)
        {
            fscanf(my_f, " %lu", &so_max_trans_gen_nsec);
        }
        else if (strcmp(load, "SO_RETRY") == 0)
        {
            fscanf(my_f, " %d", &so_retry);
        }
        else if (strcmp(load, "SO_TP_SIZE") == 0)
        {
            fscanf(my_f, " %d", &so_tp_size);
        }
        else if (strcmp(load, "SO_MIN_TRANS_PROC_NSEC") == 0)
        {
            fscanf(my_f, " %lu", &so_min_trans_proc_nsec);
        }
        else if (strcmp(load, "SO_MAX_TRANS_PROC_NSEC") == 0)
        {
            fscanf(my_f, " %lu", &so_max_trans_proc_nsec);
        }
        else if (strcmp(load, "SO_SIM_SEC") == 0)
        {
            fscanf(my_f, " %d", &so_sim_sec);
        }
        else if (strcmp(load, "SO_NUM_FRIENDS") == 0)
        {
            fscanf(my_f, " %d", &so_num_friends);
        }
        else if (strcmp(load, "SO_HOPS") == 0)
        {
            fscanf(my_f, " %d", &so_hops);
        }
    }
    fclose(my_f);
}

/*
 * It checks the data loaded by - void load_file() -
 */
void test_load_file()
{
    printf("SO_USERS_NUM = %d\n", so_users_num);
    printf("SO_NODES_NUM = %d\n", so_nodes_num);
    printf("SO_BUDGET_INIT = %d\n", so_budget_init);
    printf("SO_REWARD = %d\n", so_reward);
    printf("SO_MIN_TRANS_GEN_NSEC = %lu\n", so_min_trans_gen_nsec);
    printf("SO_MAX_TRANS_GEN_NSEC = %lu\n", so_max_trans_gen_nsec);
    printf("SO_RETRY = %d\n", so_retry);
    printf("SO_TP_SIZE = %d\n", so_tp_size);
    printf("SO_MIN_TRANS_PROC_NSEC = %lu\n", so_min_trans_proc_nsec);
    printf("SO_MAX_TRANS_PROC_NSEC = %lu\n", so_max_trans_proc_nsec);
    printf("SO_SIM_SEC = %d\n", so_sim_sec);
    printf("SO_NUM_FRIENDS = %d\n", so_num_friends);
    printf("SO_HOPS = %d\n", so_hops);
}