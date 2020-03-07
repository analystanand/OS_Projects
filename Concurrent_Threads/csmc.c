/******************************Libraries*********************************************/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h> 
#include <sys/time.h>

/***********************************************************************************/

/**********************************Constants****************************************/

#define PROGRAMMING 2           // Maximum sleep time for which student goes to programming
#define TUTORING 0.0002         // Tutoring time

/***********************************************************************************/

/*****************************global variables**************************************/

int MAX_CHAIRS = 0;             // For storing max number of chairs
int MAX_HELP = 0;               // For storing max number of times a student can seek help
int occupied_chairs = 0;        // To keep track of occupied chairs
int total_sessions_tutored = 0; // To keep track of total sessions tutored
int total_request = 0;          // To keep track of total requests
long *tutor_id;                 // For storing tutor id
int st_getting_tut = 0;         // To keep track of the students being tutored currently
pthread_mutex_t lock1;          // Lock 1 for student thread
pthread_mutex_t lock2;          // Lock 2 for Queue
pthread_mutex_t lock3;          // Lock 3 for Tutor thread
pthread_mutex_t lock4;          // Lock 4 for Coordinator thread (Note: not used!)
pthread_mutex_t lock5;          // Lock 5 for Priority Queue
sem_t *stdnt_sem;               // Semaphore for student-tutor
sem_t sem1;                     // Semaphore for Student<->Coordinator 
sem_t sem2;                     // Semaphore for Coordinator<->Tutor
struct Queue* q;                // Struct Queue pointer for FIFO Queue 
int fifo_student_id = 0;        // For storing student id
int fifo_visits=0;              // For storing number of times a student has visited
struct priority_q my_q;         // Struct my_q of type Struct priority_q

/***********************************************************************************/

/**************************************Queue****************************************/
/* Queue for chairs in the waiting room where a student sits when he/she arrives in 
   CSMC. If a seat is empty, he/she occupies one and is pushed into this queue.
   Please note: This queue is not the Priority Queue!
*/
/***********************************************************************************/

// A linked list (LL) Node to store a queue entry 
struct QNode { 
	int student_id;
  int visits; 
	struct QNode* next; 
}; 

// The queue, front stores the front node of LL and rear stores the last node of LL 
struct Queue { 
	struct QNode *front, *rear; 
}; 

// A utility function to create a new linked list node. 
struct QNode* newNode(int id,int v) 
{ 
	struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode)); 
	temp->student_id = id; 
  temp->visits = v; 
	temp->next = NULL; 
	return temp; 
} 

// A utility function to create an empty queue 
struct Queue* createQueue() 
{ 
	q = (struct Queue*)malloc(sizeof(struct Queue)); 
	q->front = q->rear = NULL; 
	return q; 
} 

// The function to add a key k to q 
void enQueue(struct Queue* q, int id,int v) 
{ 
	// Create a new LL node 
	struct QNode* temp = newNode(id,v); 

	// If queue is empty, then new node is front and rear both 
	if (q->rear == NULL) { 
		q->front = q->rear = temp; 
		return; 
	} 

	// Add the new node at the end of queue and change rear 
	q->rear->next = temp; 
	q->rear = temp; 
} 

// Function to remove a key from given queue q 
void deQueue() 
{ 
	// If queue is empty, return NULL. 
	if (q->front == NULL) 
		return ; 

	// Store previous front and move front one node ahead 
	struct QNode* temp = q->front;                                                     
	q->front = q->front->next; 
	fifo_student_id = temp->student_id;
	fifo_visits = temp->visits;
	free(temp);    

	// If front becomes NULL, then change rear also as NULL 
	if (q->front == NULL) 
		q->rear = NULL;                                                                                                                                                                                                                                                                     
} 

/*********************************Queue End*****************************************/

/********************************Priority Queue*************************************/
/* When the student takes up a seat and is pushed into our normal queue, the 
   co-ordinator plays an important role and pushes the student into the Priority 
   Queue based on the following conditions:

   1. A Student visiting for the first time gets highest priority
   2. A student who has seeked help k times has a lower priority than a student who 
      has seeked help i times, provided k > i.
   3. If two students have same priority, then the student who came first has a higher
      priority.
*/
/***********************************************************************************/

// A linked list (LL) node to store a queue entry
struct student{
  int id;
  int visit_count;  
  struct student *next;
  struct student *prev;
};

struct priority_q{
  struct student *head;   
  struct student *tail;	  
};

// A utility function to create an empty priority queue
void initialize(){
  my_q.head = (struct student*)malloc(sizeof(struct student));
  my_q.tail = (struct student*)malloc(sizeof(struct student));
  my_q.head->next = my_q.tail;
  my_q.head->prev = NULL;
  my_q.tail->prev = my_q.head;
  my_q.tail->next = NULL;
}

// Function to pop head from priority queue
int pop(){
  int priority_pop_id = 0;
  struct student *temp = my_q.head->next;
  if (temp==my_q.tail)
      return -1;
  priority_pop_id = temp->id; //Added
  my_q.head->next = my_q.head->next->next;
  my_q.head->next->prev = my_q.head;
 // free(temp);
  return priority_pop_id;
}

// Function to add into priority queue
void add_to_priority_q(int id, int visit_count){
  struct student *std = (struct student*)malloc(sizeof(struct student));
  std->id = id;
  std->visit_count = visit_count;
  std->prev = (struct student*)malloc(sizeof(struct student));
  std->next = (struct student*)malloc(sizeof(struct student));
  struct student *temp = my_q.head->next;
  while(temp!=my_q.tail && temp->visit_count<=std->visit_count){
    temp=temp->next;
  }
  if(temp==my_q.tail){
    std->next = my_q.tail;
    std->prev = my_q.tail->prev;
    my_q.tail->prev->next = std;
    my_q.tail->prev = std;
  }
  else{
    std->next = temp;
    temp->prev->next = std;
    std->prev = temp->prev;
    temp->prev = std;
  }
  temp = NULL;
}

/*****************************Priority Queue End************************************/

/******************************Student Function*************************************/

void* student(void *id)
{   
    long n = (long)id;                          // To store current Student's id
    int help_counter = 0;                       // To Keep track of how many times a student has taken help
    int sleep_time = rand() % PROGRAMMING + 1;  // Generating random sleep time
    
    while(1) {
      sleep(sleep_time);                        // Student is programming for upto 2 ms
      pthread_mutex_lock(&lock1);               // Acquiring the lock 1
      if (occupied_chairs <= MAX_CHAIRS){       // If there are empty chairs
        occupied_chairs++;                      // Student sits in a chair
        printf("St: Student %ld takes a seat. Empty chairs = %d.\n",n,MAX_CHAIRS-occupied_chairs);
        help_counter++;                         // To keep track of how many times a student has already taken help
        total_request++;                        // Total requests for tutoring
        pthread_mutex_unlock(&lock1);           // Release the lock 1
        
        pthread_mutex_lock(&lock2);             // Acquire the lock 2 for Queue ops
        enQueue(q,n, help_counter);             // Student found an empty chair so he is enqueued
        pthread_mutex_unlock(&lock2);           // Release the lock 2 for Queue ops

        //printf("****************************************************Student notifies coordinator\n");
        sem_post(&sem1);                        // Notify the coordinator
        //printf("***********************************************************Student waits for tutor\n");
        sem_wait(&stdnt_sem[n]);                // Wait for tutor

        pthread_mutex_lock(&lock1);             // Acquire the lock 1
        occupied_chairs--;                      // Student leaves as he has been tutored
        pthread_mutex_unlock(&lock1);           // Release the lock 1
        
        printf("St: Student %ld received help from Tutor %ld.\n",n,tutor_id[n]);
        if(help_counter==MAX_HELP)              // Student has taken max help
          break;
      } else {
          printf("St: Student %ld found no empty chair. Will try again later\n",n);
          pthread_mutex_unlock(&lock1);         // If a student finds no empty chair he goes back to programming
          sleep(sleep_time);                    // Programming again
      }
    }
  pthread_exit(NULL);
} 

/*****************************Student Function End**********************************/

/*****************************Coordinator Function**********************************/

static void* coordinator(void *id)
{    
    long n = (long)id;                                  // To store Co-ordinator's id

    while(1){
        //printf("**************************************************Coordinator waits for Student\n");
        sem_wait(&sem1);                                // Wait for a student to arrive and take a seat
        
        pthread_mutex_lock(&lock2);                     // Acquire the lock 2 for Queue ops
        deQueue();                                      // Remove Student from Queue to push into Priority Queue 
        printf("Co: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d \n",fifo_student_id,fifo_visits,occupied_chairs,total_request);
        pthread_mutex_unlock(&lock2);                   // Release the lock 2 for Queue ops

        pthread_mutex_lock(&lock5);                     // Acquire the lock 5 for Priority Queue ops
        add_to_priority_q(fifo_student_id,fifo_visits); // Queues the student based on the student’s priority
        pthread_mutex_unlock(&lock5);                   // Release the lock 5 for Priority Queue ops
        
        //printf("******************************************************Coordinator notifies Tutor\n");        
        sem_post(&sem2);                                // Signal a Tutor that a student is here
    }
   pthread_exit(NULL);
}

/*****************************Coordinator Function End******************************/

/*********************************Tutor Function************************************/

static void* tutor(void *id)
{   
    
    long n = (long)id;                // To store current Tutors id
    
    while(1){
      //printf("********************************************************Tutor waits for Coordinator\n");    
      sem_wait(&sem2);                // Waiting for coordinator to notify

      pthread_mutex_lock(&lock5);     // Acquire the lock 5 for Priority Queue ops
      int val = pop();                // Pop the student from Priority Queue
      /*if (val==-1){
        pthread_mutex_unlock(&lock3);
        continue;
      }*/
      pthread_mutex_unlock(&lock5);   // Release the lock 5 for Priority Queue ops
    
      pthread_mutex_lock(&lock3);     // Acquire the lock 3
      st_getting_tut++;               // Current Students' being Tutored inc.
      total_sessions_tutored++;       // Total number of sessions tutored by all the tutors
      printf("Tu: Student %d tutored by Tutor %ld. Students tutored now = %d. Total sessions tutored = %d \n",val,n,st_getting_tut,total_sessions_tutored);
      tutor_id[val] = (long) id;      // Storng the current Tutor's id  
      pthread_mutex_unlock(&lock3);

      sleep(TUTORING);                // Tutor is tutoring

      //printf("*************************************************************Tutor signals Student\n");
      sem_post(&stdnt_sem[val]);      // Tutor has finished tutoring and is now available

      pthread_mutex_lock(&lock3);     // Acquire the lock 3
      st_getting_tut--;               // Decrease current students being tutored counter
      pthread_mutex_unlock(&lock3);   // Release the lock 3
    }
  pthread_exit(NULL);
} 

/****************************Tutor Function End*************************************/

/********************************Main Function**************************************/

int main(int argc, char const *argv[]){

    int no_of_students = 0,no_of_tutors=0;        // Local variables to store number of students and tutors
    long i,j,k=0;                                 // For creating Student and Tutor threads  
    void *value;                                  // 
    
    q = createQueue();                            // Create an empty Queue 
    initialize();                                 // Create an empty Priority Queue

    if (pthread_mutex_init(&lock1, NULL) != 0) {  // Initialize the lock
      fprintf(stderr,"mutex init has failed\n");  // If Lock has failed to initialize
      return 1; 
    }

    if (pthread_mutex_init(&lock2, NULL) != 0) {   // Initialize the lock
      fprintf(stderr,"mutex init has failed\n");   // If Lock has failed to initialize
      return 1; 
    }

    if (pthread_mutex_init(&lock3, NULL) != 0) {   // Initialize the lock
      fprintf(stderr,"mutex init has failed\n");   // If Lock has failed to initialize
      return 1; 
    }
    
    if (pthread_mutex_init(&lock4, NULL) != 0) {    // Initialize the lock
      fprintf(stderr,"mutex init has failed\n");    // If Lock has failed to initialize
      return 1; 
    }
    
    if (pthread_mutex_init(&lock5, NULL) != 0) {    // Initialize the lock
      fprintf(stderr,"mutex init has failed\n");    // If Lock has failed to initialize
      return 1; 
    }

    sem_init(&sem1, 0, 0);                          // Initialize semaphore for Student<->Coordintor
    sem_init(&sem2, 0, 0);                          // Initialize semaphore for Coordinator<->Tutor

    if(argc==5){
      no_of_students =  atoi(argv[1]);              // No. of students
      no_of_tutors   =  atoi(argv[2]);              // No. of tutors
      MAX_CHAIRS     =  atoi(argv[3]);              // Max chairs
      MAX_HELP       =  atoi(argv[4]);              // Max times student can seek help
    }
    else{
        fprintf(stderr,"Too few arguments\n");      // Print to Standard if arguments not sufficient 
        exit(1);  
    }
    
    // Declare student and Tutor Threads
    pthread_t stud_thread[no_of_students] , *tut_thread , coordinator_thread; 
    
    // stud_thread =  malloc(sizeof(pthread_t) * no_of_students);   // Allocate memory to student threads
    tut_thread =  malloc(sizeof(pthread_t) * no_of_tutors);         // Allocate memory to tutor threads

    stdnt_sem = (sem_t*)malloc(sizeof(sem_t)*no_of_students);       // Initialize array of student semaphores 
    
    for(i=0;i<no_of_students;i++){
       sem_init(&stdnt_sem[i], 0, 0);                               // Initializing all the students' semaphores
    }

    tutor_id = malloc(sizeof(long)*no_of_students);                 // Allocate memory for storing tutor ids

    pthread_create(&coordinator_thread,NULL,coordinator,(void*)k);  // Create a single coordinator thread  
    
    // Create all tutor threads
    for(j = 0; j < no_of_tutors; j++) {
        assert(pthread_create(&tut_thread[j], NULL, tutor, (void *) j) == 0);
    }  
    // Create all student threads
    for(i = 0; i < no_of_students; i++) {
        assert(pthread_create(&stud_thread[i], NULL, student, (void *) i) == 0);
     }

    // Wait for all student threads to finish
    for(i = 0; i < no_of_students; i++) {
         assert(pthread_join(stud_thread[i], NULL) == 0);
    }

    // Free all memory 
    //free(stud_thread); 
    free(tut_thread);
    free(stdnt_sem);
    free(tutor_id);

    exit(EXIT_SUCCESS);  // Exit abandoning infinite loop of Tutor thread
    return 0;
}

/****************************Main Function End**************************************/
/*                             
  Please note: There are a lot of commented out printf() statements before semaphores.
  If you are keen to know how this concurrent program is working, un-comment all the 
  commented out printf()'s and run small arguments like ./csmc 10 2 3 2
                                End of Code
*/
/***********************************************************************************/