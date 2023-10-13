#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

typedef struct process
{
    pid_t pid;
    char *name;
    unsigned int priority;
    unsigned int insertion_order;
    unsigned long exec_time;
    unsigned long wait_time;
} process;

unsigned short NCPU;
unsigned long TSLICE;

struct proc_queue
{
    process *queue;
    unsigned int size;
    unsigned int capacity;
    unsigned long insertion_count;
};

struct proc_queue *waiting;
pid_t *running;

void setup();
void cleanup();
void swap(process *a, process *b);
void queue_proc(process *proc);
void max_heapify(unsigned int index);
process get_proc();

int main(int argc, char const *argv[])
{
    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);
    setup();

    // process a, b, c, d, e;
    // a.pid = 1;
    // b.pid = 2;
    // c.pid = 3;
    // d.pid = 4;
    // e.pid = 5;

    // a.priority = 1;
    // b.priority = 2;
    // c.priority = 2;
    // d.priority = 3;
    // e.priority = 1;

    // queue_proc(&a);
    // queue_proc(&b);
    // queue_proc(&c);
    // queue_proc(&d);
    // queue_proc(&e);

    // process f;
    // f = get_proc();
    // printf("proc pid: %d prior: %d\n", f.pid, f.priority);
    // f = get_proc();
    // printf("proc pid: %d prior: %d\n", f.pid, f.priority);
    // f = get_proc();
    // printf("proc pid: %d prior: %d\n", f.pid, f.priority);
    // f = get_proc();
    // printf("proc pid: %d prior: %d\n", f.pid, f.priority);
    // f = get_proc();
    // printf("proc pid: %d prior: %d\n", f.pid, f.priority);

    // SIGSTOP AND SIGCONT and kill syscall to pause and resume child process

    // while (1)
    // {

    // }
    pid_t shell_pid = fork() if (shell_pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (shell_pid == 0)
    {
        execv("")
    }
    else
    {
    }

    cleanup();
    return 0;
}

void setup()
{
    waiting = malloc(sizeof(struct proc_queue));
    waiting->queue = malloc(100 * sizeof(process));
    waiting->capacity = 100;
    waiting->size = 0;
    waiting->insertion_count = 0;
    running = malloc(NCPU * sizeof(pid_t));
}

void cleanup()
{
    free(waiting->queue);
    free(waiting);
    free(running);
}

void swap(process *a, process *b)
{
    process temp = *a;
    *a = *b;
    *b = temp;
}

void queue_proc(process *proc)
{
    if (waiting->size == waiting->capacity)
    {
        printf("waiting queue is full, can not insert\n");
        return;
    }

    waiting->size++;
    int i = waiting->size - 1;
    proc->insertion_order = waiting->insertion_count++;
    waiting->queue[i] = *proc;

    while (i != 0 && waiting->queue[(i - 1) / 2].priority < waiting->queue[i].priority)
    {
        swap(&waiting->queue[i], &waiting->queue[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

void max_heapify(unsigned int index)
{
    unsigned int largest = index;
    unsigned int left = 2 * index + 1;
    unsigned int right = 2 * index + 2;

    // Find the largest among the root, left child, and right child
    if (left < waiting->size && waiting->queue[left].priority > waiting->queue[largest].priority ||
        (waiting->queue[left].priority == waiting->queue[largest].priority &&
         waiting->queue[left].insertion_order < waiting->queue[largest].insertion_order))
        largest = left;
    if (right < waiting->size && waiting->queue[right].priority > waiting->queue[largest].priority ||
        (waiting->queue[right].priority == waiting->queue[largest].priority &&
         waiting->queue[right].insertion_order < waiting->queue[largest].insertion_order))
        largest = right;

    // If the largest element is not the root, swap them and recursively heapify the affected subtree
    if (largest != index)
    {
        swap(&waiting->queue[index], &waiting->queue[largest]);
        max_heapify(largest);
    }
}

process get_proc()
{
    if (waiting->size <= 0)
    {
        printf("Heap is empty, Cannot extract\n");
        process dummy = {.pid = (pid_t)0, .name = "empty"};
        return dummy;
    }

    if (waiting->size == 1)
    {
        waiting->size--;
        return waiting->queue[0];
    }

    // Store the maximum value, and replace it with the last element in the heap
    process max = waiting->queue[0];
    waiting->queue[0] = waiting->queue[waiting->size - 1];
    waiting->size--;

    // Heapify the root element to maintain the max heap property
    max_heapify(0);

    return max;
}