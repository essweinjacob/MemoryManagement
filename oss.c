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
void incTimer(int amount);
int getNSec();
int getSec();
bool checkReady();
void iniFT();
void iniPCB();

// Global Variables
// Pid list info
int *listOfPIDS;
int numOfPIDS;

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

// File
FILE *fp;

// Variables for fork timer
bool spawnReady = true;
unsigned int lastForkSec = 0;
unsigned int lastForkNSec = 0;

// Frame Table Block
struct FrameTableBlock FT[256];
int oldestFrame = -1;

int main(int argc, char *argv[]){
	/*
	// Set up real time 2 second clock
	struct itimerval time1;
	time1.it_value.tv_sec = 2;
	time1.it_value.tv_usec = 0;
	time1.it_interval = time1.it_value;
	signal(SIGALRM, god);
	setitimer(ITIMER_REAL, &time1, NULL);
	*/

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

	// Initialize tables
	iniFT();
	//iniPCB();

	// Timer
	timer->sec = 0;
	timer->nsec = 0;
	
	// PID array
	listOfPIDS = calloc(100, (sizeof(int)));

	// Forking variables
	pid_t pid = -1;
	int activeChild = 0;
	int forkIndex = 0;
	int childLaunched = 0;
	int childExit = 0;

	while(1){
		int i;
		for(i = 0; i < 18; i++){
			if(pcb[i].isActive == 1){
				//printf("pcb[%d].isActive = %d\n", i, pcb[i].isActive);
				ossMsg.mtype = ossMsg.pid = pcb[i].pid;
				ossMsg.index = pcb[i].index;
				int sndRes = msgsnd(ossMsgID, &ossMsg, (sizeof(struct Message)), 0);
				fprintf(stderr, "msg send in oss %d\n", i);
				if(sndRes == -1){
					printf("OSS msgsnd ERROR: %s\n", strerror(errno));
					god(1);
					exit(EXIT_FAILURE);
				}
				struct msqid_ds msgds;
				msgctl(ossMsgID, IPC_STAT, &msgds);
				fprintf(stderr, "Number of messages: %d\n", msgds.msg_qnum);
				int rcvRes = msgrcv(ossMsgID, &ossMsg, (sizeof(struct Message)), 1, 0);
				//fprintf(stderr, "msg recieve in oss %d\n", i);
				if(rcvRes == -1){
					printf("OSS msgrcv ERROR: %s\n", strerror(errno));
					god(1);
					exit(EXIT_FAILURE);
				}
				if(ossMsg.isTerm){
					pcb[i].isActive = 0;
				}else{
					int address = ossMsg.address;
					//printf("pcb[%d].pageNumber[%d].isValid = %d\n", i, address/1000, pcb[i].pageTable[address/1000].isValid);

					// Access pages
					int pageNum = address / 1000;

					// See what type of access the process has to this page
					if(pcb[i].pageTable[pageNum].access == 0){
						fp = fopen("tablelog", "a");
						fprintf(fp, "OSS: P%d requsting read of address %d at time %d:%d\n", i, address, getSec(), getNSec());
						fclose(fp);
					}else{
						fp = fopen("tablelog", "a");
						fprintf(fp, "OSS: P%d requsting write of address %d at time %d:%d\n", i, address, getSec(), getNSec());
						fclose(fp);
					}
				
					// See if page already exists in processes memory
					if(pcb[i].pageTable[pageNum].isValid){
						// Page exists
						incTimer(10);
						if(pcb[i].pageTable[pageNum].access == 0){
							fp = fopen("tablelog", "a");
							fprintf(fp, "OSS: Indicating to P%d that read has happened to address %d at time %d:%d\n", i, address, getSec(), getNSec());
							fclose(fp);
						}else{
							fp = fopen("tablelog", "a");
							fprintf(fp, "OSS: Indicating to P%d that write has happened to address %d at time %d:%d\n", i, address, getSec(), getNSec());
							fclose(fp);
						}
						FT[pcb[i].pageTable[pageNum].frame].refByte = 1;
						//pcb[i].pageTable[pageNum].isValid = false;

					}else{
						// Page doesnt exist page fault
						incTimer(14);
						fp = fopen("tablelog", "a");
						fprintf(fp, "OSS: Address %d of P%d is not in a frame, dirty bit\n", address, i);
						fclose(fp);
						//pcb[i].pageTable[pageNum].isValid = true;
					}

				}
			incTimer(0);
			}
		}	
		if(forkIndex >= 18){
			forkIndex = 0;
		}
		if(activeChild <= 17 && pcb[forkIndex].isActive == 0 && childExit < 100 && childLaunched < 100){
			pid = fork();
			if(pid < 0){
				perror("OSS ERROR: FAILED TO FORK");
				printf("Active Child = %d\n", activeChild);
				god(1);
				exit(EXIT_FAILURE);
			}
			if(pid == 0){
				char convIndex[1024];
				sprintf(convIndex, "%d", forkIndex);
				//char *args[] = {"./user", "./user", convIndex, NULL};
				int exeStatus = execl("./user", "./user", convIndex, NULL);
				if(exeStatus == -1){
					perror("OSS ERROR: FAILED OT LAUNCH USER PROCESS");
					god(1);
					exit(EXIT_FAILURE);
				}
			}else{
				fp = fopen("tablelog", "a");
				fprintf(fp, "P%d has launched\n", forkIndex);
				printf("P%d HAS LAUNCHED\n", forkIndex);
				fclose(fp);
				pcb[forkIndex].isActive = 1;
				pcb[forkIndex].index = forkIndex;
				pcb[forkIndex].pid = listOfPIDS[numOfPIDS] = pid;
				numOfPIDS++;
				childLaunched++;
				activeChild++;
				//spawnReady = false;

			}
		}
		else{/*
			printf("What is broken?\n");
			printf("Active Child = %d\n", activeChild);
			printf("Fork index = %d\n", forkIndex);
			printf("PCB[forkIndex].isActive = %d\n", pcb[forkIndex].isActive);
			printf("childExit = %d\n", childExit);
			printf("childLaunched = %d\n", childLaunched);
			printf("spawnReady = %d\n", spawnReady);
			*/
		}

		int status;
		if((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0){
			//printf("Past waitpid\n");
			//printf("OSS waitpid status return: %s\n", strerror(errno)); 
			if(WIFEXITED(status)){
				int exitStatus = WEXITSTATUS(status);
				//printf("OSS WEXITSTATUS return: %s\n", strerror(errno));
				//fp = fopen("tablelog", "a");
				//fprintf(stderr, "P%d has exited correctly\n", exitStatus);
				printf("P%d has exited\n", exitStatus);
				//fclose(fp);
				activeChild--;
				childExit++;
			}
		}

		if(activeChild > 18){
			god(1);
		}

		if(childExit >= 100 && activeChild <= 0){
			break;
		}
		forkIndex++;
		incTimer(0);
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
}

void god(int signal){
	printf("Waking god up\n");
	freeMem();
	int i;
	for(i = 0; i < MAX_PROC; i++){
		kill(listOfPIDS[i], SIGTERM);
	}
	printf("GOD HAS BEEN CALLED AND THE RAPTURE HAS BEGUN. SOON THERE WILL BE NOTHING\n");
	free(listOfPIDS);
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
	if(amount == 0){
		int randTime = rand() % 1000 + 1;
		timer->nsec += randTime;
	}else{	
		timer->nsec += amount;
	}
	while(timer->nsec >= 1000000000){
		timer->sec++;
		timer->nsec -= 1000000000;
		// DEBUG printf("Stuck in timer?\n");
	}
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
		FT[i].refByte = 0;
		FT[i].dirtyBit = 0;
	}
}

/*
void iniPCB(){
	srand(time(NULL));
	int i;
	for(i = 0; i < 18; i++){
		pcb[i].index = -1;
		pcb[i].pid = -1;
		pcb[i].isActive = 0;
		int j;
		for(j = 0; j < 32; j++){
			pcb[i].pageTable[j].frame = -1;
			pcb[i].pageTable[j].access = rand() % 2;
			pcb[i].pageTable[j].isValid = false;
		}
	}
}
*/
