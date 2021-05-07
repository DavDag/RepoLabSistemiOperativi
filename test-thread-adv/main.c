#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

static pthread_cond_t gCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;

#define BUFFER_SIZE 10

static int *gBuffer;
static int gBufferIndex = 0;

void *Producer(void *);
void *Consumer(void *);

void insertClosingPointForConsumers();

// Missing error checks
int main(int argc, char **argv)
{
    srand(time(NULL));
    pthread_t pTID1, pTID2, cTID1, cTID2;
    int id1 = 1, id2 = 2;
    gBuffer = (int *) malloc(sizeof(int) * BUFFER_SIZE);
    memset(gBuffer, 0, BUFFER_SIZE);
    pthread_create(&pTID1, NULL, Producer, &id1);
    pthread_create(&pTID2, NULL, Producer, &id2);
    pthread_create(&cTID1, NULL, Consumer, &id1);
    pthread_create(&cTID2, NULL, Consumer, &id2);
    pthread_join(pTID1, NULL);
    pthread_join(pTID2, NULL);
    insertClosingPointForConsumers();
    pthread_join(cTID1, NULL);
    pthread_join(cTID2, NULL);
    free(gBuffer);
    return 0;
}

void insertClosingPointForConsumers() {
    pthread_mutex_lock(&gMutex);
    while (gBufferIndex > 0) {
        pthread_cond_wait(&gCond, &gMutex);
    }
    gBuffer[gBufferIndex] = -1;
    printf("[MAIN] %d at index %d\n", -1, gBufferIndex);
    gBufferIndex++;
    pthread_cond_signal(&gCond);
    pthread_mutex_unlock(&gMutex);
}

void *Producer(void *args) {
    int id = *((int *) args);
    for (int i = 0; i < 25; ++i) {
        pthread_mutex_lock(&gMutex);
        while (gBufferIndex == BUFFER_SIZE) {
            pthread_cond_wait(&gCond, &gMutex);
        }
        const int val = rand() % 100;
        gBuffer[gBufferIndex] = val;
        printf("[P #%d] Produce : %d at index %d\n", id, val, gBufferIndex);
        gBufferIndex++;
        pthread_cond_signal(&gCond);
        pthread_mutex_unlock(&gMutex);
    }
    printf("Producer stopped\n");
    return NULL;
}

void *Consumer(void *args) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    int id = *((int *) args);
    int val = 0;
    while(val >= 0) {
        pthread_mutex_lock(&gMutex);
        while (gBufferIndex <= 0) {
            pthread_cond_wait(&gCond, &gMutex);
        }
        gBufferIndex--;
        val = gBuffer[gBufferIndex];
        printf("[C #%d] Consume : %d at index %d\n", id, val, gBufferIndex);
        if (val < 0) gBufferIndex++;
        pthread_cond_signal(&gCond);
        pthread_mutex_unlock(&gMutex);
    }
    printf("Consumer stopped\n");
    return NULL;
}
