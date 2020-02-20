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