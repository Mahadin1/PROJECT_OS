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
void* eva_thread(void* arg){
  Evaluator e = *(Evaluator *)arg;
  int over;
  char msg[100];
  while(1){
    pthread_mutex_lock(&submissionLock);
    while(examOver == 0 && submissionCount == 0){ 
    pthread_cond_wait(&submissionCondition,&submissionLock);
    }
    if(submissionCount ==0 && examOver == 1){
    pthread_mutex_unlock(&submissionLock);
    break;
    } 
    submissionCount--;
    Result r = submissionQueue[submissionCount];
    pthread_mutex_unlock(&submissionLock);
    
    snprintf(msg,sizeof(msg),"Evaluator %d is grading student %d\n",e.evaluatorID,r.studentID);
    logEvent(msg);
    if(r.timeOut == 1){
      pthread_mutex_lock(&resultLock);
      studentResults[resultCount].graded = 1;
      studentResults[resultCount].studentID = r.studentID;
      studentResults[resultCount].score = 0;
      studentResults[resultCount].timeOut = r.timeOut;
      studentResults[resultCount].timeTaken = r.timeTaken;
      resultCount++;
      pthread_mutex_unlock(&resultLock);
      snprintf(msg,sizeof(msg),"Evalator %d did not evalute student (TIMEOUT) %d",e.evaluatorID,r.studentID);
      logEvent(msg);
    }else{
    sem_wait(&gradingLimit);
  sleep(2);
  pthread_mutex_lock(&resultLock);
  studentResults[resultCount].graded = 1;
  studentResults[resultCount].studentID = r.studentID;
  studentResults[resultCount].score = r.score;
  studentResults[resultCount].timeOut = r.timeOut;
  studentResults[resultCount].timeTaken = r.timeTaken;
  // studentResults[e.studentID].score = (rand() % 100); 
  resultCount++;
  snprintf(msg,sizeof(msg),"Evalator %d evaluated student %d",e.evaluatorID,r.studentID);
  logEvent(msg);
  pthread_mutex_unlock(&resultLock);
  sem_post(&gradingLimit);
}
}
  return NULL;
}
int main(){
  srand(time(NULL));
  printf("System is Starting\n");
  pthread_mutex_init(&examLock,NULL);
  pthread_mutex_init(&resultLock,NULL);
  pthread_mutex_init(&logLock,NULL);
  sem_init(&gradingLimit,0,MAXEVALUATORLIMIT);
  pthread_mutex_init(&submissionLock,NULL);
  pthread_cond_init(&submissionCondition,NULL);

  logFd = open("examRecords.txt", O_WRONLY | O_CREAT | O_APPEND , 0644);
  if(logFd < 0){
    printf("Error , File Open Failed\n");
    return -1;
  }

  pthread_t autoSaveThread;
  pthread_create(&autoSaveThread,NULL,autoSave,NULL);


  Student s[MAXSTUDENTS];
  Evaluator e[MAXEVALUATOR];
  pthread_t std_t[MAXSTUDENTS];
  pthread_t eva_t[MAXEVALUATOR];


// Student working in main
  for(int i =0 ;i<MAXSTUDENTS;i++){
    s[i].studentID = i;
    s[i].score = ((rand() % 10000) / 100.00);
    e[i].evaluatorID = i;
  }

  for(int i =0;i<MAXSTUDENTS;i++){
    pthread_create(&std_t[i],NULL,std_thread,&s[i]);
  }

  for(int i =0;i<MAXEVALUATOR;i++){
    pthread_create(&eva_t[i],NULL,eva_thread,&e[i]);
  }

  for(int i =0;i<MAXSTUDENTS;i++){
    pthread_join(std_t[i],NULL);
  }
  pthread_mutex_lock(&examLock);
  examOver = 1;
  pthread_mutex_unlock(&examLock);
  pthread_cond_broadcast(&submissionCondition); 

  for(int i =0;i<MAXEVALUATOR;i++){
    pthread_join(eva_t[i],NULL);
  }

  autoSaveActive = 0;
  pthread_join(autoSaveThread, NULL);
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
