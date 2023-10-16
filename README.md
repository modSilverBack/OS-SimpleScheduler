# OS-SimpleScheduler
a simple scheduler which runs with simple shell to virtually schedule submitted processes in a round robin fashion
Contribution

1.Abhishek Beniwal- Priority queue, handle sigusr1,handle sigusr2,max heapify,setup.
2.Shivam Pandey- setup, handle sigalrm,cleanup,inc wait time, queue proc,handle sigint.
(Rest done Together)


In shell, its main function is that it Communicates with the scheduler using shared memory and signals.
 Global variables include pointers, process structure, and a linked list for storing command history.
In int main method, forks a child process for a scheduler, and enters a loop for shell commands.
In setup method, it sets up signal handling and shared memory for communication with the scheduler.
In clean up method, it handles cleanup tasks before exiting the program. It unmaps the shared memory, unlinks the shared memory object, writes the command history to a file, and exits
In exec method, it checks if the first argument of the command is "submit." If the condition is true, it checks the number of arguments (num_args). If the number of arguments is not 3 or 4, it prints a usage message and exits. If the number of arguments is correct, it opens a shared memory object (shm_open) and maps it into the process's address space (mmap). This shared memory is used for communication with the scheduler. It then copies relevant information (process name, file path, and priority) from the command arguments to the shared memory. If the optional priority argument is provided (num_args == 4), it converts the priority argument to an integer using atoi and assigns it to submitted_process->priority. After writing to shared memory, it unmaps the shared memory (munmap). It sends a signal (kill(scheduler_pid, SIGUSR1)) to notify the scheduler that a process has been submitted. Finally, it exits the child process (exit(0)).
In scheduler it manages a waiting queue of processes submitted by the shell. Implements a round-robin scheduling algorithm. Executes processes on the specified number of CPUs. The scheduler uses signals for communication. SIGUSR1 is used for process submission, SIGUSR2 for initiating process execution, and SIGALRM for time slicing. Processes are submitted with a name, path, and optional priority. The scheduler executes them in a round-robin fashion, considering their priorities. Adjust the NCPU and TSLICE parameters in both the shell and scheduler for different configurations.
 

Private git link-https://github.com/modSilverBack/OS-SimpleScheduler
