/*
  Name: Luke Mclaren
  Std #: V00763009
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// DEFINTIONS FOR DIRECTIONS
#define HIGH_EAST 'E'
#define LOW_EAST 'e'
#define HIGH_WEST 'W'
#define LOW_WEST 'w'

// MUTEXES & CONDITION VARIABLES
pthread_mutex_t load_mutex		= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  load_var		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t queue_mutex		= PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t dispatch_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  dispatch_var	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t start_mutex		= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  start_var		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t track_mutex		= PTHREAD_MUTEX_INITIALIZER;

//TRAIN STRUCTURE
typedef struct{
	pthread_t thread;
	pthread_cond_t grant_var;
	int num;
	int crossing_time;
	int loading_time;
	char priority;
	char direction[4];
} Train;

//NODE STRUCTURE
typedef struct node {
  Train *train;
  struct node *next;
} node;

//QUEUES
struct node *head = NULL;
struct node *e_queue = NULL;
struct node *E_queue = NULL;
struct node *w_queue = NULL;
struct node *W_queue = NULL;

//GLOBAL VARIABLES
int MAX_TRAINS;
int TRAIN_CTR = 0;
int SECONDS = 0;
int FIRST_TRAIN = 0;

//TRACK TRACKER
char last_accross = '0';

/*	PARSE FUNCTION

	Parses the input file, creating a node for each train that contains a train struct.
	The node is then pushed onto the queue
	
*/

void * parse(char *argv[]){
	FILE *train_info = fopen(argv[1], "r"); 		//open train file
	char buff[255];

	int train_number = 0;

	sscanf((char *)argv[2],"%d",&MAX_TRAINS);		//set MAX_TRAINS to the correct argument

	while(fscanf(train_info, "%s", buff) != EOF){	// Initialize a node and train struct for each 
		struct node *new_node = (struct node*)malloc(sizeof(struct node*));
		Train *train = malloc(sizeof(Train));
		
		train->num = train_number;
		train->grant_var = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
		train_number = train_number+1;

		train->priority = strtok(buff, ",:")[0];
		if(train->priority == 'e' || train->priority == 'E'){
			strcpy(train->direction, "East");
		}else{
			strcpy(train->direction, "West");
		}
		sscanf(strtok(NULL, ",:"),"%d",&train->loading_time);
		sscanf(strtok(NULL, ",:"),"%d",&train->crossing_time);
		
		new_node->train = train; 					//Append each train to the head queue
		new_node->next = head;
		head=new_node;
	}	
}

/*	PUSH FUNCTION

	Pushes the train nodes to their corresponding queues

*/

void * push(struct node* node, int train_num){
	pthread_mutex_lock(&queue_mutex);
	if(HIGH_EAST == node->train->priority){

		if(E_queue != NULL){
			struct node *curr = E_queue;
			while(curr->next != NULL) {
				curr = curr->next;
			}
			curr->next = node;
			node->next = NULL;
		}else{
			node->next = E_queue;
			E_queue = node;
		}

	}else if(LOW_EAST == node->train->priority){

		if(e_queue != NULL){
			struct node *curr = e_queue;
			while(curr->next != NULL) {
				curr = curr->next;
			}
			curr->next = node;
			node->next = NULL;
		}else{
			node->next = e_queue;
			e_queue = node;
		}

	}else if(HIGH_WEST == node->train->priority){

		if(W_queue != NULL){
			struct node *curr = W_queue;
			while(curr->next != NULL) {
				curr = curr->next;
			}
			curr->next = node;
			node->next = NULL;
		}else{
			node->next = W_queue;
			W_queue = node;
		}

	}else{

		if(w_queue != NULL){
			struct node *curr = w_queue;
			while(curr->next != NULL) {
				curr = curr->next;
			}
			curr->next = node;
			node->next = NULL;
		}else{
			node->next = w_queue;
			w_queue = node;
		}
	}
	printf("%02d:", (node->train->loading_time/(60 * 60))); //hours
	printf("%02d:", node->train->loading_time/600); //minutes
	printf("%02d.", ((node->train->loading_time/10)%60)); //seconds
	printf("%d ", node->train->loading_time%10); //tenths
	printf("Train  #%d is ready to go ", train_num);
	printf("%s\n", node->train->direction);

	pthread_cond_signal(&start_var); //Signal the dispatcher that it may start  

	pthread_mutex_unlock(&queue_mutex);
}

/*	MAIN_TRAIN FUNCTION

	When a thread enters this function it loads (usleeps) for it's loading time, and then pushes itself
	to it's correspoding queue and blocks itself on it's condition variable. 

	The thread will unblock when signaled by the dispatcher and cross the main track (usleep) for
	it's crossing time.

*/

void * main_train(void *train_number){
    struct node *curr = head;

	int train_num =*((int*)train_number);

	while(curr) {	// find threads corresponding train in the head queue
		if(curr->train->num == train_num){	// once the train is found enter the load_mutex

			pthread_mutex_lock(&load_mutex);

			TRAIN_CTR = TRAIN_CTR + 1;		// increment the train counter
			
			if(TRAIN_CTR != MAX_TRAINS){	// wait till all trains - 1 are blocked on the load_var
				pthread_cond_wait(&load_var, &load_mutex);
			}else{							// signal all trains to begin loading once the last train enters
				TRAIN_CTR = 0;
				pthread_cond_broadcast(&load_var);
			}

			pthread_mutex_unlock(&load_mutex);
			
			usleep(curr->train->loading_time * 100000);		// All trains begin "loading"

			push(curr, train_num);							// after loading push train onto corresponding queue
			pthread_mutex_lock(&track_mutex);				// enter the main track mutex and block on corresponding train con_var
			
			pthread_cond_wait(&curr->train->grant_var, &track_mutex);
			
			if(FIRST_TRAIN == 0){							// Used for print the timestamp
				SECONDS = SECONDS + curr->train->loading_time;
				FIRST_TRAIN = 1;
			}

			printf("%02d:", (SECONDS/(600 * 60))); 
 			printf("%02d:", ((SECONDS/600)%600));
 			printf("%02d.", ((SECONDS/10)%60));
 			printf("%d ", SECONDS%10);

			printf("Train  #%d is ON the main track going ", train_num);
			printf("%s\n", curr->train->direction);
			usleep(curr->train->crossing_time * 100000);	// Train is "Crossing the track"
			pthread_cond_signal(&dispatch_var);	
			
			SECONDS = SECONDS + curr->train->crossing_time;
 			printf("%02d:", (SECONDS/(600 * 60))); //hours
 			printf("%02d:", ((SECONDS/600)%600)); //minutes
 			printf("%02d.", ((SECONDS/10)%60)); //seconds
 			printf("%d ", SECONDS%10); //tenths
			
			printf("Train  #%d is OFF the main track after going ", train_num);
			printf("%s\n", curr->train->direction);
			pthread_mutex_unlock(&track_mutex);

			break;
		}else{
			curr = curr->next;
		}
	}
}

/*	SWAP FUNCTION

	Swap takes the current node and checks if there is train in the node's queue that has the same loading time
	but a lower number. If so it takes the place of the original node and returns itself. If not the original node returns.

*/

struct node * swap(struct node * curr){
	struct node * temp = curr->next;
	
	int lowest_train_num = curr->train->num;
	int loading_time = curr->train->loading_time;
	
	while(temp != NULL){
		if(temp->train->num < lowest_train_num && temp->train->loading_time == loading_time){
			lowest_train_num = temp->train->num;
		}
		temp = temp->next;
	}

	temp = curr;

	if(temp->train->num == lowest_train_num){
		return temp;
	}else{
		while(temp != NULL){
			if(temp->next == NULL){
				if(temp->train->loading_time == loading_time){
					return temp;
				}else{
					return curr;
				}
			}else if(temp->next->train->num == lowest_train_num){
				struct node * temp_2 = temp->next;
				temp->next = temp_2->next;
				temp_2->next = curr;
				return temp_2;
			}
			temp = temp->next;
		}
	}
}

/*	DISPATCH FUNCTION

	Dispatch loops untill all trains have crossed the main track, comparing the heads of all
	station queues and pops the head off and runs that train accross the main track	

*/

void * dispatch(){
	while(TRAIN_CTR != MAX_TRAINS){
		usleep(200000); 	// Adds a jitter to the dispatcher to avoid small race conditions with the usleep
		struct  node *curr = NULL;
		char * direction;
				
		if('0' == last_accross || 'E' == last_accross || 'e' == last_accross){	// Handles the back and forth of the main track
			if(W_queue != NULL){
				W_queue = swap(W_queue); // runs the swap function to ensre the head of the function is the train with the lowest #
				curr = W_queue;
				W_queue = curr->next;
				last_accross = 'W';
			}else if(E_queue !=NULL){
				E_queue = swap(E_queue);
				curr = E_queue;
				E_queue = curr->next;
				last_accross = 'E';
			}else if(w_queue !=NULL){
				w_queue = swap(w_queue);
				curr = w_queue;
				w_queue = curr->next;
				last_accross = 'w';
			}else if(e_queue != NULL){
				e_queue = swap(e_queue);
				curr = e_queue;
				e_queue = curr->next;
				last_accross = 'e';
			}
		} else{
			if(E_queue != NULL){
				E_queue = swap(E_queue);
				curr = E_queue;
				E_queue = curr->next;
				last_accross = 'E';
			}else if(W_queue !=NULL){
				W_queue = swap(W_queue);
				curr = W_queue;
				W_queue = curr->next;
				last_accross = 'W';
			}else if(e_queue !=NULL){
				e_queue = swap(e_queue);
				curr = e_queue;
				e_queue = curr->next;
				last_accross = 'e';
			}else if(w_queue != NULL){
				w_queue = swap(w_queue);
				curr = w_queue;
				w_queue = curr->next;
				last_accross = 'w';
			}
		}

		if(curr){	// If a train has been selected to cross the track block on
					// the condition variable untill finished crossing and increment the train counter
			pthread_mutex_lock(&track_mutex);
			pthread_cond_signal(&curr->train->grant_var);
			pthread_cond_wait(&dispatch_var, &track_mutex);
			TRAIN_CTR = TRAIN_CTR + 1;
			pthread_mutex_unlock(&track_mutex);
		}
		
	}
}

/*	MAIN FUNCTION

	Ensures the program is being executed correctly and if so creates the threads
	for each train and appends them to a queue (linked list) and waits untill 
	the first train has loaded to begin the dispatching function.

*/

int main(int argc, char *argv[]) {

	if(argc == 3){
		parse(argv); 				// function to parse the input file, takes arguments

		struct node *curr = head; 	// points to the head queue
		
		while(curr) {				// create thread for each train in the head queue 
									// and enter the main_train function with the threads corresponding train number
			pthread_create(&curr->train->thread, NULL, main_train, (void *) &curr->train->num); 
			curr = curr->next;
		}
		
		pthread_mutex_lock(&start_mutex);
		pthread_cond_wait(&start_var, &start_mutex); // Wait for first train to finish loading
		pthread_mutex_unlock(&start_mutex);

		dispatch(); // Begin to dispatch

	}else{
		printf("Usage: ./schdlr <train_file> <#_of_trains>\n");
		return 0;
	}  
}
