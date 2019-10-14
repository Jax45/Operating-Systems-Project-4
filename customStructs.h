//structure for the dispatch of quantum and pid
struct Dispatch{
        int quantum;
        long long int pid;
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
        int CPU;
        int system;
        int burst;
        long int simPID;
        int priority;
};
//for the message queue
struct mesg_buffer {
        long mesg_type;
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
