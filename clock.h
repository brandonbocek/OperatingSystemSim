
#ifndef CLOCK_H
#define CLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define FILEPERMS (O_WRONLY | O_TRUNC | O_CREAT)
#define MSGSZ 128
#define PERM (S_IRUSR | S_IWUSR)
#define RPERM (S_IRUSR)

void signalHandler();
int initelement(int semid, int semnum, int semvalue);
int removesem(int semid);
int semop(int semid, struct sembuf *sops, size_t nsops);
void setsembuf(struct sembuf *s, int num, int op, int flg);
int r_semop(int semid, struct sembuf *sops, int nsops);
pid_t r_waitpid(pid_t pid, int *stat_loc, int options);
void printerror(char *msg, int error);
int removeSemaphore(int msgid);
int removeMessageQueue(int msgid);
int detachandremove(int shmid, void *shmaddr);


/*  The virtual clock */
struct SharedMemory {
	struct timespec timeStart, timeNow, timePassed;
	int clockSeconds;
	int clockNanoseconds;
};


/* The Message being sent from the user to oss  */
struct msg_buf {
	long mType;
	int pid;
	char shmMsg[MSGSZ];
};


enum state {idle, want_in, in_cs};

key_t key = 1221;
key_t send_key = 6325;
key_t recieve_key = 5236;

#endif
