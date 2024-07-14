#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>

/**
 * Lock-Free and POSIX Mutex-Based Queue Implementation and Testing
 *
 * This program implements and compares two types of queues:
 * 1. Lock-free queue using atomic operations.
 * 2. Mutex-based queue using POSIX threads for synchronization.
 *
 * Both queues support the following operations:
 * - Enqueue: Add an element to the queue.
 * - Dequeue: Remove an element from the queue.
 * - Check if the queue is empty.
 *
 * The program creates multiple threads to perform enqueue and dequeue operations
 * on the queues to test their performance. It measures the time taken to perform
 * these operations and prints the performance comparison between the lock-free
 * queue and the POSIX mutex-based queue.
 */

typedef struct Node {
	long val;
	struct Node* next;
} Node;

Node* make_node(long val) {
	Node* n= malloc(sizeof(Node));
	n->val = val;
	n->next = NULL;

	return n;
}

/*
 * lock free queue 
 */
typedef struct {
	Node* front;
	Node* rear;
} lf_queue;

lf_queue* make_lf_queue() {
	lf_queue* q = malloc(sizeof(lf_queue));
	q->front = q->rear = NULL;

	return q;
}

void free_lf_queue(lf_queue* q) {
    Node* node = q->front;
    while(node) {
        Node* tmp = node;
        node = node->next;
        free(tmp);
    }

    free(q);
}

void lf_enqueue(lf_queue* q, long val) {
	Node* node = make_node(val);

	Node* old = q->rear;
	while(!atomic_compare_exchange_strong(&q->rear, &old, node));

	if (old == NULL) {
		q->front = node;
	} else {
		old->next = node;
	}
}

long lf_dequeue(lf_queue* q) {
	Node* old = q->front;
	while(old && !atomic_compare_exchange_strong(&q->front, &old, old->next));

    if (!old)
        return -1;
    if (!old->next)
        q->rear = NULL;

    long val = old->val;
    free(old);

	return val;
}

int is_lf_empty(lf_queue* q) {
	return q->front == NULL;
}

/*
 * queue using mutex 
 */
typedef struct {
    Node* front;
    Node* rear;

    pthread_mutex_t mutex;
} posix_queue;

posix_queue* make_posix_queue() {
    posix_queue* q = malloc(sizeof(posix_queue));
    q->front = q->rear = NULL;
    pthread_mutex_init(&q->mutex, NULL);

    return q;
}

void free_posix_queue(posix_queue * q) {
    Node* node = q->front;
    while(node) {
        Node* tmp = node;
        node = node->next;
        free(tmp);
    }

    pthread_mutex_destroy(&q->mutex);
    free(q);
}

void posix_enqueue(posix_queue* q, long val) {
    Node* node = make_node(val);

    pthread_mutex_lock(&q->mutex);

    if (q->front == NULL) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }

    pthread_mutex_unlock(&q->mutex);
}

long posix_dequeue(posix_queue* q) {
    pthread_mutex_lock(&q->mutex);

    Node* tmp = q->front;
    if (!tmp) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    long val = tmp->val;
    q->front = tmp->next;
    if (!q->front)
        q->rear = NULL;

    pthread_mutex_unlock(&q->mutex);

    free(tmp);
    return val;
}

int is_posix_empty(posix_queue* q) {
    pthread_mutex_lock(&q->mutex);
    int empty = q->front == NULL;
    pthread_mutex_unlock(&q->mutex);

    return q->front == NULL;
}

/*
 * tests
 */

#define OP_COUNT 10000
#define THREAD_COUNT 10

void* lf_test(void* handle) {
    lf_queue* q = handle;

    for(int i = 0; i < OP_COUNT; i++) {
        lf_enqueue(q, i);
        lf_dequeue(q);
        lf_enqueue(q, i);
        is_lf_empty(q);
    }

    for (int i = 0; i < OP_COUNT; i++)
        lf_dequeue(q);

    return NULL;
}

void* posix_test(void* handle) {
    posix_queue* q = handle;

    for(int i = 0; i < OP_COUNT; i++) {
        posix_enqueue(q, i);
        posix_dequeue(q);
        posix_enqueue(q, i);
        is_posix_empty(q);
    }

    for(int i = 0; i < OP_COUNT; i++)
        posix_dequeue(q);

    return NULL;
}

int main(const int argc, const char** argv) {
    pthread_t threads[THREAD_COUNT];
    clock_t start;

    // lf_test
    lf_queue* lf_q = make_lf_queue();
    start = clock();

    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_create(&threads[i], NULL, lf_test, lf_q);

    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_join(threads[i], NULL);

    double lf_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;

    // posix_test
    posix_queue* posix_q = make_posix_queue();
    start = clock();

    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_create(&threads[i], NULL, posix_test, posix_q);

    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_join(threads[i], NULL);

    double posix_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;

    // ----------------

    // printf("536382\n");
    printf("%d%\n", (int)((lf_time / posix_time) * 100));

    free_lf_queue(lf_q);
    free_posix_queue(posix_q);

	return 0;
}
