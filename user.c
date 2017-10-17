#include "clock.h"

int msgid, msgid_two, shmid;
struct SharedMemory *shm;

int main(int argc, char* argv[]) {

	int error;
	int i, j, n;
	int semid;
	struct sembuf semwait[1];
	struct sembuf semsignal[1];
	struct sembuf sem;
	signal(SIGINT, signalHandler);	
	pid_t pid = getpid();

	/*  Create and attach shared memory to child process */
	if((shmid = shmget(key, sizeof(struct SharedMemory) * 2, 0666)) < 0) {
		fprintf(stderr, "Child: shmget() returned an error! Program terminating...\n");
		shmdt(shm);
		exit(EXIT_FAILURE);
	}

	if ((shm = (struct SharedMemory *)shmat(shmid, NULL, 0)) == (struct SharedMemory *) -1) {
		fprintf(stderr, "shmat() returned an error! Program terminating...\n");
		shmdt(shm);
		exit(EXIT_FAILURE);
	}

	/*  Establishing the send and receive for the message passed to the master */
	if((msgid_two = msgget(recieve_key, 0666)) < 0) {
		fprintf(stderr, "Child %i has failed attaching the recieve queue", pid);
		shmdt(shm);
		exit(EXIT_FAILURE);
	}
	
	if((msgid = msgget(send_key, 0666)) < 0) {
		fprintf(stderr, "Child %i has failed attaching the sending queue", pid);
		shmdt(shm);
		exit(EXIT_FAILURE);
	}

	/* Creating a Semaphore */
	if ((semid = semget(IPC_PRIVATE, 1, RPERM | IPC_EXCL)) == -1) {
		perror("Failed to create a private semaphore");
		return 1;
	}
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	

		/*  https://linux.die.net/man/3/clock_gettime */
		struct timespec tm, timePassed;
		clock_gettime(CLOCK_MONOTONIC, &tm);
	
		srand((unsigned)(tm.tv_sec ^ tm.tv_nsec ^ (tm.tv_nsec >> pid)));
		int duration = (rand() % 100001);

		clock_gettime(CLOCK_MONOTONIC, &timePassed);
	
		/* Wait for the random duration */
		while(1){
								
			/* WAIT if it's not your turn to enter */	
			r_semop(semid, semwait, 1);
			/**************  CRITICAL SECTION STARTS **************/
				
			if(shm->clockNanoseconds < (shm->clockNanoseconds + duration)){
				r_semop(semid, semsignal, 1);
				break;
			}
			r_semop(semid, semsignal, 1);
		}
		struct msg_buf sendbuf, msgbuff_two;
	
		sendbuf.mType = 1;
		sendbuf.pid = pid;
	
		/*********** Exit Section *************/
		/*  save the time format to the shmMsg and send the message to the master */
		sprintf(sendbuf.shmMsg, "%i.%u", shm->clockSeconds, shm->clockNanoseconds);
	
		if(msgsnd(msgid, &sendbuf, MSGSZ, IPC_NOWAIT) < 0) {
			fprintf(stderr, "msgsnd retuned an error in slave.c pid: %i. Program terminating...\n", pid);
			shmdt(shm);
			exit(EXIT_FAILURE);
		}
	



	/************** Reaminder Section *****************/
	if ((r_wait(NULL) == -1) && (errno != ECHILD))
		printerror("Failed to wait", errno);
	if ((i == 1) && ((error = removesem(semid)) == -1)) {
		printerror("Failed to clean up", error);
		return 1;
	}	
	
	shmdt(shm);
	exit(3);
}

int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

/*  Initializing the Semaphore Buffer */
void setsembuf(struct sembuf *s, int num, int op, int flg) {
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}

/* Restarts semop after a signal */
int r_semop(int semid, struct sembuf *sops, int nsops) {
	while (semop(semid, sops, nsops) == -1)
	if (errno != EINTR)
		return -1;
	return 0;
}

pid_t r_wait(int *stat_loc) {
	pid_t retval;
	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR)) ;
	return retval;
}

void printerror(char *msg, int error) {
	fprintf(stderr, "[%ld] %s: %s\n", (long)getpid(), msg, strerror(error));
}

/*  Kills all when signal is received */
void signalHandler() {
	pid_t id = getpid();
	printf("Signal was Received and the Master Process with ID: %i will be terminated\n", id);
	shmdt(shm);
	killpg(id, SIGINT);
	exit(EXIT_SUCCESS);
}

