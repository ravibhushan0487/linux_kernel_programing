#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

//DEFINITIONS
#define DUMP _IOWR(530,0, struct dump_arg)
#define NUMBER_OF_THREADS 5
#define MAX_HASHTABLE_DATA 200
#define MAX_TABLE_OPERATION 100

#define SEARCH_OPERATION 1
#define ADD_OPERATION 2
#define DELETE_OPERATION 3



//HASH TABLE OBJECT
typedef struct ht_object
{
    int key;
    int data ;
} ht_object_t;

//IOCTL STRUCT
struct dump_arg 
{
    int n;                          //Bucket number N
    ht_object_t object_array[8];    //8 objects to retrieve
};

int hashdata_counter = 0;
int hashtable_operations = 0;
int fd_write_1,fd_write_2;

pthread_mutex_t common_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

void read_hash_table(int ht_table_number,int key) 
{
    int ret;
    ht_object_t *ht_node;
    ht_node = (ht_object_t *) malloc(sizeof(ht_object_t));
    ht_node->key = key;
    if(ht_table_number) 
    {
        
        ret = read(fd_write_1, ht_node, sizeof(ht_object_t));
    } 
    else 
    {
        ret = read(fd_write_2, ht_node, sizeof(ht_object_t));
    }
    if(ht_node->data == -1) 
    {
        printf("ht530-%d Key: %d Data: Not found\n",ht_table_number, ht_node->key);
    } 
    else 
    {
        printf("ht530-%d Key: %d Data: %d\n",ht_table_number, ht_node->key, ht_node->data);
    }
}

void add_to_hash_table() 
{
    int ht_table_number = rand() % 2;
    
    ht_object_t *ht_node;
    ht_node = (ht_object_t *) malloc(sizeof(ht_object_t));
    ht_node->key = rand() % 200;
    ht_node->data = rand() % 200;
    
    if(ht_table_number) 
    {
        write(fd_write_1, ht_node, sizeof(ht_object_t));
    } 
    else 
    {
        write(fd_write_2, ht_node, sizeof(ht_object_t));
    }
    printf("WRITE - ");
    read_hash_table(ht_table_number,ht_node->key);
}

void delete_from_hash_table() 
{
    int ht_table_number = rand() % 2;
    
    ht_object_t *ht_node;
    ht_node = (ht_object_t *) malloc(sizeof(ht_object_t));
    ht_node->key = rand() % 200;
    ht_node->data = 0;
    printf("DELET - ");
    if(ht_table_number) {
        write(fd_write_1, ht_node, sizeof(ht_object_t));
    } 
    else 
    {
        write(fd_write_2, ht_node, sizeof(ht_object_t));
    }
    read_hash_table(ht_table_number,ht_node->key);
}

void search_hash_table() 
{
    int ht_table_number = rand() % 2;
    
    ht_object_t *ht_node;
    ht_node = (ht_object_t *) malloc(sizeof(ht_object_t));
    ht_node->key = rand() % 200;
    printf("READI - ");
    read_hash_table(ht_table_number,ht_node->key);
}

//Thread function
void *thread_function(void *context)
{
    pthread_t pid = pthread_self();
    
    while(hashdata_counter < MAX_HASHTABLE_DATA)
    {
        struct timespec current_time;
        pthread_mutex_lock(&common_mutex_lock);
        int ret = clock_gettime(CLOCK_MONOTONIC,&current_time);
        if(ret)
        {
            perror("unable to get clock_gettime");
            exit(1);
        }
        current_time.tv_nsec+= rand() % 50000000;
        current_time.tv_nsec+= 10000000;
        add_to_hash_table();
        hashdata_counter++;
        pthread_mutex_unlock(&common_mutex_lock);
        clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&current_time, NULL);
        
    }
    while(hashtable_operations < MAX_TABLE_OPERATION)
    {
        struct timespec current_time;
        pthread_mutex_lock(&common_mutex_lock);
        int ret = clock_gettime(CLOCK_MONOTONIC,&current_time);
        if(ret)
        {
            perror("unable to get clock_gettime");
            exit(1);
        }
        current_time.tv_nsec+= rand() % 50000000;
        current_time.tv_nsec+= 10000000;
        int operation;
        operation = rand() % 3;
        operation+= 1;
        if(operation == SEARCH_OPERATION) 
        {
            search_hash_table();
        } 
        else if(operation == ADD_OPERATION) 
        {
            add_to_hash_table();
        } 
        else 
        {
            delete_from_hash_table();
        }
        hashtable_operations++;
        pthread_mutex_unlock(&common_mutex_lock);
        clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&current_time, NULL);
    }
    pthread_exit(NULL);
}



int main()
{
    pthread_t pid[NUMBER_OF_THREADS];
    ht_object_t *ht_node;
    ht_node = (ht_object_t *) malloc(sizeof(ht_object_t));

    fd_write_1 = open("/dev/ht530-1",O_RDWR);
    if(fd_write_1 < 0) 
    {
        printf("Unable to open ht530-1 device\n");
        exit(1);
    }
    fd_write_2 = open("/dev/ht530-2",O_RDWR);
    if(fd_write_2 < 0) 
    {
        printf("Unable to open ht530-2 device\n");
        exit(1);
    }
       
    printf("BOTH DEVICES OPENED\n");

    
    
    //THREAD CREATION
    int index;
    for (index=0; index<NUMBER_OF_THREADS; index++) 
    {
        pthread_attr_t attr;
        struct sched_param param;
        pthread_attr_init(&attr);

        // safe to get existing scheduling param
        pthread_attr_getschedparam (&attr, &param);
        
        // set the priority; others are unchanged 
        param.sched_priority = index;
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        
        // setting the new scheduling param 
        pthread_attr_setschedparam(&attr,&param);
        pthread_create(&pid[index],&attr,thread_function,NULL);
    }



    // JOIN - WAITING TO END
    int joinindex;
    for (joinindex=0; joinindex<NUMBER_OF_THREADS; joinindex++) 
    {
        pthread_join(pid[joinindex], NULL);
    }
   


    // IOCTL DUMP
    int j = 0;
    int i;
    struct dump_arg* darg = (struct dump_arg*) malloc(sizeof(struct dump_arg));
    printf("\nIOCTL: HASH TABLE 1\n");
    while(j<128) 
    {
        darg->n = 8;
        ioctl(fd_write_1, DUMP, darg);
        printf("IOCTL BUCKET: %d\n", j);
        for(i = 0; i < darg->n; ++i) 
        {
          printf("%d K(%d) D(%d) |", i+1, darg->object_array[i].key, darg->object_array[i].data);
        }
        printf("\n");
        j++;
    }
    printf("\nIOCTL: HASH TABLE 2\n");
    j=0;
    while(j<128) 
    {
        darg->n = 8;
        ioctl(fd_write_2, DUMP, darg);
        printf("IOCTL BUCKET: %d\n", j);
        for(i = 0; i < darg->n; ++i) 
        {
          printf("%d K(%d) D(%d) |", i+1, darg->object_array[i].key, darg->object_array[i].data);
        }
        printf("\n");
        j++;
    }
    close(fd_write_1);
    close(fd_write_2);
    return 0;
}


