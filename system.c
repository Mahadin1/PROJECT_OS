#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<time.h>
#include<semaphore.h>
#include<time.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>

#define MAXSTUDENTS 5
#define MAXEVALUATORLIMIT 2
#define MAXEVALUATOR 4
#define EXAMTIMELIMIT 3 
#define MAXSTUDENTEXAMTIME 7

// structures
typedef struct {
int studentID;
double score;
} Student;

typedef struct{
  int studentID;
  double score;
  int graded;
  int timeOut;
  double timeTaken;
} Result;


//Shared Values
pthread_mutex_t resultLock;
pthread_mutex_t submissionLock;
pthread_mutex_t examLock;
pthread_cond_t submissionCondition;
sem_t gradingLimit;
Result studentResults[MAXSTUDENTS];
int resultCount = 0;
pthread_mutex_t resultLock;
Result submissionQueue[MAXSTUDENTS];
int submissionCount = 0;
int examOver = 0;
int logFd;
pthread_mutex_t logLock;

// student thread
void* std_thread(void* arg){

  Student s = *(Student *)arg;
  char msg[100];
  time_t startTime  = time(NULL);
  sleep(rand() % MAXSTUDENTEXAMTIME);
  time_t endTime = time(NULL);
  double examtimetaken = difftime(endTime,startTime);
  pthread_mutex_lock(&submissionLock);
  submissionQueue[submissionCount].studentID = s.studentID;
  submissionQueue[submissionCount].graded = 0;
  submissionQueue[submissionCount].timeTaken = examtimetaken;
  if(examtimetaken > EXAMTIMELIMIT){
    submissionQueue[submissionCount].timeOut = 1;
  }

  return NULL;
}

int main(){
  srand(time(NULL));
  printf("System is Starting\n");
// Initlializing all the data
  pthread_mutex_init(&resultLock,NULL);


  Student s[MAXSTUDENTS];
  pthread_t std_t[MAXSTUDENTS];


// Student working in main
  for(int i =0 ;i<MAXSTUDENTS;i++){
    s[i].studentID = i;
    s[i].score = ((rand() % 10000) / 100.00);
  }

  for(int i =0;i<MAXSTUDENTS;i++){
    pthread_create(&std_t[i],NULL,std_thread,&s[i]);
  }


  for(int i =0;i<MAXSTUDENTS;i++){
    pthread_join(std_t[i],NULL);
  }
  printf("\n--- Submitted Results ---\n");
    for(int i = 0; i < resultCount; i++){
      printf("Student %d | Score: %.2f | Graded: %d\n",
        studentResults[i].studentID,
        studentResults[i].score,
        studentResults[i].graded);
    }


  pthread_mutex_destroy(&resultLock);
  return 0;
}
