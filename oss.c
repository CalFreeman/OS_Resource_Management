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

#define SEMNAME "/Semfile"
#define PERMS (mode_t) (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)
#define MAX 18
#define BILLION 1000000000
#define maxTimeBetweenNewProcsNS 100000000
#define maxTimeBetweenNewProcsSecs 1
#define MAXPROCS 100

  //prototypes
void signalHandler();
void sigAlarmHandler(int sig_num);
int randomInt(int min_num, int max_num);

  // ctrl-c handler variables
int processCount = 0;
int exitFlag = 0;

  // shm pointers and ids
static system_clock* shmClockPtr;
int shmid;  // shared memory id
int msqid;


        // MAIN
int main(int argc, char *argv[]){

  int opt;
  int nflag = 18;
  int hflag = 0;
  int vflag = 0;

  while ((opt = getopt(argc, argv, "hv")) != -1)
    switch(opt){
      case 'h':
        hflag = 1;
        break;
      case 'v':
        vflag = 1;
        break;
      case '*':
        break;
      default:
        abort ();
    }

  if(hflag == 1){
    printf("Program is designed to simulate operating system resource allocation\n");
    printf(" -v for verbose mode to shut off verbose message\n");
    return EXIT_SUCCESS;
  }

  // file variables
  FILE *ofp = NULL;
  char *ofname = NULL;
  ofname = "logFile.dat";

  // variables for launching child processes
  unsigned int timeToNextProcsSecs;
  unsigned int timeToNextProcsNS;
  unsigned int launchNextProcsSecs;
  unsigned int launchNextProcsNS;

  // processes id variables
  pid_t pid;
  pid_t waitPID;
  int CHILDPIDS[18] = {0}; // DEFAULT FLAG 0 used for default CHILDPIDS[] flag
  int status;
  int totalp = 0;
  int i = 0;
  int j = 0;
  int x = 0;
  int y = 0;
  int z = 0;
  int count = 0;
  int temp = 0;
  int test = 0;

  int cflag = 0;
  int pflag = 0;
  srand(time(0));

  int index = 0; // available index of current pcb
  char pidNum[5]; // string pid for passing execl
  // msg queue struct
  struct msgbuf message;

  // opening output log file to clear it
  ofp = fopen(ofname, "w");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }
  fclose(ofp);


  // setting up logfile for appending
  ofp = fopen(ofname, "a");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }


  // master time termination
  alarm(3);


  // catch for ctrl-c signal
  signal(SIGINT, signalHandler);


  // catch alarm signal
  signal(SIGALRM, sigAlarmHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error oss: key shared memory key generation");
    return EXIT_FAILURE;
  }

  key_t skey = ftok("oss.c", 35);
  if(skey ==-1){
    perror("error oss: skey shared memeory key generation");
    return EXIT_FAILURE;
  }

  // key for msg queue
  key_t msgkey = ftok("oss.c", 25);
  if(msgkey == -1){
    perror("Error oss: message queue key generation");
    return EXIT_FAILURE;
  }
        // CREATE SHARED MEMORY
  // might not be large enough
  // shmid system_clock
  shmid = shmget(key, sizeof(system_clock), 0600 | IPC_CREAT);
  if((shmid == -1) && (errno != EEXIST)){
    perror("Error oss: shmget shmid");
    exit(EXIT_FAILURE);
  }


        // ATTACH SHARED MEMORY SEGMENTS
  // attach the segment to our data space system clock
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error oss: shmat shmClockPtr\n");
    exit(EXIT_FAILURE);
  } else {
    shmClockPtr->seconds = 0;
    shmClockPtr->nanoSeconds = 0;
  }

  // create semaphore
  unsigned int value = 1;
  sem_t *semlock = sem_open(SEMNAME, O_CREAT, 0666, 1);
  // check semaphore creation
  if(semlock == SEM_FAILED){
    printf("Error oss: semaphore creation failed\n");
    return EXIT_FAILURE;
  }

  // create nessage queue
  if ((msqid = msgget(msgkey, 0600 | IPC_CREAT)) < 0){
    perror("Error oss: msgget\n");
    return EXIT_FAILURE;
  }

  // randomize resource amounts
  for(i = 0; i < 20; i++){
    shmClockPtr->available[i] = randomInt(1,10);

    // ranomdize temp for total and avaiable instances per instance
    temp = 0;
    temp = randomInt(1, 10);
    shmClockPtr->resources[i].total_r = temp;
    shmClockPtr->resources[i].available_r = temp;

    // log it
    if(vflag == 0){
      fprintf(ofp, "oss: resource %d has %d instances\n",
      i, shmClockPtr->resources[i].total_r);
    }
    // set default resource values
    for(j = 0; j < MAX; j++){
      shmClockPtr->resources[i].claims[j] = 0;
      shmClockPtr->resources[i].requests[j] = 0;
      shmClockPtr->resources[i].allocated[j] = 0;
      shmClockPtr->resources[i].released[j] = 0;
    }

  } // end for i = 0

  // set max resources for user
  temp = randomInt(1,10);
  shmClockPtr->maxResources = temp;

  // random time for next process
  srand((unsigned)time(NULL));
  timeToNextProcsSecs = rand() % maxTimeBetweenNewProcsSecs;
  timeToNextProcsNS = rand() % maxTimeBetweenNewProcsNS;
  launchNextProcsSecs = launchNextProcsSecs + timeToNextProcsSecs;
  launchNextProcsNS = launchNextProcsNS + timeToNextProcsNS;
  if( launchNextProcsNS >= BILLION){
    launchNextProcsNS = launchNextProcsNS - BILLION;
    launchNextProcsSecs = launchNextProcsSecs + 1;
  }

  // resource allocation map
  int requests[18][20] = {0};
  int requestPIDS[18][20] = {0};
  int allocation[18][20] = {0};

  // variable flags for simulation
  int pids[20] = {0};
  int pidsC = 0;
  int spids[20] = {0};
  int rflag = 0;

  int claim = 0;

  // values for unset message
  message.mtype = 999999;

  printf("STARTING SCHEDULING PROCESS. . .\n");


         // PRIMARY LOOP
  while (exitFlag == 0){
    // termination flag for breaking outof loop
    if(exitFlag == 1){
      break;
    }
    // increment the clock using shared memory timer
    shmClockPtr->nanoSeconds += 100000;
    if(shmClockPtr->nanoSeconds >= BILLION){
      shmClockPtr->seconds++;
      shmClockPtr->nanoSeconds -= BILLION;
    }

    //printf("current: %d:%d\n", shmClockPtr->seconds, shmClockPtr->nanoSeconds);

    // check for released resources
    for(i = 0; i < 20; i++){
      for(j = 0; j < MAX; j++){
        if ( shmClockPtr->resources[i].released[j] != 0 ) {

          fprintf(ofp, "oss: process %d released %d resources\n", j, i);

            /* log release in shared mem */
          sem_wait(semlock);

          shmClockPtr->resources[i].available_r += shmClockPtr->resources[i].released[j];
          shmClockPtr->resources[i].released[j] = 0;

          sem_post(semlock);
        }
      }
    }

    // allocate resources
    for (i = 0; i < 20; i++) {
      for (j = 0; j < MAX; j++) {
        if (shmClockPtr->resources[i].requests[j] > 0) {
          claim = shmClockPtr->resources[i].claims[j] - shmClockPtr->resources[i].allocated[j];
          x = shmClockPtr->resources[i].requests[j];
          y = shmClockPtr->resources[i].allocated[j];
          y = y + shmClockPtr->resources[i].requests[j];
          z = shmClockPtr->resources[i].total_r;
          count = shmClockPtr->resources[i].available_r;

          if ( x > claim || y > z) {
            fprintf(ofp, "OSS: DEADLOCK AVOIDACE blocking Process %d for requesting resource %d \n ", j, i);

            sem_wait(semlock);
            shmClockPtr->resources[i].requests[j] = -1;
            sem_post(semlock);

          }else if (x <= count){
            sem_wait(semlock);
            //shmClockPtr->procs_counter[j] += sh_mem_ptr->resources[i].requests[j];

            if(vflag == 0){
              fprintf(ofp, "OSS: unblocking process %d and granting it resource %d \n", j, i);
            }

            shmClockPtr->resources[i].available_r -= shmClockPtr->resources[i].requests[j];
            shmClockPtr->resources[i].allocated[j] += shmClockPtr->resources[i].requests[j];
            shmClockPtr->resources[i].requests[j] = 0;
            sem_post(semlock);
          }
        }
      }
    }

    // launch new processes if necessary
    if(totalp < MAXPROCS && processCount < MAX && shmClockPtr->seconds >= launchNextProcsSecs
         && shmClockPtr->nanoSeconds >= launchNextProcsNS){
      printf("inside totalp\n");

      // flag for active child
      cflag = 1;

      // vflag
      if(vflag == 0){
        fprintf(ofp, "OSS: Child process %d launching @ %d:%d\n"
          , processCount, shmClockPtr->seconds, shmClockPtr->nanoSeconds);
      }

      // fork child process
      pid = fork();
      printf("child forked...\n");
      if(pid < 0){
        perror("ForkError: Error: failed to fork child_pid = -1");
        break;
      } else if(pid == 0){
        // converting index to string to pass to child
        sprintf(pidNum, "%d", processCount);
        //printf("passing index %d\n", processControl[index].index);
        //execl("./user", pidNum, NULL);
        execl("./user", pidNum, (char *)NULL);
        perror("Execl Error: failed to exec child from the fork of oss");
        exit(EXIT_FAILURE); //child prog failed to exec
      } else {
        // PARENT PROCESSES
        CHILDPIDS[processCount] = pid;

        // attach pid to mtype
        sprintf(message.mtext, "Process: %d", processCount);
        pidsC++;
        totalp++;
        processCount++;

        message.mtype = pid;
        printf("pid %d: \n", pid);
        // send message
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1){
          perror("Error user.c: msgsnd failed");
        } else {
          printf("oss msgsnd\n");
        }


        // random time for next process
        srand((unsigned)time(NULL));
        timeToNextProcsSecs = rand() % maxTimeBetweenNewProcsSecs;
        timeToNextProcsNS = rand() % maxTimeBetweenNewProcsNS;
        launchNextProcsSecs = launchNextProcsSecs + timeToNextProcsSecs;
        launchNextProcsNS = launchNextProcsNS + timeToNextProcsNS;

        if( launchNextProcsNS >= BILLION){
          launchNextProcsNS = launchNextProcsNS - BILLION;
          launchNextProcsSecs = launchNextProcsSecs + 1;
        }
      }
    } // end if (processCount < MAX)

    //recieve message
    if(cflag == 1){
      //reciving message from user
      if(msgrcv(msqid, &message, sizeof(message), 255255, IPC_NOWAIT) == -1){
        //printf("oss msgrcv failed\n");
      } else {
        printf("oss msgrcv\n");
      }
    }

    if(message.mtype != 999999){
      printf("parse message\n");
      // parse msg from child for resources
      int cpid, type, amount, process, resource;
      sscanf(message.mtext, "%d/%d/%d/%d/%d", &cpid, &process, &type, &resource, &amount);

      printf("process info: %d, %d, %d, %d, %d\n", cpid, process, type, resource, amount);
      if(1> 0) {
        printf("write\n");
      }else {
        printf("read\n");
        message.mtype = cpid;
        break;
      }

    printf("else....\n");
    message.mtype = 999999;
    }

    //printf("totalp & processCount: %d:%d\n", totalp, processCount);
    //printf("nextprocess: %d:%d\n", launchNextProcsSecs, launchNextProcsNS);

    if(processCount >= 17){
      for(i = 0; i<MAX; i++){
        if(CHILDPIDS[i] > 0){
          //printf("%d\n", i);
          test = waitpid(CHILDPIDS[i], &status, WNOHANG); // returns 0 immediately
          if(test == 0){
            temp = temp; //child process still running
          }else{
            if(WIFEXITED(status)){ // returns true if child terminated normally
              if(vflag == 0){
                fprintf(ofp, "OSS: Process %d terminated with status %d\n", i, status);
              }
              // free terminated processes allocated memory
              for(j = 0; j<MAX; j++){
                 allocation[i][j] = 0;
              }
              CHILDPIDS[i] = 0;
              processCount--;

            }else{
              kill(CHILDPIDS[i], SIGKILL);
              CHILDPIDS[i] = 0;
              processCount--;
              CHILDPIDS[i] = 0;
            }
          }
        }
      }
    }


  } // end while(exitFlag == 0)



        // CLEAN UP
  // wait on child procs
  //while((waitPID = wait(&status)) > 0);

  //remove msg queue
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Error oss: msgctl");
  }

  // detach shared memory
  shmdt(shmClockPtr);

  // delete shared memory
  if(shmctl(shmid, IPC_RMID, NULL) == -1){
    perror("Error oss: mshmctl shmid");
  }

  // detach then delete semaphore
  sem_close(semlock);
  sem_unlink(SEMNAME);

  // clean up
  fclose(ofp);
  return EXIT_SUCCESS;
}


        // CTRL-C EXIT HANDLER
void signalHandler() {
  exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");

  signal(SIGINT, signalHandler);
}


        // ALARM EXIT HANDLER
void sigAlarmHandler(int sig_num){
  printf("signal alarm exit.\n");
  exitFlag = 1;

  signal(SIGALRM, sigAlarmHandler);
}

// new number each call to random
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