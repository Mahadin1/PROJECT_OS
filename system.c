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

#define MAXSTUDENTS 100
#define MAXEVALUATORLIMIT 5
#define MAXEVALUATOR 20
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

void logEvent(char *msg){
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char buffer[32];
  strftime(buffer, sizeof(buffer) , "%Y-%m-%d %H:%M:%S", t);

  char logLine[256];
  snprintf(logLine,sizeof(logLine),"|%s| %s\n",buffer,msg);
  pthread_mutex_lock(&logLock);
  write(logFd,logLine,strlen(logLine));
  pthread_mutex_unlock(&logLock);
}
// student thread
void* std_thread(void* arg){
  Student s = *(Student *)arg;
  char msg[100];
  time_t startTime  = time(NULL);
  sleep(rand() % MAXSTUDENTEXAMTIME);
  time_t endTime = time(NULL);
  double examtimetaken = difftime(endTime,startTime);
  double finalScore = 0;
  int cheated = 0;
  int copyFrom = -1;
  int timeOut = 0;
  if(examtimetaken <= EXAMTIMELIMIT){
    timeOut = 0;
    finalScore = s.score;
  }else{
    int cheatFlag = rand() % 2;
    if(cheatFlag == 1){
      pthread_mutex_lock(&submissionLock);
      if(submissionCount > 0){
        int from = rand() % submissionCount;
        finalScore = submissionQueue[from].score;
        copyFrom = submissionQueue[from].studentID;
        cheated = 1;
      }else{
        finalScore = 0;
        timeOut = 1;
      }
      pthread_mutex_unlock(&submissionLock);
    }else{
      finalScore = 0;
      timeOut = 1;
    }
  }
  if(timeOut == 1){
    snprintf(msg,sizeof(msg), "Student %d Exam , CAUSE | TIMEOUT | time: %.1fs",s.studentID,examtimetaken);
  }else{
    snprintf(msg,sizeof(msg), "Student %d submitted the Exam in time %.1fs",s.studentID,examtimetaken);
  }
  logEvent(msg);
  // printf("%s\n",msg);
  pthread_mutex_lock(&submissionLock);
  submissionQueue[submissionCount].studentID = s.studentID;
  submissionQueue[submissionCount].graded = 0;
  submissionQueue[submissionCount].timeTaken = examtimetaken;
  submissionQueue[submissionCount].score = finalScore;
  submissionQueue[submissionCount].cheated = cheated;
  submissionQueue[submissionCount].cheatedFrom = copyFrom; 
  submissionQueue[submissionCount].timeOut = timeOut;
  totalSubmitted++;
  submissionCount++;
  pthread_cond_signal(&submissionCondition);
  pthread_mutex_unlock(&submissionLock); 
  return NULL;
}
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
    snprintf(msg,sizeof(msg),"Evaluator %d is grading student %d",e.evaluatorID,r.studentID);
    // printf("%s\n",msg);
    logEvent(msg);
    if(r.timeOut == 1){
      pthread_mutex_lock(&resultLock);
      studentResults[resultCount].graded = 0;
      studentResults[resultCount].studentID = r.studentID;
      studentResults[resultCount].score = 0;
      studentResults[resultCount].timeOut = r.timeOut;
      studentResults[resultCount].timeTaken = r.timeTaken;
      resultCount++;
      pthread_mutex_unlock(&resultLock);
      timeoutCount++;
      snprintf(msg,sizeof(msg),"Evalator %d did not evalute student %d |TIMEOUT|",e.evaluatorID,r.studentID);
      // printf("%s\n",msg);
      logEvent(msg);
    }else{
    sem_wait(&gradingLimit);
  sleep(2);
  pthread_mutex_lock(&resultLock);
  studentResults[resultCount].graded = 1;
  studentResults[resultCount].studentID = r.studentID;
  studentResults[resultCount].score = r.cheated  == 1 ? 0.0 : r.score;
  studentResults[resultCount].timeOut = r.timeOut;
  studentResults[resultCount].timeTaken = r.timeTaken;
  studentResults[resultCount].cheated = r.cheated;
  studentResults[resultCount].cheatedFrom = r.cheatedFrom;
  // studentResults[e.studentID].score = (rand() % 100); 
  resultCount++;
  if(r.cheated == 1){
    cheatedCount++;
    snprintf(msg,sizeof(msg),"Evalator %d Fails student %d Because of Cheating",e.evaluatorID,r.studentID);
  }else{
    snprintf(msg,sizeof(msg),"Evalator %d evaluated student %d",e.evaluatorID,r.studentID);
  }
  // printf("%s\n",msg);
  logEvent(msg);
  pthread_mutex_unlock(&resultLock);
  sem_post(&gradingLimit);
}
}
  return NULL;
} 

void* dashboard(void* arg){
  while(1){
    system("clear");

    time_t now = time(NULL);
    double timepassed = difftime(now, examStartTime);
    int n = 0;
    sem_getvalue(&gradingLimit,&n);
    int currentGrading = MAXEVALUATORLIMIT - n;
    

    printf("    ======= LIVE DASHBOARD =======          \n");
    printf("          LIVE EXAM SITUATION    \n");
    printf(" ==========================================\n");

    printf("Student Submitted : %d / %d\n",totalSubmitted,MAXSTUDENTS);
    printf("Student Graded    : %d / %d\n",resultCount,MAXSTUDENTS);
    printf("Time OUT          : %d\n",timeoutCount);
    printf("Cheater Caught    : %d\n",cheatedCount);
    printf("Currently Grading : %d\n",currentGrading);
    printf("--------------------------------------------\n");
    printf("TIME PASSED : %.0f\n",timepassed);
    printf("--------------------------------------------\n\n");

    if(resultCount >= MAXSTUDENTS){
      break;
    }
    sleep(1);
  }
  return NULL;
}

void* autoSave(void *arg){
  while(autoSaveActive){

    sleep(AUTOSAVEROUTINE);
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer) , "%Y-%m-%d %H:%M:%S", t);
    //AS -> autoSave
    int fdAS = open("autoSave.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fdAS < 0){
      printf("Auto Save Fails\n");
      continue;
    }

    pthread_mutex_lock(&resultLock);
    char msg[100];
    snprintf(msg,sizeof(msg),"AUTOSAVE , DATE AND TIME  : %s\n",buffer);
    write(fdAS,msg,strlen(msg));

    for(int i =0;i< resultCount;i++){
      char status[10];
      strcpy(status, studentResults[i].cheated  ? "CHEATED"  :studentResults[i].timeOut  ? "TIMED OUT": "PASSED");
      snprintf(msg,sizeof(msg),"Student %d | score : %.1f | graded : %d | Status : %s\n",studentResults[i].studentID,studentResults[i].score,studentResults[i].graded,status);
      write(fdAS,msg,strlen(msg));
    }
    pthread_mutex_unlock(&resultLock);
    close(fdAS);
    logEvent("AutoSave Done");
  }
  return NULL;
}
void genAnalytics(){
  int passed = 0;
  int failed = 0;
  int cheated = 0;
  int timedout = 0;
  double highestscore = 0;
  int highestID  = -1;
  double lowest = 0;
  double sumScore =0;
  double scoreavg = 0;
  int totalstd = 0;


  for(int i =0;i<resultCount;i++){
    if(studentResults[i].score > 0.0){
      lowest = studentResults[i].score;
      break;
    }
  }

  for(int i =0;i<resultCount;i++){
    if(studentResults[i].timeOut == 1){
      timedout++;
    }else if(studentResults[i].cheated == 1){
      cheated++;
    }else if(studentResults[i].score >= 50){
      passed++;
    }else{
      failed++;
    }

    if(highestscore < studentResults[i].score){
      highestID = studentResults[i].studentID;
      highestscore = studentResults[i].score;
    }
    if(lowest > studentResults[i].score && studentResults[i].score > 0){
      lowest = studentResults[i].score;
    }
    if(studentResults[i].timeOut == 0 && studentResults[i].cheated == 0) {
     sumScore += studentResults[i].score;
     totalstd++;
    }
  }

  scoreavg = sumScore/totalstd;


  printf(" ------ Exam Analytics ------\n");
  printf("Total Students : %d\n",MAXSTUDENTS);
  printf("Passed : %d\n",passed);
  printf("Failed : %d\n",failed);
  printf("Cheated : %d\n",cheated);
  printf("TimeOUT : %d\n",timedout);
  printf("Highes Score (STUDENT-ID, SCORE) : (%d , %.1f)\n",highestID,highestscore);
  printf("Average Score : %.1f\n",scoreavg);
  printf("Lowest Score : %.1f\n",lowest);

  int fdAna = open("ExamAnalytics.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if(fdAna < 0){
    printf("Analytics write failed\n");
    return;
  }
  char writedata[500];
  snprintf(writedata,sizeof(writedata)," ------ Exam Analytics ------\n"
  "Total Students : %d\n"
  "Passed : %d\n"
  "Failed : %d\n"
  "Cheated : %d\n"
  "TimeOUT : %d\n"
  "Highes Score (STUDENT-ID, SCORE) : (%d , %.1f)\n"
  "Average Score : %.1f\n"
  "Lowest Score : %.1f\n",MAXSTUDENTS,passed,failed,cheated,timedout,highestID,highestscore,scoreavg,lowest); 
  write(fdAna,writedata,strlen(writedata));

  close(fdAna);
  logEvent("Analytics Generated\n");
}
int main(){
  srand(time(NULL));
  printf("System is Starting\n");
// Initlializing all the dataute
  pthread_mutex_init(&examLock,NULL);
  pthread_mutex_init(&resultLock,NULL);
  pthread_mutex_init(&logLock,NULL);
  sem_init(&gradingLimit,0,MAXEVALUATORLIMIT);
  pthread_mutex_init(&submissionLock,NULL);
  pthread_cond_init(&submissionCondition,NULL);
  examStartTime = time(NULL);

  logFd = open("examRecords.txt", O_WRONLY | O_CREAT | O_APPEND , 0644);
  if(logFd < 0){
    printf("Error , File Open Failed\n");
    return -1;
  }

  pthread_t dashboardThread;
  pthread_create(&dashboardThread,NULL,dashboard,NULL);
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
  pthread_join(dashboardThread, NULL);
  dashboardLive = 0;
  genAnalytics();
// ===== PRINT ALL RESULTS =====
  printf("\n--- Submitted Results ---\n");
    for(int i = 0; i < resultCount; i++){
    printf("Student %d      | Score: %.2f     | Time: %.1fs | %s\n",
    studentResults[i].studentID,
    studentResults[i].score,
    studentResults[i].timeTaken,
    studentResults[i].cheated  ? "CHEATED"  :
    studentResults[i].timeOut  ? "TIMED OUT": "HONEST");
    }

  logEvent("End Exam");
  close(logFd);

  pthread_mutex_destroy(&logLock);
  pthread_mutex_destroy(&examLock);
  pthread_mutex_destroy(&submissionLock);
  pthread_cond_destroy(&submissionCondition);
  sem_destroy(&gradingLimit);
  pthread_mutex_destroy(&resultLock);
  return 0;
}
