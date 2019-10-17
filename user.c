//Name: Jackson Hoenig
//Class: CMP-SCI 4760
//Description: This is the child process of the parent program oss.c
//please check that one for details on the whole project. this program is not
//designed to be called on its own.
//This program takes in 3 arguments, the shared memory id, semaphore id, and the message queue id.
//witht that data the program tries to access the shared memory through the semaphore given.
//when it gets the SHM, it checks the clock and calculates a random time to add to the ns that it will end
//then it releases shared memory and checks until that time has passed on that shm clock.
//then sends the data in a message queue to the parent process. and terminates.

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/msg.h>
#include <stdbool.h>
#include "customStructs.h"
#include "semaphoreFunc.h"
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//signal handler
void signalHandler(int sig){
	exit(1);
}

int main(int argc, char  **argv) {
	srand(getpid());
	signal(SIGABRT,signalHandler);	
	
	struct sembuf semsignal[1];
	struct sembuf semwait[1];
	int error;	
	//printf("Inside process: %ld\nsemid after: %d\n", (long)getpid(),atoi(argv[2]));
	//get semaphore id	
	int shmid = atoi(argv[1]);	
	int semid = atoi(argv[2]);
	int msgid = atoi(argv[3]);
	int shmidPID = atoi(argv[4]);
	int shmidpcb = atoi(argv[5]);	
        if(semid == -1){
                perror("Error: user: Failed to create a private semaphore");
                exit(1);
        }	
	
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	printf("Before the while loop\n");
    while(1){	
	message.mesg_type = 1;	
	pid_t myPid = getpid();
	//struct mesg_buffer Rcv;
	message.mesg_type = 1;
	while(1){
	    int result = msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT);
		if(result != -1){
		    printf("message pid = %s, %lld my pid = %lld",message.mesg_text,message.pid,myPid);	
		    if (message.pid == myPid){	
	                break;
		    }
	        }
		errno = 0;
	}
	
	//wait for shm to show us dispatched
	//msgrcv(msgid, &message, sizeof(message), getpid(), 0);

	//struct Dispatch *dispatch = (struct Dispatch*) shmat(shmidPID,(void*)0,0);	
	/*while(dispatch->pid != getpid()){
		sleep(1);
		printf("here is the current dispatch pid %ld and my pid %ld",dispatch->pid,getpid());
	}
	*/
	//dispatch->flag = 1;
	//printf("WE GOT PAST THE SLEEP NOW SENDING MESSAGE\n\n\n");
	//get random number
	int purpose = rand() % 3;
	//purpose = 1;
	if (purpose == 0){
		//terminate
		message.pid = getppid();
                message.mesg_type = 2;
                /*send back what time you calculated to end on.*/
                sprintf(message.mesg_text, "Done");
                msgsnd(msgid, &message, sizeof(message), 0);
                perror("USER: ");
		printf("Exiting Process\n");
                exit(0);
	}	
	else if(purpose == 2){
		//Wait for event
		
	}
	//else = 1
	
	//get quantum from shm
	struct Dispatch *dispatch = (struct Dispatch*) shmat(shmidPID,(void*)0,0);
	unsigned int quantum = dispatch->quantum;
	int bitIndex = dispatch->index;
	printf("bit index %d, quantum %d",bitIndex,quantum);
	//dispatch->pid = 0;
	//will we use entire quantum?
	shmdt(dispatch);
	if ( rand() % 2 ){
		//use part of quantum
		quantum = rand() % quantum;
	}
	unsigned long long int CPU;
	unsigned long long int duration; 	
	unsigned long long int currentTime;	
	unsigned long long  burst;
	unsigned int ns = quantum;
	unsigned int sec;
	unsigned long long startTime;
	perror("USER CHECK: ");
	if (r_semop(semid, semwait, 1) == -1){
                 perror("Error: oss: Failed to lock semid. ");
                 exit(1);
        }
	/*if (r_semop(semid, semwait, 1) == -1){
		perror("Error: user: Child failed to lock semid. ");
		exit(1);
	}*/
	else{
		//inside critical section
	        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
		ns += shmclock->nano;
		sec = shmclock->second;
	 	startTime = (1000000000 * shmclock->second) + shmclock->nano; 
	        shmdt(shmclock);
		printf("got the clock\n");	
		struct PCB *shmpcb = (struct PCB*) shmat(shmidpcb, (void*)0,0);
		burst = shmpcb[bitIndex].burst;
		shmdt(shmpcb);	
		//exit the Critical Section
		if ( r_semop(semid, semsignal, 1) == -1) {
			perror("User: Failed to clean up semaphore");
			return 1;
		}
	
	}
	
	
	
	//Make sure we convert the nanoseconds to seconds if big enough
	if( ns >= 1000000000){
		sec += ns % 1000000000;
		ns = ns / 1000000000;
	}
//	printf("Waiting for logical time: %d:%lld",sec,ns);
	

	bool timeElapsed = false;
	while(!timeElapsed){
		if (error = r_semop(semid, semwait, 1) == -1){
			perror("Error: user: Child failed to lock semid. ");
	                return 1;
	        }
	        else {
		
			//inside CS
	                struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
			if((shmclock->nano >= ns && shmclock->second == sec) || shmclock->second > sec){
				timeElapsed = true;
				currentTime = (1000000000 * shmclock->second) + shmclock->nano;
				
				struct PCB *shmpcb = (struct PCB*) shmat(shmidpcb, (void*)0,0);
				if (shmpcb == (struct PCB*)(-1)){

					perror("USER: PCB SHMAT");
					exit(1);

				}
				shmpcb[bitIndex].duration = (currentTime - startTime);
				shmpcb[bitIndex].CPU += shmpcb[bitIndex].duration;
				duration = shmpcb[bitIndex].duration;
				CPU = shmpcb[bitIndex].CPU;
				
				shmdt(shmpcb);	
				
			}
			shmdt(shmclock);

			//exit CS
	                if ((error = r_semop(semid, semsignal, 1)) == -1) {
	                        printf("Failed to clean up");
       		                return 1;
                	}

		}
	}
	
/*	if(CPU >= burst){
		//message.mesg_text = "Done";

		//send message
		message.pid = getppid();
        	message.mesg_type = 1;
		sprintf(message.mesg_text, "Done");
        	msgsnd(msgid, &message, sizeof(message), 0);
		printf("Exiting Process\n");
		exit(0);
	}
	else{*/
		//message.mesg_text = "Not Done";
		message.pid = 0;//getppid();
		message.mesg_type = 2;
		sprintf(message.mesg_text, "Not done yet.");
		msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
			
	//}
    }
	return 0;
}	
