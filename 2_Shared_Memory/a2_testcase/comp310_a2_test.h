/* Written by Xiru Zhu
 * This is for testing your assignment 2. 
 */
#ifndef __A2_TEST__
#define __A2_TEST__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include "a2_lib.h"
//LEAVE MAX KEYS as twice the number of pods
#define __TEST_MAX_KEY__  256 // Number of keys generated (get error when this > 1...)
#define __TEST_MAX_KEY_SIZE__ 31 // Max size of key
#define __TEST_MAX_DATA_LENGTH__ 256 // Max size of value
#define __TEST_MAX_POD_ENTRY__ 256 // Number of rounds (iterations)
#define __TEST_SHARED_MEM_NAME__ "/seanstappas_db"
#define __TEST_SHARED_SEM_NAME__ "/seanstappas_sem"
#define __TEST_FORK_NUM__ 4 // Number of forks in test 2
#define RUN_ITERATIONS 2000 // Number of iterations for test 2

sem_t *open_sem_lock;
pid_t pids[__TEST_FORK_NUM__];

void kill_shared_mem();
void intHandler(int dummy);
void generate_string(char buf[], int length);
void generate_key(char buf[], int length, char **keys_buf, int num_keys);

/*
    Generate random string
*/
void generate_string(char buf[], int length){
    int type;
    for(int i = 0; i < length; i++){
        type = rand() % 3;
        if(type == 2)
            buf[i] = rand() % 26 + 'a';
        else if(type == 1)
            buf[i] = rand() % 10 + '0';
        else
            buf[i] = rand() % 26 + 'A';
    }
    buf[length - 1] = '\0';
}
/*
    Generate random values
*/
void generate_unique_data(char buf[], int length, char **keys_buf, int num_keys){
    generate_string(buf, __TEST_MAX_DATA_LENGTH__);
    int counter = 0;
    for(int i = 0; i < num_keys; i++){
        if(strcmp(keys_buf[i], buf) == 0){ // Check if buf element matches key
            counter++;
        }
    }
    if(counter > 1){
        generate_unique_data(buf, length, keys_buf, num_keys); // Recursive, to guarantee uniqueness of random keys
    }
    return;
}
/*
    Generate random keys
*/
void generate_key(char buf[], int length, char **keys_buf, int num_keys){
    generate_string(buf, __TEST_MAX_KEY_SIZE__);
    int counter = 0;
    for(int i = 0; i < num_keys; i++){
        if(strcmp(keys_buf[i], buf) == 0){
            counter++;
        }
    }
    if(counter > 1){
        generate_key(buf, length, keys_buf, num_keys);
    }
    return;
}

#endif