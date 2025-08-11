// INCLUDE
#include <stdio.h>
#include <pthread.h>

// INITIALISE I
int i = 0;
int turn = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// CONCURRENT FUNCTIONS
void * f0() {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (turn !=0) {
            pthread_cond_wait(&cond, &mutex);
        }
        i++;
        printf("%d\n", i);
        turn = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void * f1() {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (turn != 1) {
            pthread_cond_wait(&cond, &mutex);
        }
        i++;
        printf("%d\n", i);
        turn = 0;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
    
// MAIN
int main(int argc, char ** argv) {
    pthread_t thread;
    // start first thread
    if (pthread_create(&thread, NULL, f0, NULL)) {
        fprintf(stderr, "failed to create thread\n");
        return -1;
    }
    // start second thread
    f1();
    return 0;
}
