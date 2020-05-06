#include "shared.h"

// Functions used
void getMsg();
void getClock();
void getSema();
void getPCB();
void semLock();
void semRelease();


// Global varaiables
// Clock variables
key_t clockKey = -1;
int clockID = -1;
struct Clock *timer;

// Semaphore variables
key_t semaKey = -1;
int semaID = -1;
struct sembuf semOp;

// PCB Variabled
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

int main(int argc, char *argv[]){
	// Get all shared memeory stuff
	getMsg();
	getClock();
	getSema();
	getPCB();

	// Get this proceess's pid
	pid_t thisPID = getpid();
	srand(thisPID);
	int index = atoi(argv[1]);

	unsigned int address = 0;
	unsigned int requestPage = 0;
	int loopCount = 0;
	int loopMax = (rand() % (1100 - 900 + 1)) + 900;

	//printf("Child with index %d has started\n", index);
	
	while(1){
		//fprintf(stderr, "Ready to recieve message in user %d Loop Count: %d Loop Max: %d\n", index, loopCount, loopMax);
		int rsvRes = msgrcv(ossMsgID, &ossMsg, (sizeof(struct Message)), getpid(), 0); 
		//fprintf(stderr, "msd received by user.c %d\n", index);
		if(rsvRes == -1){
			printf("USR msgsnd ERROR: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		//printf("Message received in user %d\n", index);
	
		if(loopCount >= 5){
			ossMsg.isTerm = 1;
		}else{
			address = rand() % 32768 + 0;
			requestPage = address >> 10;
			ossMsg.isTerm = 0;
		}
	
		ossMsg.mtype = 1;
		ossMsg.address = address;
		ossMsg.requestPage = requestPage;
		int sndRes = msgsnd(ossMsgID, &ossMsg, (sizeof(struct Message)), 0);
		//fprintf(stderr, "msg sent by user.c %d\n", index);
		if(sndRes == -1){
			printf("USR msgsnd ERROR: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		loopCount++;
		if(ossMsg.isTerm == 1){
			//printf("P%d is about to exit in user.c\n", ossMsg.index);
			break;
		}
	}
	
	//printf("Returning index %d\n", index);
	return index;

}

void getClock(){
	clockKey = ftok("./oss.c", 1);
	if(clockKey == -1){
		perror("ERROR IN child.c: FAILED TO GENERATE KEY FROM SHARED MEM FOR CLOCK");
		exit(EXIT_FAILURE);
	}
	clockID = shmget(clockKey, sizeof(struct Clock*), 0666 | IPC_CREAT);
	if(clockID == -1){
		perror("ERROR IN child.c: FAILED TO GET KEY FOR CLOCK");
		exit(EXIT_FAILURE);
	}
	timer = (struct Clock*)shmat(clockID, (void*) 0, 0);
	if(timer == (void*)-1){
		perror("ERROR IN child.c: FAILED TO ATTACH MEMEORY FOR CLOCK");
		exit(EXIT_FAILURE);
	}
}

void getSema(){
	semaKey = ftok("./oss.c", 2);
	if(semaKey == -1){
		perror("ERROR IN child.c: FAILED TO GENERATE KEY FROM SHARED MEM FOR SEMAPHORE");
		exit(EXIT_FAILURE);
	}
	semaID = semget(semaKey, 1, 0666 | IPC_CREAT);
	if(semaID == -1){
		perror("ERROR IN child.c: FAILED TO GET KEY FOR SEMAPHORE");
		exit(EXIT_FAILURE);
	}
}

void getPCB(){
	pcbKey = ftok("./oss.c", 3);
	if(pcbKey == -1){
		perror("ERROR IN CHILD.C: FAILED TO GENERATE KEY FOR PCB");
		exit(EXIT_FAILURE);
	}
	size_t procTableSize = sizeof(struct ProcessControlBlock) * MAX_PROC;
	pcbID = shmget(pcbKey, procTableSize, 0666 | IPC_CREAT);
	if(pcbID == -1){
		perror("ERROR IN USER.C: FAILED TO GET KEY FOR PCB");
		exit(EXIT_FAILURE);
	}
	pcb = (struct ProcessControlBlock*)shmat(pcbID, (void*) 0, 0);
	if(pcb == (void*)-1){
		perror("ERROR IN USER.C: FAILED TO ATTACH MEMEORY FOR PCB");
		exit(EXIT_FAILURE);
	}
}

void getMsg(){
	ossMsgKey = ftok("./oss.c", 4);
	usrMsgKey = ftok("./oss.c", 5);
	if(ossMsgKey == -1 || usrMsgKey == -1){
		perror("ERROR IN USER.C: FAILED TO GENERATE KEY FOR MESSAGE QUEUES");
		exit(EXIT_FAILURE);
	}
	ossMsgID = msgget(ossMsgKey, IPC_CREAT | 0600);
	usrMsgID = msgget(usrMsgKey, IPC_CREAT | 0600);
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
