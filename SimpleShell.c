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

#define MAX_HISTORY_LENGTH 100
#define MAX_LINE_LENGTH 100
#define MAX_CMDS_IN_LINE 10
#define MAX_ARGS_IN_CMD 10
#define MAX_PATH_LENGTH 20
#define MAX_NAME_LENGTH 20

typedef struct
{
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    unsigned int priority;

} process;

char *NCPU;
char *TSLICE;
int scheduler_pid;
int shm_fd;
process *submitted_process;

typedef struct history_node
{
    char line[MAX_LINE_LENGTH];
    struct history_node *next;
    time_t start_time;
    double runing_duration;
} history_node;

history_node *history_tail = NULL;
size_t num_history_node = 0;

void setup();
void cleanup();
void exec_line(char line[]);
void shell_loop();
void record_cmd(char line[]);
void read_history();
void write_history();
void sigint_handler();
void print_history();
int is_elf_file(char *filename);
int is_bash_script(char *filename);
void truncate_history();
history_node *new_node(char cmd[]);

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE>\n", argv[0]);
        exit(1);
    }

    NCPU = argv[1];
    TSLICE = argv[2];
    setup();

    scheduler_pid = fork();
    if (scheduler_pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (scheduler_pid == 0)
    {
        execl("./SimpleScheduler", "SimpleScheduler", NCPU, TSLICE, NULL);
        perror("execl");
        exit(1);
    }
    else
    {
        read_history();
        sleep(1);
        shell_loop();
        return 0;
    }
}

void setup()
{
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    shm_fd = shm_open("/submitted_process", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }
    if (ftruncate(shm_fd, sizeof(process)) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    submitted_process = (process *)mmap(NULL, sizeof(process), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (submitted_process == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
}

void shell_loop()
{
    char line[MAX_LINE_LENGTH];
    while (true)
    {
        printf("SimpleShell<3 ");
        fflush(stdout);

        if (fgets(line, MAX_LINE_LENGTH, stdin) == NULL)
        {
            perror("fgets");
            exit(1);
        }
        exec_line(line);
    }
}

void record_cmd(char cmd[])
{
    history_node *new_node = (history_node *)malloc(sizeof(history_node));
    strcpy(new_node->line, cmd);

    if (history_tail == NULL)
    {
        new_node->next = new_node;
        history_tail = new_node;
        num_history_node++;
    }
    else if (num_history_node == MAX_HISTORY_LENGTH)
    {
        new_node->next = history_tail->next->next;
        free(history_tail->next);
        history_tail->next = new_node;
    }
    else
    {
        new_node->next = history_tail->next;
        history_tail->next = new_node;
        history_tail = history_tail->next;
        num_history_node++;
    }

    time(&(history_tail->start_time));
}

void exec_line(char line[])
{

    line[strlen(line) - 1] = '\0';

    if (strcmp(line, "exit") == 0)
    {
        printf("Exiting...\n");
        munmap(submitted_process, sizeof(process));
        shm_unlink("/submitted_process");
        kill(scheduler_pid, SIGINT);
        int status;
        if (waitpid(scheduler_pid, &status, WUNTRACED) == -1)
        {
            perror("waitpid");
            exit(1);
        }
        printf("scheduler exited with status %d\n", WEXITSTATUS(status));
        write_history();
        exit(0);
    }
    else if (strcmp(line, "start") == 0)
    {
        kill(scheduler_pid, SIGUSR2);
        sleep(5);
        return;
    }

    char *commands[MAX_CMDS_IN_LINE];
    int num_cmds = 0;
    char *token = strtok(line, "|");

    while (token != NULL)
    {
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int fd[2];
    int prev_write_fd = 0;

    for (int i = 0; i < num_cmds; i++)
    {
        record_cmd(commands[i]);

        if (pipe(fd) == -1)
        {
            perror("pipe");
            exit(1);
        }

        int child_pid = fork();

        if (child_pid == -1)
        {
            perror("fork");
            exit(1);
        }
        else if (child_pid == 0)
        {
            if (i != 0)
            {
                dup2(prev_write_fd, STDIN_FILENO);
                close(prev_write_fd);
            }

            if (i != num_cmds - 1)
                dup2(fd[1], STDOUT_FILENO);

            char *args[MAX_ARGS_IN_CMD];
            int num_args = 0;
            token = strtok(commands[i], " ");

            while (token != NULL)
            {
                args[num_args++] = token;
                token = strtok(NULL, " ");
            }
            args[num_args] = NULL;

            if (strcmp(args[0], "submit") == 0)
            {
                if (num_args != 3 && num_args != 4)
                {
                    printf("usage: submit <proc name> <file path> <priority>(optional)\n");
                }

                shm_fd = shm_open("/submitted_process", O_RDWR, 0);
                if (shm_fd == -1)
                {
                    perror("shm_open");
                    exit(1);
                }

                submitted_process = (process *)mmap(NULL, sizeof(process), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
                if (submitted_process == MAP_FAILED)
                {
                    perror("mmap");
                    exit(1);
                }

                strcpy(submitted_process->name, args[1]);
                strcpy(submitted_process->path, args[2]);
                submitted_process->priority = 1;
                if (num_args == 4)
                {
                    submitted_process->priority = atoi(args[3]);
                }

                munmap(submitted_process, sizeof(process));
                kill(scheduler_pid, SIGUSR1);
                exit(0);
            }

            close(fd[0]);
            close(fd[1]);

            if (strcmp(args[0], "history") == 0)
            {
                print_history();
                exit(0);
            }
            if (strncmp(args[0], "./", 2) == 0 && is_elf_file(args[0]))
            {
                char *new_args[3];
                new_args[0] = "./launch";
                new_args[1] = (char *)args[0] + 2;
                new_args[2] = NULL;
                execv(new_args[0], new_args);
            }
            if (strncmp(args[0], "./", 2) == 0 && is_bash_script(args[0]))
            {
                char temp[100];
                FILE *ftemp = fopen(args[0], "r");
                fgets(temp, 100, ftemp);
                while (fgets(temp, 100, ftemp) != NULL)
                {
                    if (temp[0] != '#')
                        exec_line(temp);
                }
                fclose(ftemp);
                exit(0);
            }
            if (strncmp(args[0], "./", 2) == 0)
            {
                perror("not a elf or bashscript.\n");
                exit(1);
            }

            execvp(args[0], args);
            perror("execvp");
            exit(1);
        }
        else
        {
            close(fd[1]);
            if (i != 0)
                close(prev_write_fd);
            if (i == num_cmds - 1)
                close(fd[0]);
            prev_write_fd = fd[0];

            if (wait(NULL) == -1)
            {
                perror("wait");
                exit(1);
            }
            else
            {
                time_t now;
                time(&now);
                history_tail->runing_duration = difftime(now, history_tail->start_time);
            }
            sleep(1);
        }
    }
}

void sigint_handler()
{
    printf("history: \n");
    print_history();
    write_history();
    munmap(submitted_process, sizeof(process));
    shm_unlink("/submitted_process");
    printf("SIGINT received.\n");
    printf("exiting...\n");
    kill(scheduler_pid, SIGUSR2);
    exit(0);
}

void sinusr1_handler()
{
    kill(scheduler_pid, SIGUSR1);
}

history_node *new_node(char cmd[])
{
    history_node *new_node = (history_node *)malloc(sizeof(history_node));
    strcpy(new_node->line, cmd);
    new_node->next = new_node;
    return new_node;
}

void read_history()
{
    FILE *history;
    history = fopen("simple_shell_history.txt", "r");
    char cmd[MAX_LINE_LENGTH];
    int count = 0;
    while (fgets(cmd, MAX_LINE_LENGTH, history) != NULL && count < MAX_HISTORY_LENGTH)
    {
        cmd[strlen(cmd) - 1] = '\0';
        if (history_tail == NULL)
        {
            history_tail = new_node(cmd);
            fgets(cmd, MAX_LINE_LENGTH, history);
            sscanf(cmd, "%ld", &history_tail->start_time);
            fgets(cmd, MAX_LINE_LENGTH, history);
            sscanf(cmd, "%f", &history_tail->runing_duration);
            fflush(history);
        }
        else
        {
            history_node *temp = history_tail->next;
            history_tail->next = new_node(cmd);
            history_tail->next->next = temp;
            history_tail = history_tail->next;
            fgets(cmd, MAX_LINE_LENGTH, history);
            sscanf(cmd, "%ld", &history_tail->start_time);
            fgets(cmd, MAX_LINE_LENGTH, history);
            sscanf(cmd, "%f", &history_tail->runing_duration);
        }
        num_history_node++;
        count++;
    }
}

void write_history()
{
    FILE *history;
    history = fopen("simple_shell_history.txt", "w");
    int fd = fileno(history);
    if (ftruncate(fd, 0) == -1)
    {
        perror("ftruncate");
        fclose(history);
        exit(1);
    }
    history_node *head = history_tail->next;

    while (head != history_tail)
    {
        fprintf(history, "%s\n", head->line);
        fprintf(history, "%ld\n", head->start_time);
        fprintf(history, "%f\n", head->runing_duration);
        fflush(history);
        head = head->next;
    }
    fprintf(history, "%s\n", history_tail->line);
    fprintf(history, "%ld\n", head->start_time);
    fprintf(history, "%f\n", head->runing_duration);
    fclose(history);

    truncate_history();
}

void print_history()
{
    history_node *head = history_tail->next;
    int count = 1;
    while (head != history_tail)
    {
        printf("%.2d. %s\n", count++, head->line);
        printf("  run-time: %s", ctime(&(head->start_time)));
        printf("  run-durantion: %.4f\n", head->runing_duration);
        head = head->next;
    }
    printf("%.2d. %s\n", count, history_tail->line);
    printf("  run-time: %s", ctime(&(head->start_time)));
    printf("  run-durantion: %.4f\n", head->runing_duration);

    fflush(stdout);
}

int is_elf_file(char *filename)
{
    int elf_fd = open(filename, O_RDONLY);

    if (elf_fd == -1)
    {
        perror("opening file");
        exit(1);
    }

    uint8_t magic[4];
    if (read(elf_fd, magic, sizeof(uint8_t) * 4) != 4)
    {
        perror("Error reading magic bytes");
        close(elf_fd);
        exit(1);
    }

    close(elf_fd);
    if (magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F')
    {
        return true;
    }
    else
    {
        return false;
    }
}

int is_bash_script(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }

    char line[100];

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return false;
    }

    if (strcmp(line, "#!/bin/bash") == 0)
    {
        fclose(file);
        return true;
    }
    fclose(file);
    return false;
}

void truncate_history()
{
    while (history_tail != history_tail->next)
    {
        history_node *temp = history_tail->next->next;
        free(history_tail->next);
        history_tail->next = temp;
    }
    free(history_tail);
    history_tail = NULL;
}
