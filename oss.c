/************************************************************************************
 * Name: Jackson Hoenig
 * Class: CMPSCI 4760
 * Project 3
 * Description:
 *     This program sets a shared memory clock which is a structure of and int,
 *     and a long long int. then sets up a semaphore to guard that shared memory.
 *     then, this program sets up a message queue for communication between it and
 *     its children processes. this program will spawn the maximum number of 
 *     child processes at a time and increment the shared memory clock one ns every 
 *     loop it goes through. this program also will read messages coming back from the
 *     child processes and log them in the logfile given in the options.
 *     this program will only end in one of three ways:
 *     timeout, 100 processes spawned, or 2 seconds of logical shm time passed
 *     for more information see the Readme.
 *************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#include <sys/msg.h>
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//prototype for exit safe
void exitSafe(int);

//this function is used to create the wait and signal functions.
//it populates the sembuf struct with the parameters given
//i found this code in the textbook on chapter 15
void setsembuf(struct sembuf *s, int num, int op, int flg) {
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}
//this function runs a semaphore operation "sops" on the
//semaphore with the id of semid. it will loop continuously
//through the operation until it comes back as true.
int r_semop(int semid, struct sembuf *sops, int nsops) {
	while(semop(semid, sops, nsops) == -1){
		if(errno != EINTR){
			return -1;
		}
	}
	return 0;
}
//a simple function to destroy the semaphore data.
int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

//a function to initialize the semaphone to a number in semvalue
//it is used later to set the critical resource to number to 1
//since we only have one shared memory clock.
int initElement(int semid, int semnum, int semvalue) {
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semvalue;
	return semctl(semid, semnum, SETVAL, arg);
}

//Global option values
int maxChildren = 5;
char* logFile = "log.txt";
int timeout = 3;

//get shared memory
static int shmid;
static int shmidPID;
static int semid;
static int shmidPCB;
static struct Clock *shmclock;
static struct PCB *shmpcb;
static struct Dispatch *shmpid;       
//get message queue id
int msgid;
                       
//array to hold the pid's of all the child processes
int pidArray[20];

//function is called when the alarm signal comes back.
void timerHandler(int sig){
	printf("Process has ended due to timeout\n");
	//kill the children
	int i = 0;
        for(i = 0; i < 1; i++) {
                kill(pidArray[i],SIGABRT);
        }
	printf("Killed process \n");
	exitSafe(1);
	exit(1);
}
//function to exit safely if there is an errory or anything
//it removes the IPC data.
void exitSafe(int id){
	//destroy the Semaphore memory
	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }
	//destroy msg queue
	msgctl(msgid,IPC_RMID,NULL);
	//detatch dispatch shm
	shmdt(shmpid);
	//destroy shared memory 
        shmctl(shmid,IPC_RMID,NULL);
        shmctl(shmidPCB,IPC_RMID,NULL);
        shmctl(shmidPID,IPC_RMID,NULL);

	exit(id);
}
//function is called when the number of processes spawned has reached 100
void maxProcesses(){
	printf("You have hit the maximum number of processes of 100, killing remaining processes and terminating.\n");
	int i = 0;	
	for(i = 0; i < maxChildren; i++) {
		kill(pidArray[i],SIGABRT);
		//waitpid(pidArray[i],0,0);
	}
	exitSafe(1);
}

//function to be called when the logical shm time has
//exceeded 2 seconds.
void logicalTimeout(){
	printf("2 seconds of logical time have elapsed. killing the remaining processes and terminating.\n");
	int i = 0;
        for(i = 0; i < maxChildren; i++) {
             kill(pidArray[i],SIGABRT);
        }
        exitSafe(1);
}

//structure for the dispatch of quantum and pid
struct Dispatch{
	int quantum;
	long long int pid;
};

//Structure for the Process Control Block
struct PCB{
	int CPU;
	int system;
	int burst;
	long int simPID;
	int priority;
};

//structure for the shared memory clock
struct Clock{
	unsigned int second;
	unsigned int nano;
};

//structure for the message queue
struct mesg_buffer {
	long mesg_type;
	char mesg_text[100];
} message;

int main(int argc, char **argv){
	int opt;
	//get the options from the user.
	while((opt = getopt (argc, argv, "hs:l:t:")) != -1){
		switch(opt){
			case 'h':
				printf("Usage: ./oss [-s [Number Of Max Children]]] [-l [Log File Name]] [-t [timeout in seconds]]\n");
				exit(1);
			case 's':
				maxChildren = atoi(optarg);
				if (maxChildren > 20){
					printf("-s option argument must be less than 20.");
					exit(0);
				}
				break;
			case 'l':
				logFile = optarg;
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			
			default:
				printf("Wrong Option used.\nUsage: ./oss [-s [Number Of Max Children]] [-l [Log File Name]] [-t [timeout in seconds]]\n");
				exit(1);
		}
	}
	//set the countdown until the timeout right away.
	alarm(timeout);
        signal(SIGALRM,timerHandler);
	
	//get shared memory for clock
	key_t key = ftok("./oss",45);
	if(errno){
		perror("Error: oss: Shared Memory key not obtained: ");
		exitSafe(1);
	}	
	shmid = shmget(key,1024,0666|IPC_CREAT);
	if (shmid == -1 || errno){
		perror("Error: oss: Failed to get shared memory. ");
		exitSafe(1);
	}
	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	shmclock->second = 0;
	shmclock->nano = 0;
	shmdt(shmclock);

	//get shared memory for the Process Control Table
	key_t pcbkey = ftok("./oss",'l');
	if(errno){
		perror("Error: oss: Shared Memory key not obtained: ");
                exitSafe(1);
	}
	shmidPCB = shmget(pcbkey,1024,0666|IPC_CREAT);
        if (shmidPCB == -1 || errno){
                perror("Error: oss: Failed to get shared memory. ");
                exitSafe(1);
        }
        shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
        int x;
	for(x = 0; x < 18; x++){
		shmpcb[x].CPU = 0;
		shmpcb[x].system = 0;
		shmpcb[x].burst = 0;
		shmpcb[x].simPID = 0;	
		shmpcb[x].priority = 0;
		
	}
        shmdt(shmpcb);
	
	//get the shared memory for the pid and quantum
	key_t pidkey = ftok("./oss",'m');
	if(errno){
                perror("Error: oss: Shared Memory key not obtained: ");
                exitSafe(1);
        }
        shmidPID = shmget(pidkey,16,0666|IPC_CREAT);
        if (shmidPID == -1 || errno){
                perror("Error: oss: Failed to get shared memory. ");
                exitSafe(1);
        }
        
	
	//Initialize semaphore
	//get the key for shared memory
	key_t semKey = ftok("/tmp",'j');
	if( semKey == (key_t) -1 || errno){
                perror("Error: oss: IPC error: ftok");
                exitSafe(1);
        }
	//get the semaphore id
	semid = semget(semKey, 1, PERMS);
       	if(semid == -1 || errno){
		perror("Error: oss: Failed to create a private semaphore");
		exitSafe(1);	
	}
	
	//declare semwait and semsignal
	struct sembuf semwait[1];
	struct sembuf semsignal[1];
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	if (initElement(semid, 0, 1) == -1 || errno){
		perror("Failed to init semaphore element to 1");
		if(removesem(semid) == -1){
			perror("Failed to remove failed semaphore");
		}
		return 1;
	}

	//get message queue key
	key_t msgKey = ftok("/tmp", 'k');
	if(errno){
		
		perror("Error: oss: Could not get the key for msg queue. ");
		exitSafe(1);
	}
	//set message type
	message.mesg_type = 1;
		
	//get the msgid
	msgid = msgget(msgKey, 0600 | IPC_CREAT);
	if(msgid == -1 || errno){
		perror("Error: oss: msgid could not be obtained");
		exitSafe(2);
	}
	
	//open logfile
	FILE *fp;
	remove(logFile);
	fp = fopen(logFile, "a");
	
	
	
	int i;
	unsigned int ns, sec;	
	const int maxTimeBetweenNewProcsNS = 10;
	const int maxTimeBetweenNewProcsSecs = 1;
	
	struct Clock launchTime;
	//make the initial 18 processes
	for(i = 0; i < 1; i++){

		//get launch time
		
		if (r_semop(semid, semwait, 1) == -1){
                        perror("Error: oss: Failed to lock semid. ");
			exitSafe(1);
                }
                else {
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
			
			launchTime.nano = shmclock->nano + (rand() % maxTimeBetweenNewProcsNS);
                	launchTime.second = shmclock->second + (rand() % maxTimeBetweenNewProcsSecs);
			
			shmdt(shmclock);
                        if (r_semop(semid, semsignal, 1) == -1) {
                 	       perror("Error: oss: failed to signal Semaphore. ");
                               exitSafe(1);
                        }
		}
		//loop
		bool launched = false;
		while(!launched){
			
			if (r_semop(semid, semwait, 1) == -1){
                        	perror("Error: oss: Failed to lock semid. ");
                        	exitSafe(1);
                	}
                	else {
			
                	        shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
				//check if launch time has passed
				 if(shmclock->second > launchTime.second || (shmclock->second == launchTime.second && shmclock->nano > launchTime.nano)){
                                        launched = true;
                                }
	
					
				//increment clock
        	                unsigned int increment = rand() % 1000;
        	                if(shmclock->nano >= 1000000000){
        	                        shmclock->second += (unsigned int)(shmclock->nano / 1000000000);
        	                        shmclock->nano = (shmclock->nano % 1000000000) + increment;
        	                }
        	                else{
        	                        shmclock->nano += increment;
        	                }
        	                shmclock->second += 1;
				ns = shmclock->nano;
				sec = shmclock->second;			
	
	                        shmdt(shmclock);
	                        if (r_semop(semid, semsignal, 1) == -1) {
	                                perror("Error: oss: failed to signal Semaphore. ");
	                                exitSafe(1);
	                        }
	
			}		
		}
		//generate user process
		
		//fork process
		pid_t pid = fork();
		if(pid == 0){
			char arg[20];
                        snprintf(arg, sizeof(arg), "%d", shmid);
                        char spid[20];
                        snprintf(spid, sizeof(spid), "%d", semid);
                        char msid[20];
                        snprintf(msid, sizeof(msid), "%d", msgid);
			char disId[20];
			snprintf(disId, sizeof(disId), "%d", shmidPID);
                        execl("./user","./user",arg,spid,msid,disId,NULL);
                        perror("Error: oss: exec failed. ");		
			exitSafe(1);
		}
		printf("forked child %lld\n",pid);
		fprintf(fp,"OSS: Generating process with pid %lld and putting it in queue 0 at time %d:%d\n",pid,sec,ns);
		//pcb
		if (r_semop(semid, semwait, 1) == -1){
                        perror("Error: oss: Failed to lock semid. ");
        	        exitSafe(1);
                }
	        else {
			printf("Setting up the PCB\n");
			shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
			shmpcb[i].simPID = pid;
			shmdt(shmpcb);
			pidArray[i] = pid;
			if (r_semop(semid, semsignal, 1) == -1) {
                	        perror("Error: oss: failed to signal Semaphore. ");
	        	        exitSafe(1);
                	}
		}
		//loop back	
	}
	//attach shm for the dispatch shm
	shmpid = (struct Dispatch*) shmat(shmidPID, (void*)0,0);
	
	//Now, get dispatch time
	struct Clock dispatchTime;
	dispatchTime.second = 1;
	dispatchTime.nano = 2;
while(1){
	//loop for dispatch
	bool dispatched = false;
	
	//get the pid index
	int pidIndex = 0;
	
	while(!dispatched){
		if (r_semop(semid, semwait, 1) == -1){
                        perror("Error: oss: Failed to lock semid. ");
                        exitSafe(1);
                }
		else{
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                
			if(shmclock->second > dispatchTime.second || (shmclock->second == dispatchTime.second && shmclock->nano > dispatchTime.nano)){
				dispatched = true;
                	 }
         	 
                	 unsigned int increment = rand() % 1000;
                	 if(shmclock->nano >= 1000000000){
	        	         shmclock->second += (unsigned int)(shmclock->nano / 1000000000);
                	         shmclock->nano = (shmclock->nano % 1000000000) + increment;
                	 }
                	 else{
        		         shmclock->nano += increment;
                	 }
                	 shmclock->second += 1;
                	  
			ns = shmclock->nano;
			sec = shmclock->second;              
                	if (r_semop(semid, semsignal, 1) == -1) {
                	        perror("Error: oss: failed to signal Semaphore. ");
                	        exitSafe(1);
                	}
		}
	}
	
	bool finished = false;
	
	//send signal to child via shm
	fprintf(fp,"OSS: Dispatching process with PID %lld from queue 0 at time %d:%d\n",pidArray[pidIndex],sec,ns);
	shmpid->quantum = 10;
	shmpid->pid = pidArray[pidIndex];
	//wait for child to send message back
	while(!finished){
		if (msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT) != -1){
        	   	
			printf("OSS: Receiving that process with PID %lld ran for %s nanoseconds",pidArray[pidIndex], message.mesg_text);
        		fprintf(fp,"OSS: Receiving that process with PID %lld ran for %s nanoseconds\n",pidArray[pidIndex],message.mesg_text);
        		finished = true;
			shmpid->pid = 0;	
			//generate another process
			pid_t pid = fork();
                	if(pid == 0){
                        	char arg[20];
                        	snprintf(arg, sizeof(arg), "%d", shmid);
                        	char spid[20];
                        	snprintf(spid, sizeof(spid), "%d", semid);
                        	char msid[20];
                        	snprintf(msid, sizeof(msid), "%d", msgid);
                        	char disId[20];
                        	snprintf(disId, sizeof(disId), "%d", shmidPID);
                        	execl("./user","./user",arg,spid,msid,disId,NULL);
                        	perror("Error: oss: exec failed. ");
                        	exitSafe(1);
			}
			pidArray[pidIndex] = pid;
			//parent process setup pcb
			printf("Setting up the PCB\n");
                        shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
                        shmpcb[i].simPID = pid;
                        shmdt(shmpcb);
                        pidArray[i] = pid;
                        if (r_semop(semid, semsignal, 1) == -1) {
                                perror("Error: oss: failed to signal Semaphore. ");
                                exitSafe(1);
                        }

		}
		
		else if(errno == ENOMSG){
			//received no mesage
			errno = 0;
			//increment clock
			if (r_semop(semid, semwait, 1) == -1){
	                        perror("Error: oss: Failed to lock semid. ");
	                        exitSafe(1);
	                }
	                else{
	                        shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
	
        	                 unsigned int increment = rand() % 1000;
        	                 if(shmclock->nano >= 1000000000){
        	                         shmclock->second += (unsigned int)(shmclock->nano / 1000000000);
        	                         shmclock->nano = (shmclock->nano % 1000000000) + increment;
        	                 }
        	                 else{
        	                         shmclock->nano += increment;
        	                 }
        	                 shmclock->second += 1;
	
                        	if (r_semop(semid, semsignal, 1) == -1) {
                        	        perror("Error: oss: failed to signal Semaphore. ");
                        	        exitSafe(1);
                        	}
                	}
		}
	}
}



        
              /* 
			while ((childPid = waitpid(-1, &stat, WNOHANG)) > 0){
			//the child terminated execute another
			//	printf("Child has terminated %ld\n",childPid);
				int j = 0;
				for(j = 0; j < 20; j++){
					if(pidArray[j] == childPid){
						//printf("Found pid %ld, matching %ld at index %d",childPid,pidArray[j],j);
						break;
					}
				}
				if(processes >= 100){
					maxProcesses();
				}
				pidArray[j] = fork();
				
                                if(pidArray[j] == 0){
					
                                //	printf("Forking process: %ld",(long)getpid());
                                	char arg[20];
                                	snprintf(arg, sizeof(arg), "%d", shmid);
                                	char spid[20];
                                	snprintf(spid, sizeof(spid), "%d", semid);
					char msid[20];
                		        snprintf(msid, sizeof(msid), "%d", msgid);
		                        execl("./user","./user",arg,spid,msid,NULL);
                                	perror("Error: oss: exec failed. ");
                                	exit(0);
                                }
                                else if(pidArray[j] == -1 || errno){
                        	        perror("Error: oss: second set of Fork failed");
                 	                exitSafe(1);
                                }
				//parent
				processes++;
                                


			}
		}
	        else if(errno == ENOMSG){
                	//we did not receive a message
			errno = 0;
	        }
		*/

	
	//WE WILL NEVER REACH HERE BECAUSE OF TIMER.
	perror("Error: oss: reached area that shouldn't be reached.");
	exitSafe(1);

	return 0;
}
