/*
  Name: Luke Mclaren
  Std #: V00763009
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define HIGH_EAST 'E'
#define LOW_EAST 'e'
#define HIGH_WEST 'W'
#define LOW_WEST 'w'

pthread_mutex_t load_mutex		= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  load_var		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t queue_mutex		= PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t dispatch_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  dispatch_var	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t start_mutex		= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  start_var		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t track_mutex		= PTHREAD_MUTEX_INITIALIZER;

typedef struct{
	pthread_t thread;
	pthread_cond_t grant_var;
	int num;
	int crossing_time;
	int loading_time;
	char priority;
	char direction[4];
} Train;

typedef struct node {
  Train *train;
  struct node *next;
} node;

struct node *head = NULL;
struct node *e_queue = NULL;
struct node *E_queue = NULL;
struct node *w_queue = NULL;
struct node *W_queue = NULL;

int MAX_TRAINS;
int TRAIN_CTR = 0;
int SECONDS = 0;
int FIRST_TRAIN = 0;

char last_accross = '0';

void * parse(char *argv[]){
	FILE *train_info = fopen(argv[1], "r");
	char buff[255];

	int train_number = 0;

	sscanf((char *)argv[2],"%d",&MAX_TRAINS);

	while(fscanf(train_info, "%s", buff) != EOF){
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
		
		new_node->train = train;
		new_node->next = head;
		head=new_node;
	}	
}

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
	printf("%d ", node->train->loading_time);
	printf("Train  #%d is ready to go ", train_num);
	printf("%s\n", node->train->direction);

	pthread_cond_signal(&start_var);

	pthread_mutex_unlock(&queue_mutex);
}

void * load(void *train_number){
    struct node *curr = head;

	int train_num =*((int*)train_number);

	while(curr) {
		if(curr->train->num == train_num){

			pthread_mutex_lock(&load_mutex);

			TRAIN_CTR = TRAIN_CTR + 1;
			
			if(TRAIN_CTR != MAX_TRAINS){			
				pthread_cond_wait(&load_var, &load_mutex);
			}else{
				TRAIN_CTR = 0;
				pthread_cond_broadcast(&load_var);
			}

			pthread_mutex_unlock(&load_mutex);
			
			usleep(curr->train->loading_time * 100000);

			push(curr, train_num);
			pthread_mutex_lock(&track_mutex);
			
			pthread_cond_wait(&curr->train->grant_var, &track_mutex);
			if(FIRST_TRAIN == 0){
				SECONDS = SECONDS + curr->train->loading_time;
				FIRST_TRAIN = 1;
			}
			printf("%d ", SECONDS);
			printf("Train  #%d is ON the main track going ", train_num);
			printf("%s\n", curr->train->direction);
			usleep(curr->train->crossing_time * 100000);
			pthread_cond_signal(&dispatch_var);	
			SECONDS = SECONDS + curr->train->crossing_time;
 			printf("%d ", SECONDS);
			printf("Train  #%d is OFF the main track after going ", train_num);
			printf("%s\n", curr->train->direction);
			pthread_mutex_unlock(&track_mutex);

			break;
		}else{
			curr = curr->next;
		}
	}
}

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

void * dispatch(){
	while(TRAIN_CTR != MAX_TRAINS){
		usleep(200000);
		struct  node *curr = NULL;
		char * direction;
				
		if('0' == last_accross || 'E' == last_accross || 'e' == last_accross){
			if(W_queue != NULL){
				W_queue = swap(W_queue);
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

		if(curr){
			pthread_mutex_lock(&track_mutex);
			pthread_cond_signal(&curr->train->grant_var);
			pthread_cond_wait(&dispatch_var, &track_mutex);
			TRAIN_CTR = TRAIN_CTR + 1;
			pthread_mutex_unlock(&track_mutex);
		}
		
	}
}

int main(int argc, char *argv[]) {

	if(argc == 3){
		parse(argv);

		struct node *curr = head;
		while(curr) {
			pthread_create(&curr->train->thread, NULL, load, (void *) &curr->train->num);
			curr = curr->next;
		}
		
		pthread_mutex_lock(&start_mutex);
		pthread_cond_wait(&start_var, &start_mutex);
		pthread_mutex_unlock(&start_mutex);

		dispatch();

	}else{
		printf("Usage: ./schdlr <train_file> <#_of_trains>\n");
		return 0;
	}  
}
