// Pull in from shared.h
#include "shared.h"

// Functions needed
void god(int signal);
void freeMem();
void getClock();
void getSema();
void getPCB();
void getMsg();
void semLock();
void semRelease();
void incTimer();

// Global Variables
// Pid list info
int *listOfPIDS;
int forkIndex = -1;

// Clock variables
key_t clockKey = -1;
int clockID = -1;
struct Clock *timer;

// Semaphore variables
key_t semaKey = -1;
int semaID = -1;
struct sembuf semOp;

// Process Control Block Variables
key_t pcbKey = -1;
int pcbID = -1;
struct ProcessControlBlock *pcb;

// Data/Resource Variables
key_t dataKey = -1;
int dataID = -1;
struct Data *data;

// Message Queues
key_t ossMsgKey = -1;
int ossMsgID = -1;
struct Message ossMsg;

key_t usrMsgKey = -1;
int usrMsgID = -1;
struct Message usrMsg;

// Queue
static struct Queue *queue;

// Verbose and line count are global because im lazy
bool verbose = false;
int lineCount = 0;

int main(int argc, char *argv[]){
	// Set up all shared memory
	getClock();
	getSema();
	getPCB();
	getMsg();

	// Set up signal handling
	signal(SIGINT, god);
	signal(SIGPROF, god);

	// Random Number generator
	srand(time(NULL));


    // Program Finished
	freeMem();
}


void freeMem(){
	shmctl(clockID, IPC_RMID, NULL);
	semctl(semaID, 0, IPC_RMID);
	shmctl(pcbID, IPC_RMID, NULL);
	shmctl(dataID, IPC_RMID, NULL);
	msgctl(ossMsgID, IPC_RMID, NULL);
	msgctl(usrMsgID, IPC_RMID, NULL);
	free(listOfPIDS);
}

void god(int signal){
	int i;
	for(i = 0; i < MAX_PROC; i++){
		kill(listOfPIDS[i], SIGTERM);
	}
	printf("GOD HAS BEEN CALLED AND THE RAPTURE HAS BEGUN. SOON THERE WILL BE NOTHING\n");
	freeMem();
	kill(getpid(), SIGTERM);
}

void getClock(){
	clockKey = ftok("./oss.c", 1);
	if(clockKey == -1){
		perror("ERROR IN oss.c: FAILED TO GENERATE KEY FROM SHARED MEM FOR CLOCK");
		god(1);
		exit(EXIT_FAILURE);
	}
	clockID = shmget(clockKey, sizeof(struct Clock*), 0666 | IPC_CREAT);
	if(clockID == -1){
		perror("ERROR IN oss.c: FAILED TO GET KEY FOR CLOCK");
		god(1);
		exit(EXIT_FAILURE);
	}
	timer = (struct Clock*)shmat(clockID, (void*) 0, 0);
	if(timer == (void*)-1){
		perror("ERROR IN oss.c: FAILED TO ATTACH MEMEORY FOR CLOCK");
		god(1);
		exit(EXIT_FAILURE);
	}
}

void getSema(){
	semaKey = ftok("./oss.c", 2);
	if(semaKey == -1){
		perror("ERROR IN oss.c: FAILED TO GENERATE KEY FROM SHARED MEM FOR SEMAPHORE");
		god(1);
		exit(EXIT_FAILURE);
	}
	semaID = semget(semaKey, 1, 0666 | IPC_CREAT);
	if(semaID == -1){
		perror("ERROR IN oss.c: FAILED TO GET KEY FOR SEMAPHORE");
		god(1);
		exit(EXIT_FAILURE);
	}
	semctl(semaID, 0, SETVAL, 1);
}

void getPCB(){
	pcbKey = ftok("./oss.c", 3);
	if(pcbKey == -1){
		perror("ERROR IN OSS.C: FAILED TO GENERATE KEY FROM SHARED MEEMORY FOR PCB");
		god(1);
		exit(EXIT_FAILURE);
	}
	size_t procTableSize = sizeof(struct ProcessControlBlock) * MAX_PROC;
	pcbID = shmget(pcbKey, procTableSize, 0666 | IPC_CREAT);
	if(pcbID == -1){
		perror("ERROR IN OSS.C: FAILED TO GET KEY FROM SHARED MEMEORY FOR PCB");
		god(1);
		exit(EXIT_FAILURE);
	}
	pcb = shmat(pcbID, (void*) 0, 0);
	if(pcb == (void*)-1){
		perror("ERROR IN OSS.C: FAILED TO ATTACH MEMORY FOR PCB");
		god(1);
		exit(EXIT_FAILURE);
	}
}

void getMsg(){
	ossMsgKey = ftok("./oss.c", 4);
	usrMsgKey = ftok("./oss.c", 5);
	if(ossMsgKey == -1 || usrMsgKey == -1){
		perror("ERROR IN OSS.C: FAILED TO GENERATE KEY FOR MESSAGE");
		god(1);
		exit(EXIT_FAILURE);
	}
	ossMsgID = msgget(ossMsgKey, IPC_CREAT | 0600);
	usrMsgID = msgget(usrMsgKey, IPC_CREAT | 0600);
}

/*
 * This process puts a semaphore lock on the timer, increases the time by a random amount between 1000 - 1 and checks in enough
 * microsconds have passed to equal a second and does math for it
 */
void incTimer(){
	semLock();

	int nsecInc = (rand() % 1000) + 1;
	timer->nsec += nsecInc;
	while(timer->nsec >= 1000000000){
		timer->sec++;
		timer->nsec -= 1000000000;
		// DEBUG printf("Stuck in timer?\n");
	}

	semRelease();
}

void semLock(){
	semOp.sem_num = 0;
	semOp.sem_op = -1;
	semOp.sem_flg = 0;
	semop(semaID, &semOp, 1);
}

void semRelease(){
	semOp.sem_num = 0;
	semOp.sem_op = 1;
	semOp.sem_flg = 0;
	semop(semaID, &semOp, 1);
}