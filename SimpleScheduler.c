#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <libelf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <semaphore.h>

#define MAX_PROCS_COUNT 1000
#define MAX_PATH_LENGTH 20
#define MAX_NAME_LENGTH 20
#define MAX_WAITING_PROC 100

typedef struct
{
    pid_t pid;
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    int started;
    unsigned int priority;
    unsigned int insertion_order;
    unsigned long exec_time;
    unsigned long wait_time;
} process;

unsigned short NCPU;
unsigned long TSLICE;

typedef struct
{
    process *queue;
    unsigned int size;
    unsigned int capacity;
    unsigned long insertion_count;
} proc_queue;

typedef struct
{
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    unsigned int priority;
} shared_proc_info;

proc_queue *waiting;
process *running;
int num_of_proc_running;

int shm_fd;
shared_proc_info *submitted_process;

void setup();
void cleanup();
void swap(process *a, process *b);
void queue_proc(process *proc);
void max_heapify(unsigned int index);
process get_proc();
void handle_sigusr1(int signum);
void handle_sigusr2(int signum);
void handle_sigalrm(int signum);
void handle_sigint(int signum);
void increment_wait_time();

int main(int argc, char const *argv[])
{
    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]) * 1000;
    setup();

    printf("Scheduler process started.\n");
    printf("NCPU: %d TSLICE: %d\n", NCPU, TSLICE / 1000);
    while (1)
    {
    }

    return 0;
}

void setup()
{
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (signal(SIGUSR2, handle_sigusr2) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (signal(SIGALRM, handle_sigalrm) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, handle_sigint) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    waiting = (proc_queue *)malloc(sizeof(proc_queue));
    waiting->capacity = MAX_WAITING_PROC;
    waiting->queue = (process *)malloc(MAX_WAITING_PROC * sizeof(process));
    waiting->size = 0;
    waiting->insertion_count = 0;
    running = (process *)malloc(NCPU * sizeof(process));

    shm_fd = shm_open("/submitted_process", O_RDWR, 0);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }

    submitted_process = (shared_proc_info *)mmap(NULL, sizeof(shared_proc_info), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (submitted_process == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
}

void cleanup()
{
    munmap(submitted_process, sizeof(shared_proc_info));
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

    if (left < waiting->size &&
        (waiting->queue[left].priority > waiting->queue[largest].priority ||
         (waiting->queue[left].priority == waiting->queue[largest].priority &&
          waiting->queue[left].insertion_order < waiting->queue[largest].insertion_order)))
    {
        largest = left;
    }
    
    if (right < waiting->size &&
        (waiting->queue[right].priority > waiting->queue[largest].priority ||
         (waiting->queue[right].priority == waiting->queue[largest].priority &&
          waiting->queue[right].insertion_order < waiting->queue[largest].insertion_order)))
    {
        largest = right;
    }

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
        process dummy = {.pid = 0, .name = "empty"};
        return dummy;
    }

    process max = waiting->queue[0];
    waiting->queue[0] = waiting->queue[waiting->size - 1];
    waiting->size--;

    max_heapify(0);
    return max;
}

void handle_sigusr1(int signum)
{

    process p;
    strcpy(p.name, submitted_process->name);  
    strcpy(p.path, submitted_process->path);
    p.priority = submitted_process->priority;
    p.started = false;
    p.exec_time = 0;
    p.wait_time = 0;
    queue_proc(&p);
}

void handle_sigusr2(int signum)
{
    if (waiting->size == 0)
    {
        return;
    }
    ualarm(TSLICE, 0);

    while (waiting->size > 0 && num_of_proc_running < NCPU)
    {
        process p = get_proc();
        if (!p.started)
        {
            p.pid = fork();
            if (p.pid == -1)
            {
                perror("fork");
                exit(1);
            }
            else if (p.pid == 0)
            {
                execlp(p.path, p.name, (char *)NULL);
                perror("execlp");
                exit(1);
            }
            else
            {
                printf("process %s with pid %d started execution\n", p.name, p.pid);
                p.started = 1;
            }
        }
        else
        {
            kill(p.pid, SIGCONT);
            printf("process %s with pid %d continued execution\n", p.name, p.pid);
        }
        running[num_of_proc_running++] = p;
    }
}

void increment_wait_time()
{
    for (size_t i = 0; i < waiting->size; i++)
    {
        waiting->queue[i].wait_time += TSLICE;
    }
}

void handle_sigalrm(int signum)
{
    increment_wait_time();

    int status;
    for (size_t i = 0; i < num_of_proc_running; i++)
    {
        running[i].exec_time += TSLICE;
        int pid = waitpid(running[i].pid, &status, WNOHANG);
        if (pid > 0)
        {
            printf("process name %s pid %d exec time %d wait time %d exit status %d finished execution\n",
                   running[i].name, running[i].pid, running[i].exec_time / 1000,
                   running[i].wait_time / 1000, WEXITSTATUS(status));
        }
        else
        {
            if (kill(running[i].pid, SIGSTOP) == -1)
            {
                perror("kill");
                exit(1);
            }
            queue_proc(&running[i]);
            printf("process name %s pid %d stopped execution\n", running[i].name, running[i].pid);
        }
    }
    num_of_proc_running = 0;

    handle_sigusr2(SIGUSR2);
}

void handle_sigint(int signum)
{
    cleanup();
    exit(0);
}
