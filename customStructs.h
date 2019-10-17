//structure for the dispatch of quantum and pid
struct Dispatch{
	int index;
        int quantum;
        long long int pid;
	int flag;
};

//structure for the shared memory clock
struct Clock{
        unsigned int second;
        unsigned int nano;
};

//Structure for the Process Control Block
struct PCB{
        struct Clock launch;
        struct Clock dispatch;
        unsigned long long int CPU;
        unsigned long long int  system;
	unsigned long long int duration;
        int burst;
        pid_t simPID;
        int priority;
};
//for the message queue
struct mesg_buffer {
        long mesg_type;
	pid_t pid;
        char mesg_text[100];
} message;

//for the priority queue's
//Node in a queue
struct Node {
	int key;
	struct Node* next;
};
//Queue
struct Queue {
	struct Node *front, *rear;
};

struct Node* newNode(int x){
	struct Node *temp = (struct Node*)malloc(sizeof(struct Node));
	temp->key = x;
	temp->next = NULL;
	return temp;
}

struct Queue* createQueue(){
	struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
        q->front = q->rear = NULL; 
        return q; 
} 

void enQueue(struct Queue* q, int k) 
{ 
	struct Node* temp = newNode(k); 
	if (q->rear == NULL) { 
	        q->front = q->rear = temp; 
	        return; 
	} 
	q->rear->next = temp; 
	q->rear = temp; 
}
struct Node* deQueue(struct Queue* q) { 
	if (q->front == NULL){ 
		return NULL; 
	}
	struct Node* temp = q->front; 
	free(temp); 
	q->front = q->front->next; 	
	if (q->front == NULL) {
	        q->rear = NULL;
	} 
	return temp; 
} 
