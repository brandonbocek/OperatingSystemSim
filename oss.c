#include "clock.h"

void printHelpMenu();
void killAllProcesses();

void printerror(char *msg, int error) {
	fprintf(stderr, "[%ld] %s: %s\n", (long)getpid(), msg, strerror(error));
}

struct SharedMemory *shm;
int msgid, msgid_two, shmid;
FILE *fp;
int semid;

int main(int argc, char* argv[]) {

	char *filename = "log.out";
	const char *PATH = "./user";
	int z = 20;
	int totalProcesses = 0;
	int slaveProcesses = 5;
	int c;
		
	signal(SIGINT, signalHandler);

	/* Handling command line arguments w/ ./oss  */
	while ((c = getopt (argc, argv, "hs:l:t:")) != -1) {
		switch (c) {
			case 'h':
				printHelpMenu();
				exit(EXIT_SUCCESS);
				
			case 's':
				/*  Change the number of child processes that will spawn */
				if(isdigit(*optarg)) {
					if(atoi(optarg) <= 20) {
						slaveProcesses = atoi(optarg);
					} else {
						printf("It's too dangerous to spawn more than 20 processes. I'll give you 5.\n");
					}
				} else {
					fprintf(stderr, "Give a number with '-s'\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
				
			case 'l':
				/*  Change the name of the file to write to */
				filename = optarg;
				break;

			case 't':
				/*  Change the time before the master terminates */
				if(isdigit(*optarg)) {
					z = atoi(optarg);
				} else {
					fprintf(stderr, "'Give a number with -t'\n", optarg);
					exit(EXIT_FAILURE);
				}

				break;
				
			case '?':
				/* User entered a valid flag but nothing after it */
				if(optopt == 's' || optopt == 'l' || optopt == 't') {
					fprintf(stderr, "-%c needs to have an argument!\n", optopt);
				} else {
					fprintf(stderr, "%s is an unrecognized flag\n", argv[optind - 1]);
				}
			default:	
				/* User entered an invalid flag */
				printHelpMenu();
		}
	}
	
	if((shmid = shmget(key, sizeof(struct SharedMemory) * 2, IPC_CREAT | PERM)) < 0) {
		perror("shmget");
		fprintf(stderr, "shmget() returned an error! Program terminating...\n");
		exit(EXIT_FAILURE);
	}
	
    	if((shm = (struct SharedMemory *)shmat(shmid, NULL, 0)) == (struct SharedMemory *) -1) {
		perror("shmat");
        	fprintf(stderr, "shmat() returned an error! Program terminating...\n");
        	exit(EXIT_FAILURE); 
    	}
	int msgflg = IPC_CREAT | 0666;
	struct msg_buf msgbuff_one, msgbuff_two;

	/*  Establishing a message send */
	if ((msgid = msgget(send_key, msgflg)) < 0) {
        	perror("failed to establish sending");
        	exit(EXIT_FAILURE);
    	}
	
	/*  Establishing a message receive */
	if ((msgid_two = msgget(recieve_key, msgflg)) < 0) {
        	perror("failed to establish receiving");
        	exit(EXIT_FAILURE);
    	}
	
	/* Open the log file to write to it */		
	fp = fopen(filename, "a");
	if(fp == NULL) {
		printf("Couldn't open file");
		errno = ENOENT;
		killAllProcesses();
		exit(EXIT_FAILURE);
	}

	pid_t pid, wpid, slaves[slaveProcesses];
	int index = 0;
	int processesRemaining = 100;
	int status;

	/*  Start the virtual clock, putting real seconds and nanosecs into shared memory */
	clock_gettime(CLOCK_MONOTONIC, &shm->timeStart);
	clock_gettime(CLOCK_MONOTONIC, &shm->timeNow);
	shm->clockSeconds = shm->timeNow.tv_sec - shm->timeStart.tv_sec;
	shm->clockNanoseconds = shm->timeNow.tv_nsec - shm->timeStart.tv_nsec / 1E9;

	if ((semid = semget(IPC_PRIVATE, 1, PERM)) == -1) {
		perror("Failed to create a private semaphore");
		return 1;
	}
	if (initelement(semid, 0, 1) == -1) {
		perror("Failed to initialize semaphore element to 1");
		if (removesem(semid) == -1)
			perror("Failed to remove failed semaphore");
			return 1;
	}
	
	while(1) {
		if((index < slaveProcesses) && (z < 100)) {
//			printf("About to fork\n");
			pid = fork();
			totalProcesses++;
			index++;
			
			if (pid == 0) {
//				printf("About to exec to child process\n");
				char *id;
				sprintf(id, "%i", index);
				execl(PATH, id, filename, (char *)NULL);
				_exit(EXIT_FAILURE);
			}
		} else {
		/* Getting the children's messages and printing them out */
			if(!(msgrcv(msgid, &msgbuff_one, MSGSZ, 1, IPC_NOWAIT) < 0)) {
				index--;
				printf("Child %i is terminating at my time %i.%u because it reached %s in slave\n",
					 msgbuff_one.pid, shm->clockSeconds, shm->clockNanoseconds, msgbuff_one.shmMsg);	
				fprintf(fp, "Child %i is terminating at my time %i.%u because it reached %s in slave\n", 
					msgbuff_one.pid, shm->clockSeconds, shm->clockNanoseconds, msgbuff_one.shmMsg);
				if(totalProcesses == 100) {
					break;
				}
			}
		}
		/*  Get the updated time to see if termination time was reached */
		clock_gettime(CLOCK_MONOTONIC, &shm->timeNow);
		shm->clockSeconds = shm->timeNow.tv_sec - shm->timeStart.tv_sec;
		shm->clockNanoseconds = shm->timeNow.tv_nsec - shm->timeStart.tv_nsec / 1E9;
		wpid = waitpid(-1, &status, WNOHANG);
		
		if(shm->clockSeconds >= z) {
			printf("Time limit reached, ending everything\n");
			signalHandler();
		}
	}
	
	/* Letting the child processes finish  */
	int finish;
	while ((wpid = wait(&finish)) > 0);
	
	killAllProcesses();
	printf("Exiting normally\n");
	sleep(1);
	return 0;
}

int initelement(int semid, int semnum, int semvalue) {
	union semum {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semvalue;
	return semctl(semid, semnum, SETVAL, arg);
}

int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

void setsembuf(struct sembuf *s, int num, int op, int flg) {
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}

int r_semop(int semid, struct sembuf *sops, int nsops) {
	while (semop(semid, sops, nsops) == -1)
	if (errno != EINTR)
		return -1;
	return 0;
}

int removeSemaphore(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

int removeMessageQueue(int msgid) {
	return msgctl(msgid, IPC_RMID, NULL);
}
int detachandremove(int shmid, void *shmaddr) {
	int error = 0;
	if (shmdt(shmaddr) == -1)
		error = errno;
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
		error = errno;
	if (!error)
		return 0;
	errno = error;
	return -1;
}

void printHelpMenu() {
	printf("\n\t\t~~Help Menu~~\n\t-h This Help Menu Printed\n");
	printf("\t-s *# of slave processes to spawn*\t\tie. '-s 5'\n");
	printf("\t-l *log file used*\t\t\t\tie. '-l log.out'\n");
	printf("\t-t *time in seconds the master will terminate*\tie. -t 20\n\n");
}


void signalHandler() {	
	removeSemaphore(semid);
	removeMessageQueue(msgid);
	removeMessageQueue(msgid_two);
	detachandremove(shmid, shm);
	pid_t id = getpgrp();
	killpg(id, SIGINT);
	sleep(1);
	exit(EXIT_SUCCESS);
}

void killAllProcesses() {
	removeSemaphore(semid);
	removeMessageQueue(msgid);
	removeMessageQueue(msgid_two);
	detachandremove(shmid, shm);
	fclose(fp);
}
