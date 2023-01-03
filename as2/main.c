#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// CONSTANTS AND MACROS
// for readability
#define N_THREADS 13
#define N 5
#define END_OF_TIME 10
#define FOREVER while (!time_up)

// DEFINITIONS OF NEW DATA TYPES
// for readability
typedef char thread_name_t[10];

typedef enum {
    LOW = 0,
    HIGH = 1
} lowhigh_t;

typedef enum {
    FALSE,
    TRUE
} boolean;

typedef struct {
    int value;
    lowhigh_t p;
    boolean full;
} value_t;

// monitor also defined as a new data type
typedef struct {
    value_t buffer[N];           // monitor guarded buffer
    int in, out;                 // indexes for upload/download operations
    int n_low, n_high;           // number of full slots of each type
    pthread_mutex_t m;           // monitor lock
    int suspended_not_full[2];   // number of threads suspended due to buffer full
    int suspended_has_item[2];   // number of threads suspended due to buffer not containing required item
    pthread_cond_t not_full[2];  // not_full[LOW] => low prio queue, not_full[HIGH] => high prio queue
    pthread_cond_t has_item[2];  // has_item[LOW] => low prio queue, has_item[HIGH] => high prio queue
} monitor_t;

// GLOBAL VARIABLES
// the monitor should be defined as a global variable
monitor_t mon;
boolean time_up = FALSE;

//  MONITOR API
void monitor_upload(monitor_t *mon, char *name, int value, lowhigh_t p);
int monitor_download(monitor_t *mon, char *name, lowhigh_t t);
void monitor_init(monitor_t *mon);
void monitor_destroy(monitor_t *mon);

// OTHER FUNCTION DECLARATIONS
// functions corresponding to thread entry points
void *producer(void *arg);
void *consumer(void *arg);

double spend_some_time(int);
int produce(char *name);
void consume(char *name, int value);
void copy_to_buffer(monitor_t *mon, int value, lowhigh_t p);
int copy_from_buffer(monitor_t *mon, lowhigh_t t);
lowhigh_t f(int value);

// IMPLEMENTATION OF MONITOR API
void monitor_upload(monitor_t *mon, char *name, int value, lowhigh_t p) {
    pthread_mutex_lock(&mon->m);

    // if the buffer is full we have to wait
    while (mon->n_high + mon->n_low == N && !time_up) {
        // if it this we need to place the calling thread in a queue, BUT, we have to distinguish between
        // HIGH priority requests and LOW priority requests so that we can later signal to different
        // queues
        mon->suspended_not_full[p]++;
        pthread_cond_wait(&mon->not_full[p], &mon->m);
        mon->suspended_not_full[p]--;
    }

    if (!time_up) {
        // buffer is not full, hence we can copy an item into it
        copy_to_buffer(mon, value, p);
    }

    // lastly, we need to signal to any thread waiting to download an item of this priority
    // that we placed a new one into the buffer
    if (p == LOW) {
        if (mon->suspended_has_item[LOW] > 0)
            pthread_cond_signal(&mon->has_item[LOW]);
    } else {
        if (mon->suspended_has_item[LOW] > 0) {
            pthread_cond_signal(&mon->has_item[LOW]);
        } else if (mon->suspended_has_item[HIGH] > 0) {
            pthread_cond_signal(&mon->has_item[HIGH]);
        }
    }

    pthread_mutex_unlock(&mon->m);
}

int monitor_download(monitor_t *mon, char *name, lowhigh_t p) {
    int retval;
    pthread_mutex_lock(&mon->m);

    // if the buffer contains 0 items of the requested priority, we have to wait (note that if this condition is true,
    // it also means that the buffer is not empty so we don't need to check that condition too)
    while ((p == HIGH ? mon->n_high : mon->n_high + mon->n_low) == 0 && !time_up) {
        mon->suspended_has_item[p]++;
        pthread_cond_wait(&mon->has_item[p], &mon->m);
        mon->suspended_has_item[p]--;
    }

    if (!time_up) {
        // buffer is not empty, hence we can take an item from it
        retval = copy_from_buffer(mon, p);
    }

    // lastly, we need to signal to any thread waiting to upload that we took an item from the buffer,
    // given precedence to HIGH priority ones
    if (mon->suspended_not_full[HIGH] > 0) {
        pthread_cond_signal(&mon->not_full[HIGH]);
    } else if (mon->suspended_not_full[LOW] > 0) {
        pthread_cond_signal(&mon->not_full[LOW]);
    }

    pthread_mutex_unlock(&mon->m);
    return retval;
}

void monitor_init(monitor_t *mon) {
    int i;
    for (i = 0; i < N; i++)
        mon->buffer[i].full = FALSE;

    mon->in = 0;
    mon->out = 0;
    mon->n_high = 0;
    mon->n_low = 0;

    mon->suspended_has_item[LOW] = mon->suspended_has_item[HIGH] = 0;
    mon->suspended_not_full[LOW] = mon->suspended_not_full[HIGH] = 0;

    pthread_mutex_init(&mon->m, NULL);
    pthread_cond_init(&mon->not_full[LOW], NULL);
    pthread_cond_init(&mon->not_full[HIGH], NULL);
    pthread_cond_init(&mon->has_item[LOW], NULL);
    pthread_cond_init(&mon->has_item[HIGH], NULL);
}

void monitor_destroy(monitor_t *mon) {
    pthread_cond_destroy(&mon->not_full[LOW]);
    pthread_cond_destroy(&mon->not_full[HIGH]);
    pthread_cond_destroy(&mon->has_item[LOW]);
    pthread_cond_destroy(&mon->has_item[HIGH]);
    pthread_mutex_destroy(&mon->m);
}

// MAIN FUNCTION
int main(void) {
    pthread_t my_threads[N_THREADS];
    thread_name_t my_thread_names[N_THREADS];
    int i;

    // initialize monitor data strcture before creating the threads
    monitor_init(&mon);

    for (i = 0; i < N_THREADS / 2; i++) {
        sprintf(my_thread_names[i], "p%d", i);
        pthread_create(&my_threads[i], NULL, producer, my_thread_names[i]);
    }
    for (; i < N_THREADS; i++) {
        sprintf(my_thread_names[i], "c%d", i);
        pthread_create(&my_threads[i], NULL, consumer, my_thread_names[i]);
    }

    sleep(END_OF_TIME);
    time_up = TRUE;

    printf("Timeout! Exiting...\n");
    pthread_cond_broadcast(&mon.has_item[LOW]);
    pthread_cond_broadcast(&mon.has_item[HIGH]);

    for (i = 0; i < N_THREADS; i++) {
        pthread_join(my_threads[i], NULL);
    }

    // free OS resources occupied by the monitor after creating the threads
    monitor_destroy(&mon);

    return EXIT_SUCCESS;
}

int produce(char *name) {
    spend_some_time(15);
    return rand() % 10;
}

void consume(char *name, int value) {
    spend_some_time(15);
}

void *producer(void *arg) {
    char *name = arg;
    int value;
    lowhigh_t p;
    FOREVER {
        value = produce(name);
        p = f(value);
        if (!time_up) {
            printf("producer %s: uploading value %d (%s)\n", name, value, p == LOW ? "LOW" : "HIGH");
            monitor_upload(&mon, name, value, p);
            printf("producer %s: value %d (%s) uploaded\n", name, value, p == LOW ? "LOW" : "HIGH");
        }
    }
    pthread_exit(NULL);
}

void *consumer(void *arg) {
    char *name = arg;
    int value;
    lowhigh_t threshold;
    threshold = rand() % 2;
    printf("I am consumer %s (%s)\n", name, threshold == LOW ? "LOW" : "HIGH");
    FOREVER {
        value = monitor_download(&mon, name, threshold);
        printf("consumer %s: downloaded value %d (%s)\n", name, value, threshold == LOW ? "LOW" : "HIGH");
        consume(name, value);
    }
    pthread_exit(NULL);
}

// AUXILIARY FUNCTIONS
double spend_some_time(int max_steps) {
    double x, sum = 0.0, step;
    long i, N_STEPS = rand() % (max_steps * 1000000);
    step = 1 / (double)N_STEPS;
    for (i = 0; i < N_STEPS; i++) {
        x = (i + 0.5) * step;
        sum += 4.0 / (1.0 + x * x);
    }
    return step * sum;
}

lowhigh_t f(int value) {
    lowhigh_t p = (lowhigh_t)rand() % 2;
    printf("f(%d)=%s\n", value, p == LOW ? "LOW" : "HIGH");
    return p;
}

void copy_to_buffer(monitor_t *mon, int value, lowhigh_t p) {
    int count = 0;
    while (mon->buffer[mon->in].full) {
        mon->in = (mon->in + 1) % N;
        if (++count > N) {
            perror("buffer is inconsistent");
            abort();
        }
    }
    mon->buffer[mon->in].value = value;
    mon->buffer[mon->in].p = p;
    mon->buffer[mon->in].full = TRUE;
    if (p == HIGH)
        mon->n_high++;
    else
        mon->n_low++;
}

int copy_from_buffer(monitor_t *mon, lowhigh_t t) {
    int retval, count = 0;
    while ((mon->buffer[mon->out].full == FALSE) || (mon->buffer[mon->out].p < t)) {
        mon->out = (mon->out + 1) % N;
        if (++count > N) {
            perror("buffer is inconsistent");
            abort();
        }
    }
    retval = mon->buffer[mon->out].value;
    mon->buffer[mon->out].full = FALSE;
    if (mon->buffer[mon->out].p == HIGH)
        mon->n_high--;
    else
        mon->n_low--;

    return retval;
}
