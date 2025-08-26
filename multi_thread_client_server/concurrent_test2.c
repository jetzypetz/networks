#include <pthread.h>
#include <stdio.h>
void *print_a(void * arg){
    (void)arg;
    while(1){
        printf("a");
    }
    return NULL;
}
void *print_b(){
    while(1){
        printf("b");
    }
    return NULL;
}

int main(){
    pthread_t thread_a;
    if(pthread_create(&thread_a, NULL, print_a, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    print_b();
    return 0;
}
