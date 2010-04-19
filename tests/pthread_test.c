#include <rstdio.h>
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define printf_safe(...) \
	pthread_mutex_lock(&lock); \
	printf(__VA_ARGS__); \
	pthread_mutex_unlock(&lock);

pthread_t t1;
pthread_t t2;
pthread_t t3;

pthread_t my_threads[10];
void *my_retvals[10];

__thread int my_id;
void *thread(void* arg)
{	
	for (int i = 0; i < 10; i++) {
		printf_safe("[A] pthread %d on vcore %d\n", pthread_self()->id, vcore_id());
		pthread_yield();
		printf_safe("[A] pthread %d returned from yield on vcore %d\n",
		            pthread_self()->id, vcore_id());
	}
	return (void*)(pthread_self()->id);
}

int main(int argc, char** argv) 
{
	void *retval1 = 0;
	void *retval2 = 0;

	printf_safe("[A] About to create thread 1\n");
	pthread_create(&t1, NULL, &thread, NULL);
	printf_safe("[A] About to create thread 2\n");
	pthread_create(&t2, NULL, &thread, NULL);
	printf_safe("About to create thread 3\n");
	pthread_create(&t3, NULL, &thread, NULL);
	while(1);

	while (1) {
		for (int i = 1; i < 10; i++) {
			printf_safe("[A] About to create thread %d\n", i);
			pthread_create(&my_threads[i], NULL, &thread, NULL);
		}
		for (int i = 1; i < 10; i++) {
			printf_safe("[A] About to join on thread %d\n", i);
			pthread_join(my_threads[i], &my_retvals[i]);
			printf_safe("[A] Successfully joined on thread %d (retval: %p)\n", i,
			            my_retvals[i]);
		}
	}

	printf_safe("[A] About to create thread 1\n");
	pthread_create(&t1, NULL, &thread, NULL);
	printf_safe("[A] About to create thread 2\n");
	pthread_create(&t2, NULL, &thread, NULL);
	printf_safe("About to create thread 3\n");
	pthread_create(&t3, NULL, &thread, NULL);

	printf_safe("[A] Vcore0 spinning\n");
	printf_safe("[A] About to join on thread 1\n");
	pthread_join(t1, &retval1);
	printf_safe("[A] Successfully joined on thread 1 (retval: %p)\n", retval1);
	printf_safe("[A] About to join on thread 2\n");
	pthread_join(t2, &retval2);
	printf_safe("[A] Successfully joined on thread 2 (retval: %p)\n", retval2);
	printf_safe("About to join on thread 3\n");
	pthread_join(t3, NULL);
	while(1);
} 
