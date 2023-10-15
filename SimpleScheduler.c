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

typedef struct
{
    pid_t pid;
    char *name;
    char *path;
    int started;
    int ended;
    int return_status;
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

typedef struct
{
    process *list;
    unsigned long num;
} procs;
procs *processes;

void setup();
void cleanup();
void swap(process *a, process *b);
void queue_proc(process *proc);
void max_heapify(unsigned int index);
process get_proc();
void handle_sigusr1(int signum);
void handle_sigusr2(int signum);
void handle_sigalrm(int signum);
void increment_wait_time();

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
    // b.priority = 1;
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


    printf("NCPU: %d TSLICE: %d\n", NCPU, TSLICE);
    while (1)
    {
        // sleep(1);
        // handle_sigusr2(SIGUSR2);
    }

    cleanup();
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

    waiting = (proc_queue *)malloc(sizeof(proc_queue));
    waiting->queue = (process *)malloc(100 * sizeof(process));
    waiting->capacity = 100;
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
    // shm_unlink("/submitted_process");
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

    if (left < waiting->size && waiting->queue[left].priority > waiting->queue[largest].priority ||
        (waiting->queue[left].priority == waiting->queue[largest].priority &&
         waiting->queue[left].insertion_order < waiting->queue[largest].insertion_order))
        largest = left;
    if (right < waiting->size && waiting->queue[right].priority > waiting->queue[largest].priority ||
        (waiting->queue[right].priority == waiting->queue[largest].priority &&
         waiting->queue[right].insertion_order < waiting->queue[largest].insertion_order))
        largest = right;

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

    if (waiting->size == 1)
    {
        waiting->size--;
        return waiting->queue[0];
    }

    process max = waiting->queue[0];
    waiting->queue[0] = waiting->queue[waiting->size - 1];
    waiting->size--;

    max_heapify(0);
    return max;
}

void handle_sigusr1(int signum)
{
    printf("sigusr1 handler entered\n");
    process p = {.name = submitted_process->name, .path = submitted_process->path, .priority = submitted_process->priority};
    p.insertion_order = waiting->insertion_count++;
    p.started = false;
    p.ended = false;
    p.exec_time = 0;
    p.wait_time = 0;
    queue_proc(&p);

    // process q = get_proc();
    printf("the name of the process submitted is : %s\n", p.name);
    return;
}

void handle_sigusr2(int signum)
{
    printf("intered sigusr2 handler\n");
    
    sleep(1);
    if (waiting->size == 0)
    {
        return;
    }
    ualarm(TSLICE, 0);

    while (waiting->size > 0 && num_of_proc_running < NCPU)
    {
        printf("the size of waiting queue is %d and %d process are running\n", waiting->size, num_of_proc_running);
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
        printf("the size of waiting queue is %d and %d process are running\n", waiting->size, num_of_proc_running);
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
    printf("sigalrm is recived\n");
    // printf("the size of waiting queue is %d and %d process are running\n", waiting->size, num_of_proc_running);

    int status;
    for (size_t i = 0; i < num_of_proc_running; i++)
    {
        running[i].exec_time += TSLICE;
        printf("running %d pid %d name %s\n", i, running[i].pid, running[i].name);
        int pid = waitpid(running[i].pid, &status, WNOHANG);

        printf("returned pid %d\n", pid);
        if (pid > 0)
        {
            printf("process name %s pid %d exec time %d wait time %d exit status %d finished execution\n",
                   running[i].name, running[i].pid, running[i].exec_time,
                   running[i].wait_time, running[i].return_status);
        }
        else
        {
            printf("process name %s pid %d stopped execution\n", running[i].name, running[i].pid);
            if (kill(running[i].pid, SIGSTOP) == -1)
            {
                perror("kill");
                exit(1);
            }
            queue_proc(&running[i]);
        }
    }
    num_of_proc_running = 0;

    increment_wait_time();
    printf("calling sigusr2\n");
    handle_sigusr2(SIGUSR2);
}
