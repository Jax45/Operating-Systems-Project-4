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
#include "customStructs.h"
#include "semaphoreFunc.h"
#include "bitMap.h"
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)


//prototype for exit safe
void exitSafe(int);

//Global option values
int maxChildren = 10;
char* logFile = "log.txt";
int timeout = 3;

//File pointer
//FILE * fp;
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
char unsigned bitMap[3];
//function is called when the alarm signal comes back.
void timerHandler(int sig){
	printf("Process has ended due to timeout\n");
	//kill the children
	int i = 0;
	shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
	/*for(i = 0; i < 18; i++){
		bitMap[i] = pidArray[i];
	}*/
        for(i = 0; i < 18; i++) {
        	//printf("i: %d, simPid: %lld",i,shmpcb[i].simPID);
	       if(((bitMap[i/8] & (1 << (i % 8 ))) == 0) && shmpcb[i].simPID != getpid())
		pidArray[i] = shmpcb[i].simPID;
		// kill(shmpcb[i].simPID,SIGKILL);
        }
	shmdt(shmpcb);

	printf("Killed process \n");
	exitSafe(1);
}
//function to exit safely if there is an error or anything
//it removes the IPC data.
void exitSafe(int id){
	//destroy the Semaphore memory
	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }
	//shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
	//fclose(fp);
        shmdt(shmpcb);

	//destroy msg queue
	msgctl(msgid,IPC_RMID,NULL);
	//detatch dispatch shm
	shmdt(shmpid);
	//destroy shared memory 
        shmctl(shmid,IPC_RMID,NULL);
        shmctl(shmidPCB,IPC_RMID,NULL);
        shmctl(shmidPID,IPC_RMID,NULL);
	int i;
	for(i = 0; i < 18; i++){
		if((bitMap[i/8] & (1 << (i % 8 ))) != 0){	
			kill(pidArray[i],SIGKILL);		
		}	
	}
	exit(0);
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
// Return -1 if not found change last to open bit
int findOpen(int * last){
	int i = 0;
	for(i = 0; i < 18; i++){
		if(pidArray[i] == 0){
			*last = i;
			return i;
		}
	}
	return -1;

}

void incrementClock();
//Global for increment clock
struct sembuf semwait[1];
struct sembuf semsignal[1];
//setsembuf(semwait, 0, -1, 0);
//setsembuf(semsignal, 0, 1, 0);

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
				if (maxChildren > 18){
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
	size_t PCBSize = sizeof(struct PCB) * 18;
	shmidPCB = shmget(pcbkey,PCBSize,0666|IPC_CREAT);
        if (shmidPCB == -1 || errno){
                perror("Error: oss: Failed to get shared memory. ");
                exitSafe(1);
        }
        shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
        int x;
	for(x = 0; x < 18; x++){
		shmpcb[x].launch.second = 0;
		shmpcb[x].launch.nano = 0;
		shmpcb[x].dispatch.second = 0;
		shmpcb[x].dispatch.nano = 0;
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
	//struct sembuf semwait[1];
	//struct sembuf semsignal[1];
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

	
//*******************************************************************
//
//
//
//******************************************************************	
	//open logfile
	FILE *fp;	
	remove(logFile);
	fp = fopen(logFile, "a");
	
	//Create the queues
	struct Queue* priorityZero = createQueue();
	struct Queue* priorityOne = createQueue();
	struct Queue* priorityTwo = createQueue();
	struct Queue* someQ = createQueue();
	//int que[18] = 	
	
//int i;
	const int maxTimeBetweenNewProcsNS = 10;
	const int maxTimeBetweenNewProcsSecs = 2;
	shmpid = (struct Dispatch*) shmat(shmidPID,(void*)0,0);
	struct Clock launchTime;
	launchTime.second = 0;
	launchTime.nano = 0;
	//struct Clock currentTime;
	//make the initial 18 processes
	//for(i = 0; i < 1; i++){
	int lastPid = -1;
	//int j;
	//for(j = 0; j < 1; j++){
	while(1){
		
		if(launchTime.second == 0 && launchTime.nano == 0){
			//printf("Finding a launch time\n");
			fprintf(fp,"OSS: generating a new launch time.\n");
			//Create new launch time
			if (r_semop(semid, semwait, 1) == -1){
                                        perror("Error: oss: Failed to lock semid. ");
                                        exitSafe(1);
                        }
                        else{
				shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                                launchTime.second = shmclock->second + (rand() % maxTimeBetweenNewProcsSecs);
                                launchTime.nano = shmclock->nano + (rand() % maxTimeBetweenNewProcsNS);
				shmdt(shmclock);
					
				if (r_semop(semid, semsignal, 1) == -1) {
                        	       perror("Error: oss: failed to signal Semaphore. ");
                        	       exitSafe(1);
                        	}
			}
				

			//printf("Launch time: %d:%d",launchTime.second,launchTime.nano);
		}
		//check if we need to launch the processes
		//printf("Outside the for loop\n");
			
		//if(findOpen(&lastPidIndex) != -1){
//					if((lastPid = openBit(bitMap,lastPid)) != -1){
		if (r_semop(semid, semwait, 1) == -1){
                       perror("Error: oss: Failed to lock semid. ");
                       exitSafe(1);
                }
                else{	
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
			if(shmclock->second > launchTime.second || (shmclock->second == launchTime.second && shmclock->nano > launchTime.nano)){
				//int xy = 0;
				if((lastPid = openBit(bitMap,lastPid)) != -1){
					launchTime.second = 0;
					launchTime.nano = 0;
					//launch the process
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
						char pcbID[20];
						snprintf(pcbID, sizeof(pcbID), "%d", shmidPCB);
                                		execl("./user","./user",arg,spid,msid,disId,pcbID,NULL);
                                		perror("Error: oss: exec failed. ");
                                		//fclose(fp);
						exitSafe(1);
                        		}
					else if(pid == -1){
						perror("OSS: Failed to fork.");
						exitSafe(2);
					}
                        		//printf("forked child %d\n",pid);
                        		fprintf(fp,"OSS: Generating process with pid %d and putting it in queue 0 at time %d:%d\n",pid,shmclock->second,shmclock->nano);
					fflush(fp);	
					printf("place in queue %d\n",lastPid);
					//place in queue
					enQueue(priorityZero,lastPid);
                                        //enQueue(priorityZero,lastPidIndex);

					printf("Queue size: %d \n",sizeOfQueue(priorityZero));
					
                       		        //printf("Setting up the PCB\n");
                       		        shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
                       		        shmpcb[lastPid].simPID = pid;
//Add start time
//							
					setBit(bitMap,lastPid);
					//pidArray[lastPidIndex] = 1;
					//bitMap[i] = 1;
					//shmpcb[i].dispatch.second = currentTime.second + 1;
                        	    	//shmpcb[i].dispatch.nano = currentTime.nano + 1;
					shmdt(shmclock);
                        	        shmdt(shmpcb);
				}
			}				
			if (r_semop(semid, semsignal, 1) == -1) {
                        	perror("Error: oss: failed to signal Semaphore. ");
                               	exitSafe(1);
                	}        
		        
		}
		//dispatch the ready process
		//check queue 0
		//struct Queue* chosenQueue = priorityZero;
		int size = sizeOfQueue(priorityZero);
                //int size1 = sizeOfQueue(priorityOne);
                //int size2 = sizeOfQueue(priorityTwo);
		//int size = size0;
 		// Here we need to loop through queu
		struct Node* n = deQueue(priorityZero);

//		if(n == NULL){
//			n = deQueue(priorityOne);
		//	chosenQueue = priorityOne;
//		}
//		if(n == NULL){
//			n = deQueue(priorityTwo);
		//	chosenQueue = priorityTwo;
//		}
		
		int queueFlag = 0;
		//int size0 = sizeOfQueue(priorityZero);
                //int size1 = sizeOfQueue(priorityOne);
                //int size2 = sizeOfQueue(priorityTwo);
	       	//printf("size of queue0 : %d\n",size0);
		while(size != 0){
			printf("size of queue0 : %d\n",size); 
			if (r_semop(semid, semwait, 1) == -1){
                	        perror("Error: oss: Failed to lock semid. ");
                	        exitSafe(1);
       	        	}
                    
                     	shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
                     	shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                     	printf("n->key = %d\n",n->key);
		     	fflush(stdout);
		     	int k = n->key;
		     	//n = n->next;  
			//dispatch that process
		       	printf("OSS: Dispatching process with PID %lld from queue 0 at time %d:%d\n",shmpcb[k].simPID,shmclock->second,shmclock->nano);
		       	fprintf(fp,"OSS: Dispatching process with PID %lld from queue 0 at Index %d at time %d:%d\n",shmpcb[k].simPID,k,shmclock->second,shmclock->nano);
		       	fflush(fp);
			pid_t tempPid = shmpcb[k].simPID;	
                       	shmdt(shmpcb);
			shmdt(shmclock);
			//release semaphore
		   	if (r_semop(semid, semsignal, 1) == -1) {
        	       		    perror("Error: oss: failed to signal Semaphore. ");
	                           exitSafe(1);
     		   	}
				
			message.mesg_type = 1;
			message.pid = tempPid;
				
			sprintf(message.mesg_text,"Test");
			r_semop(semid, semwait, 1);	
			shmpid->index = k;
		       	shmpid->quantum = rand() % 10 + 1;
                       	shmpid->pid = tempPid;
			r_semop(semid, semsignal, 1);
			//msgsnd(msgid, &message, sizeof(message), 0);
			//wait for child to send message back
                	bool msgREC = false;
			while(!msgREC){
				message.mesg_type = 2;	
              			if (msgrcv(msgid, &message, sizeof(message), 2, IPC_NOWAIT) != -1){
					printf("Message: %s,%d,%d\n",message.mesg_text,message.mesg_type,message.pid);
			        	msgREC = true;
					shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
	                        	printf("OSS: Receiving that process with PID %lld ran for %d nanoseconds\n",tempPid, shmpcb[k].duration);
	                        	fprintf(fp,"OSS: Receiving that process with PID %lld ran for %d nanoseconds\n",tempPid,shmpcb[k].duration);
	                        	  
					//if the process is done wait and do not enqueue
					if(message.pid == getpid()){
						printf("lower process is done");
						fprintf(fp, "OSS: process with PID %lld has terminated and had %lld CPU time\n",shmpcb[k].simPID, shmpcb[k].CPU);			
						//Wait on the terminated process
						int status = 0;
					        if(waitpid(tempPid, &status, 0) == -1){
					            perror("OSS: Waiting on pid failed");
						    exitSafe(1);
					        }			
					   	shmpid->pid = 0;
	                        	   	resetBit(bitMap,k);
		
						pidArray[k] = 0;
						shmpcb[k].launch.second = 0;
        	        			shmpcb[k].launch.nano = 0;
        	        			shmpcb[k].dispatch.second = 0;
       	        				shmpcb[k].dispatch.nano = 0;
        	        			shmpcb[k].CPU = 0;
                				shmpcb[k].system = 0;
                				shmpcb[k].burst = 0;
                				shmpcb[k].simPID = 0;
					        shmpcb[k].priority = 0;
						
						size--;
					}
					else{
						//process not done yet
						fprintf(fp,"Process %lld is still running already having used up %d CPU time\n",shmpcb[k].simPID, shmpcb[k].CPU);
						fflush(fp);
						//place back in same queue
						enQueue(priorityZero,k);
	                                        printf("Re enqueued k=%d\n",k);
						//enQueue(someQ,k);

						size--;
						if(size > 0){
							//printf("Dequeued %d\n",n->key);
							n = deQueue(priorityZero);
							printf("Dequeued %d\n",n->key);
							//n = deQueue(someQ);
						}
						printf("size is: %d\n",size);	
					}
					shmdt(shmpcb);
				}	
				else if(errno){
			 	   errno = 0;
				    //increment clock
				    incrementClock();
				}	
			}	    
		}
		//increment the clock
		incrementClock();
	}
	return 0;
}

void incrementClock(){

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
                        /*add a second!*/
                        shmclock->second += 1;
                        //currentTime.nano = shmclock->nano;
                        //currentTime.second = shmclock->second;
                        /*fprintf(fp,"Incremented the clock %d:%d\n",shmclock->second,shmclock->nano);*/
                        shmdt(shmclock);
                        if (r_semop(semid, semsignal, 1) == -1) {
                                perror("Error: oss: failed to signal Semaphore. ");
                                exitSafe(1);
                        }
               }
}
