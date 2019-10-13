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
	int shmidPID = atoi(argv[4]);
	
        if(semid == -1){
                perror("Error: user: Failed to create a private semaphore");
                exit(1);
        }	
	
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	
	//wait for shm to show us dispatched
	struct Dispatch *dispatch = (struct Dispatch*) shmat(shmidPID,(void*)0,0);
	while(dispatch->pid != getpid()){
		sleep(1);
	}
	//get quantum from shm
	unsigned int quantum = dispatch->quantum;

	//will we use entire quantum?
	
	if ( rand() % 2 ){
		//use part of quantum
		quantum = rand() % quantum;
	}
	
	
	unsigned int ns = quantum;
	unsigned int sec;	
	if ((error = r_semop(semid, semwait, 1)) == -1){
		perror("Error: user: Child failed to lock semid. ");
		exit(1);
	}
	else if (!error) {
		//inside critical section
	        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
		ns += shmclock->nano;
		sec = shmclock->second;
	      
	        shmdt(shmclock);
		printf("got the clock\n");	
	
		//exit the Critical Section
		if ((error = r_semop(semid, semsignal, 1)) == -1) {
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
		if ((error = r_semop(semid, semwait, 1)) == -1){
			perror("Error: user: Child failed to lock semid. ");
	                return 1;
	        }
	        else if (!error) {
		
			//inside CS
	                struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
			if((shmclock->nano >= ns && shmclock->second == sec) || shmclock->second > sec){
				timeElapsed = true;
			}
			shmdt(shmclock);

			//exit CS
	                if ((error = r_semop(semid, semsignal, 1)) == -1) {
	                        printf("Failed to clean up");
       		                return 1;
                	}

		}
	}
	//send message
	int msgid = atoi(argv[3]);
        message.mesg_type = 1;
        //send back what time you calculated to end on.
	sprintf(message.mesg_text, "quantum: %d, %d",quantum, ns);
        msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
	printf("Exiting Process\n");
	exit(0);
	return 0;
}	
