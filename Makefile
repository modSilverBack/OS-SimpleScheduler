all:
	gcc -o SimpleScheduler SimpleScheduler.c
	-@./SimpleScheduler 4 10