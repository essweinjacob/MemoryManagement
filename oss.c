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
int getNSec();
int getSec();
bool checkReady();
void iniFT();

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

// Bitmap
static unsigned char bitmap[MAX_PROC];

// Variables for fork timer
bool spawnReady = true;
unsigned int lastForkSec = 0;
unsigned int lastForkNSec = 0;

// Frame Table Block
struct FrameTableBlock FT[256];
int framePos = 0;

int main(int argc, char *argv[]){
	// Set up real time 2 second clock
	struct itimerval time1;
	time1.it_value.tv_sec = 2;
	time1.it_value.tv_usec = 0;
	time1.it_interval = time1.it_value;
	signal(SIGALRM, god);
	setitimer(ITIMER_REAL, &time1, NULL);

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

	// Create bitmap and zero elements
	memset(bitmap, '\0', sizeof(bitmap));

	// Timer
	timer->sec = 0;
	timer->nsec = 0;
	
	// Forking variables
	pid_t pid = -1;
	int activeChild = 0;

	
	pid = fork();
	if(pid < 0){
		perror("OSS ERROR: FAILED TO FORK");
		god(1);
		exit(EXIT_FAILURE);
	}
	if(pid == 0){
		char convIndex[1024];
		sprintf(convIndex, "%d", activeChild);
		char *args[] = {"./user", convIndex, NULL};
		int exeStatus = execvp(args[0], args);
		if(exeStatus == -1){
			perror("OSS ERROR: FAILED OT LAUNCH USER PROCESS");
			god(1);
			exit(EXIT_FAILURE);
		}
	}else{
		
		activeChild++;
	}

	ossMsg.mtype = ossMsg.pid = pid;
	ossMsg.index = activeChild;
	msgsnd(ossMsgID, &ossMsg, (sizeof(struct Message) - sizeof(long)), 0);
	printf("Sending message to user process\n");

	msgrcv(ossMsgID, &ossMsg, (sizeof(struct Message) - sizeof(long)), 1, 0);
	printf("OSS: P%d requesting read of address %d at time %3d:%3d\n", activeChild, ossMsg.address, getNSec(), getSec());
	printf("OSS: Address %d in frame %d, giving data to P%d at time %3d:%3d\n", ossMsg.address, framePos, activeChild, getNSec(), getSec());
	//printf("OSS msgrcv error: %s\n", strerror(errno));
	//printf("new index of message is: %d\n", ossMsg.index);

	//Wait for child to die
	while(1){
		int status;
		pid_t childPid = waitpid(-1, &status, WNOHANG);
		if(childPid > 0){
			printf("Child has exited\n");
			break;
		}
	}

	printf("Program has finished\n");
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
void incTimer(int amount){
	semLock();
	
	timer->nsec += amount;
	while(timer->nsec >= 1000000000){
		timer->sec++;
		timer->nsec -= 1000000000;
		// DEBUG printf("Stuck in timer?\n");
	}

	semRelease();
}

int getNSec(){
	semLock();
	int nano = timer->nsec;
	semRelease();
	return nano;
}

int getSec(){
	semLock();
	int sec = timer->sec;
	semRelease();
	return sec;
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

bool checkReady(){
	int randTime = (rand() % (500000 - 1000 + 1)) + 1000;
	if(((lastForkSec * 1000000000 + lastForkNSec) - (getSec() * 1000000000 + getNSec()))>= 500){
		spawnReady = true;
	}
}

void iniFT(){
	int i;
	for(i = 0; i < 256; i++){
		FT[i].occupied = 0;
		FT[i].refByte = -1;
		FT[i].dirtyBit = -1;
	}
}
