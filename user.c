#ifndef OSS_H_
#define OSS_H_

#define MAX 18

  // resource struct
typedef struct{

  int shared_r;     // shared resources
  int total_r;      // total resources
  int available_r;  // available resources

  int claims[MAX];
  int requests[MAX];
  int allocated[MAX];
  int released[MAX];

}resource;


  // system clock
typedef struct system_clock{
  int available[20];
  unsigned int seconds;
  unsigned int nanoSeconds;

  int maxResources;
  int processCounter;
  resource resources[MAX];

} system_clock;


  //message queue
struct msgbuf{
  long mtype;
  char mtext[256];
};

#endif
[freeman@hoare7 freeman.5]$ clear
[freeman@hoare7 freeman.5]$ cat user.c
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include "oss.h"
#include <fcntl.h> /* for O_* constants */
#include <string.h>

#define SEMNAME "/Semfile"
#define PERMS (mode_t) (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)
#define MAX 18
#define BILLION 1000000000
#define maxTimeBetweenNewProcsNS 100000000
#define maxTimeBetweenNewProcsSecs 1
#define MAXPROCS 100

int randomInt();
void signalHandler();

int msqid;
int shmid, shmbid;



int main(int argc, char *argv){

  // file variables
  FILE *ofp = NULL;
  char *ofname = NULL;
  ofname = "logFile.dat";

  // open file for appending
  ofp = fopen(ofname, "a");
  if(ofp == NULL){
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }

  //printf("child/user process launched. . .\n");
  system_clock* shmClockPtr;

  // msg queue struct
  struct msgbuf message;

  // variables
  int index;

  // signal for quitting
  signal(SIGINT, signalHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error: shared memory key generation");
    return EXIT_FAILURE;
  }

  // key for msg queue
  key_t msgkey = ftok("oss.c", 25);
  if(msgkey == -1){
    perror("Error: message queue key generation");
    return EXIT_FAILURE;
  }

  sem_t *semlock = sem_open(SEMNAME, 0);

        //CREATE SHARED MEMORY
  // shmid
  shmid = shmget(key, sizeof(system_clock), 0666 | IPC_CREAT);
  if(shmid == -1){
    perror("Error user.c: shmget shmid");
    exit(EXIT_FAILURE);
  }

  // attach the segment to our data space
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error user.c: shmat shmid");
    exit(EXIT_FAILURE);
  }

  // create message queue
  if ((msqid = msgget(msgkey, 0666 | IPC_CREAT)) < 0){
    perror("Error user.c: msgget");
    return EXIT_FAILURE;
  }

  long PID = getpid();
  //message.mtype = PID;
  int availableCopy[20];
  int i = 0;
  int resourceFlag = 0;

  // get copy of avaiable resources from shared memory
  for(i = 0; i < 20; i++){
    availableCopy[i] = shmClockPtr->available[i];
  }

  // assign user resources
  int max_resource_const = shmClockPtr->maxResources;
  int resources_needed;

  for(i = 0; i < 20; i++) {
    resources_needed = randomInt(1, max_resource_const);
    if(resources_needed == max_resource_const ) {
      shmClockPtr->resources[i].claims[index] =
        1 + (randomInt(1, shmClockPtr->resources[i].total_r));

      fprintf(ofp, "process %d claimed max resources %d\n"
        , index,  shmClockPtr->resources[i].claims[index]);

    }else{
      shmClockPtr->resources[i].claims[index] =
        1 + (randomInt(1, resources_needed));

      fprintf(ofp, "process %d claimed resources %d\n"
        , index,  shmClockPtr->resources[i].claims[index]);
    }
  }

  // child process process time code
  while(1){

    //int done = 1;
    message.mtype = 255255;
    char text[256];
    int type = 0;
    int resource = 0;
    int amount  = 0;

    int release;
    int request;
    int requestConst;
    int releaseConst;

    if(msgrcv(msqid, &message, sizeof(message), getpid(), 0) == -1){
      printf("user.c: msgrcv failed\n");
    } else {
      printf("user msgrcv\n");
    }


        // PROCESS WORK BELOW
    // assigned pcb index to index var
    index = atoi(message.mtext);
    printf("atoi:-> %d\n", index);
    //index = atoi(argv[0]);
    //index = getpid();
    //printf("child process work begin\n");

    // termination chance
    srand((unsigned)time(NULL));

    /* resource handling */
    requestConst = randomInt(1,3);
    releaseConst = randomInt(1, 3);

    for(i = 0; i < 20; i++){
      if(shmClockPtr->resources[i].allocated[index] > 0){
        /* release resources*/
        if(randomInt(1, releaseConst) == 1 ) {
          release = randomInt(1, shmClockPtr->resources[i].allocated[index]);

          sem_wait(semlock);
          shmClockPtr->resources[i].released[index] += release;
          fprintf(ofp, "process %d: has released %d instances of resource %d\n"
            , index, release, i);
          sem_post(semlock);
        }
      } else if (shmClockPtr->resources[i].requests[index] == 0){
        /*request resources*/
        if(randomInt(1, requestConst) == 1 ) {
          request = randomInt(1, ( shmClockPtr->resources[i].claims[index] - shmClockPtr->resources[i].allocated[index] ));
          if ( request > 0 ) {
            sem_wait(semlock);
            shmClockPtr->resources[i].requests[index] = request;
            sem_post(semlock);
            fprintf(ofp, "process %d: requested %d instances of resource %d \n"
              , index, request, i);
          }
        }
      } else {
        /* else continue */
      }
    }


    sprintf(text, "%d/%d/%d/%d/%d", getpid(), index, type, resource, amount);
    strcpy(message.mtext, text);
    int len = strlen(message.mtext);

    /* send message */
    sem_wait(semlock);
    if (msgsnd(msqid, &message, len  + 1, 0) == -1){
      perror("Error user.c: msgsnd failed");
      return -1;
    } else {
      printf("user msgsnd\n");
    }
    sem_post(semlock);

    /* recieve message */
    if(msgrcv(msqid, &message, sizeof(message), getpid(), 0) == -1){
      printf("user.c: msgrcv failed\n");
    } else {
     printf("user msgrcv\n");
    }

    // if procses is done flag to exit
    if(1<2){
      break;
    }

  }

  //CLEAN UP
  // detached shared memeory
  shmdt(shmClockPtr);

  // detached semaphore
  sem_close(semlock);

  printf("child done\n");

  return EXIT_SUCCESS;
}


// randomInt function
int randomInt(int min_num, int max_num){
    int result = 0, low_num = 0, hi_num = 0;
    if (min_num < max_num){
        low_num = min_num;
        hi_num = max_num + 1;
    } else {
        low_num = max_num + 1;
        hi_num = min_num;
    }
    result = (rand() % (hi_num - low_num)) + low_num;
}


  // crl-c signal handler
void signalHandler() {
  //exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");
  int i;
  //for(i = 0; i < processCount; i++){
  //  kill(CHILDPIDS[i], SIGKILL);
  //}

  // destroy message queue
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Error: msgctl");
  }

  // delete shared memory
  shmctl(shmid, IPC_RMID,NULL);

  signal(SIGINT, signalHandler);

}