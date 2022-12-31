/*
 ============================================================================
 Name        : 2020-assignment-buddies.c
 Author      : PT
 Version     :
 Copyright   : For personal use only. Please do not redistribute
 Description : Template for semaphore assignment summer 2020
 ============================================================================
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FOREVER while (!time_up)
#define N_THREADS 19
#define GROUP_SIZE 5

/* the following values are just examples of the possible duration
 * of each action and of the simulation: feel free to change them */
#define REFILL_TIME 1
#define MINGLE_TIME 5
#define END_OF_TIME 120

typedef char name_t[20];
typedef enum { FALSE,
               TRUE } boolean;

typedef struct {
    name_t thread_name;
    int drinks;
} stats_t;

time_t big_bang;
boolean time_up = FALSE;

int missing_to_toast = GROUP_SIZE;
pthread_mutex_t mutex1;

int ready_to_drink = 0;
pthread_mutex_t mutex2;

sem_t waiting_to_toast;
sem_t can_toast;
sem_t can_drink;

// GLOBAL VARS, TYPE DEFS ETC
int cmpfunc(const void *a, const void *b) {
    stats_t *A = *(stats_t **)a;
    stats_t *B = *(stats_t **)b;
    return A->drinks - B->drinks;
}

void wait_for_toasting(char *thread_name) {
    sem_wait(&waiting_to_toast);
    pthread_mutex_lock(&mutex1);
    missing_to_toast--;
    if (missing_to_toast > 0) {
        pthread_mutex_unlock(&mutex1);
        sem_post(&waiting_to_toast);
        sem_wait(&can_toast);

        pthread_mutex_lock(&mutex1);
        missing_to_toast++;
        if (missing_to_toast < GROUP_SIZE) {
            sem_post(&can_toast);
        }
        pthread_mutex_unlock(&mutex1);
    } else {
        missing_to_toast++;
        pthread_mutex_unlock(&mutex1);
        sem_post(&can_toast);
    }
}

void wait_for_drinking() {
    pthread_mutex_lock(&mutex2);
    ready_to_drink++;
    if (ready_to_drink < GROUP_SIZE) {
        pthread_mutex_unlock(&mutex2);
        sem_wait(&can_drink);

        pthread_mutex_lock(&mutex2);
        ready_to_drink--;
        if (ready_to_drink > 0) {
            sem_post(&can_drink);
        } else {
            sem_post(&waiting_to_toast);
        }
        pthread_mutex_unlock(&mutex2);
    } else {
        ready_to_drink--;
        pthread_mutex_unlock(&mutex2);
        sem_post(&can_drink);
    }
}

void initialize() {
    time(&big_bang);

    if (sem_init(&waiting_to_toast, 0, 1) != 0) {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&mutex1, NULL) != 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&can_toast, 0, 0) != 0) {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&mutex2, NULL) != 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&can_drink, 0, 0) != 0) {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }
}

void do_action(char *thread_name, char *action_name, int max_delay) {
    // you can use thread_name and action_name if you wish to display some output
    int delay = rand() % max_delay + 1;
    sleep(delay);
}

void *buddy(void *thread_name) {
    stats_t *stats = malloc(sizeof(stats_t));
    strncpy(stats->thread_name, thread_name, 20);

    printf("%s joined the party\n", (char *)thread_name);
    FOREVER {
        do_action(thread_name, "go refill glass", REFILL_TIME);

        wait_for_toasting(thread_name);

        if (!time_up)
            printf("[%4.0f]\t%s: Skol!\n", difftime(time(NULL), big_bang), (char *)thread_name);

        wait_for_drinking();

        if (!time_up) {
            stats->drinks++;
            printf("[%4.0f]\t%s drinks (#%d)\n", difftime(time(NULL), big_bang), (char *)thread_name, stats->drinks);
            do_action(thread_name, "drink and mingle", MINGLE_TIME);
        }
    }
    printf("%s left the party\n", (char *)thread_name);
    pthread_exit(stats);
}

int main(void) {
    int i;
    pthread_t tid[N_THREADS];
    stats_t *stats[N_THREADS];
    name_t thread_name[N_THREADS] = {"Ali", "Burhan", "Cristina", "Daniele",
                                     "Enrica", "Filippo", "Girish", "Heidi", "Ivan", "Jereney", "Kathy", "Luca",
                                     "Mehran", "Nick", "Orazio", "Prem", "Quentin", "Reza", "Saad"};

    initialize();

    puts("\nWELCOME BUDDIES\n");

    for (i = 0; i < N_THREADS; i++) {
        pthread_create(&tid[i], NULL, buddy, thread_name[i]);
    }

    sleep(END_OF_TIME);
    time_up = TRUE;
    // TODO: is this the correct way to unlock all waiting threads?
    missing_to_toast = 0;
    for (i = 0; i < N_THREADS; i++) {
        sem_post(&waiting_to_toast);
    }

    ready_to_drink = N_THREADS;
    for (i = 0; i < N_THREADS; i++) {
        sem_post(&can_drink);
    }

    for (i = 0; i < N_THREADS; i++) {
        pthread_join(tid[i], (void **)&stats[i]);
    }

    qsort(stats, N_THREADS, sizeof(stats_t *), cmpfunc);
    stats_t *most_sober = stats[0];
    stats_t *thirstiest = stats[N_THREADS - 1];

    printf("\nParty statistics.\n- The most sober participant was %s, had only %d glasses :(\n- The thirstiest was %s. That party animal drunk as many as %d glasses B)\n",
           most_sober->thread_name, most_sober->drinks, thirstiest->thread_name, thirstiest->drinks);
    puts("\nGOODNIGHT BUDDIES\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}
