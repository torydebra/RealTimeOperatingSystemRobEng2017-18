//Assigment1: RM with polling server for aperiodic thread
//gcc RM_PollingServer.c -lpthread
//sudo ./a.out

/*
 * Poll server take min period among all periodic task (that is period[0])
 * and capacity such that U (including the server) is the maximum possible 
 * 		(max possible is Ulub (not considering that Ulub =1 when period are multipliers) )
 
 * Polling server has the max priority according to RM because has minimum period 
 * (indeed the same min period as task 1)
 * 
 * Polling server can execute more times the aperiodic tasks, until it exausts the capacity
 
 * Polling server run until task 1 run (the only one who generate aperiodic event)
 
 * Aperiodic are considered without deadline
 * (if they have deadline = next arrival of same task, we need only to check
 * if aperiodicArrival[0] and aperiodicArrival[1] is greatear than 1. 
 * If so, aperiodic task are overlapping so we have to increase deadline_missed[] for that task)
 * */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sched.h>

// 1 2 3 periodic 4 5 aperiodic
void task1_code();
void task2_code();
void task3_code();
void task4_code();
void task5_code();
void *task1( void *);
void *task2( void *);
void *task3( void *);
void *polling_server(void *);

#define NTASK 5
#define NPERIODICTASKS 3
#define NAPERIODICTASKS 2

long int periodi[NPERIODICTASKS];
long int pollServerPeriod;
double pollServerCapacity;

struct timeval next_pronto[NPERIODICTASKS];
struct timeval pollServerNext_pronto;
long int tempodicalcolo[NTASK];

pthread_attr_t attributi[NPERIODICTASKS];
pthread_t thread_id[NPERIODICTASKS];
pthread_attr_t pollServerAttributi;
pthread_t pollServerId;
struct sched_param parametri[NPERIODICTASKS];
struct sched_param pollServerParametri;

int deadline_missed[NTASK];
int aperiodicArrival[NAPERIODICTASKS];
int polServerEnabled;


int main()
{
	//thread periods
	periodi[0]= 10000000; //in nanosecondi
	periodi[1]= 20000000;//in nanosecondi
	periodi[2]= 50000000; //in nanosecondi

	//server period minimum (at least equal) among all
	//to have max priority with RM
	pollServerPeriod = periodi[0];

	//set to zero the count of aperiodic arrivals
	for(int j =0; j<NAPERIODICTASKS; j++){
		aperiodicArrival[j] = 0;
	}
	polServerEnabled = 1; //server is enabled
	
	for (int j =0; j < NTASK; j++){
		deadline_missed[j]=0;
	}

	struct sched_param priomax;
	priomax.sched_priority=sched_get_priority_max(SCHED_FIFO);
	struct sched_param priomin;
	priomin.sched_priority=sched_get_priority_min(SCHED_FIFO);

	// max priority to main (for now)
	if (getuid() == 0){
		pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomax);
	}

	struct timeval timeval1;
	struct timezone timezone1;
	struct timeval timeval2;
	struct timezone timezone2;

	//exec task i to stimate execution time
	for (int i =0; i < NPERIODICTASKS; i++)
	{
		gettimeofday(&timeval1, &timezone1);

		if (i==0)
			task1_code();
		if (i==1)
			task2_code();
		if (i==2)
			task3_code();

		gettimeofday(&timeval2, &timezone2);

		tempodicalcolo[i]= 1000*((timeval2.tv_sec - timeval1.tv_sec)*1000000
			       +(timeval2.tv_usec-timeval1.tv_usec));
		printf("\nExecution time for task %d :%ld\n", i+1, tempodicalcolo[i]);
	}

	//calculate U of periodic task
	double U = 0.0;
	for (int i = 0; i<NPERIODICTASKS; i++){
		U += ((double)tempodicalcolo[i])/((double)periodi[i]);
	}

	//compute Ulub (period not multiply among them)
	double Ulub = 0.0;
	Ulub = NPERIODICTASKS*( pow(2.0, (1.0)/NPERIODICTASKS) -1 ); 
	
	if (U > Ulub)
	{
		printf("\n U=%lf Ulub=%lf Task periodici non schedulabili\n", U, Ulub);
		return(-1);

	} else {
		printf("\n U=%lf Ulub=%lf Task periodici schedulabili\n", U, Ulub);
	}

	//give capacity of server such that total utilization reach Ulub
	pollServerCapacity = (Ulub-U)*pollServerPeriod;
	printf("[poll Server]\nCapacity: %lf\n", pollServerCapacity);

	//exec aperiodic task i to stimate execution time
	for (int i =NPERIODICTASKS; i < NTASK; i++)
	{
		gettimeofday(&timeval1, &timezone1);
		if (i==3)
			task4_code();
		if (i==4)
			task5_code();
		gettimeofday(&timeval2, &timezone2);

		tempodicalcolo[i]= 1000*((timeval2.tv_sec - timeval1.tv_sec)*1000000
			       +(timeval2.tv_usec-timeval1.tv_usec));
		printf("\nExecution time for task %d :%ld\n", i+1, tempodicalcolo[i]);

		//integer division plus 1 because we need integer executions of
		//poll server
		int count = (tempodicalcolo[i]/pollServerCapacity)+1;
		printf("extimate number of server period needed: %d\n", count);
	}

	fflush(stdout);
	sleep(5);


	if (getuid() == 0) // set min priorità per padre (main)
		pthread_setschedparam(pthread_self(),SCHED_FIFO, &priomin);

	for (int i =0; i < NPERIODICTASKS; i++)
	{
		pthread_attr_init(&(attributi[i]));

		pthread_attr_setinheritsched(&(attributi[i]), PTHREAD_EXPLICIT_SCHED);
		//imposta politica real-time fifo
		pthread_attr_setschedpolicy(&(attributi[i]), SCHED_FIFO);

		//first task +3, second +2, third +1
		parametri[i].sched_priority = priomin.sched_priority+NPERIODICTASKS - i;
		pthread_attr_setschedparam(&(attributi[i]), &(parametri[i]));
	}

	pthread_attr_init(&(pollServerAttributi));
	pthread_attr_setinheritsched(&(pollServerAttributi), PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&(pollServerAttributi), SCHED_FIFO);
	
	//max priority to server because it is the most frequent
	//(+4 because 3 periodic task plus this server)
	pollServerParametri.sched_priority = priomin.sched_priority+(NPERIODICTASKS+1);
	pthread_attr_setschedparam(&(pollServerAttributi), &(pollServerParametri));

	int iret[NPERIODICTASKS];
	int iretPolServer;
	struct timeval ora;
	struct timezone zona;
	gettimeofday(&ora, &zona);

	//first instant in which task i is ready
	for (int i = 0; i < NPERIODICTASKS; i++)
	{
		long int periodi_micro = periodi[i]/1000;
		next_pronto[i].tv_sec = ora.tv_sec + periodi_micro/1000000;
		next_pronto[i].tv_usec = ora.tv_usec + periodi_micro%1000000;
		deadline_missed[i] = 0;
	}

	//first instant in which server is ready
	long int periodi_micro = pollServerPeriod/1000;
	pollServerNext_pronto.tv_sec = ora.tv_sec + periodi_micro/1000000;
	pollServerNext_pronto.tv_usec = ora.tv_usec + periodi_micro%1000000;

	iret[0] = pthread_create( &(thread_id[0]), &(attributi[0]), task1, NULL);
	iret[1] = pthread_create( &(thread_id[1]), &(attributi[1]), task2, NULL);
	iret[2] = pthread_create( &(thread_id[2]), &(attributi[2]), task3, NULL);
	//no need to create thread for aperiodic: is the polling server which executes them

	iretPolServer = pthread_create( &(pollServerId), &(pollServerAttributi), polling_server, NULL);

	/* Wait till threads are complete before main continues. Unless we */
	/* wait we run the risk of executing an exit which will terminate */
	/* the process and all threads before the threads have completed. */
	pthread_join( thread_id[0], NULL);
	pthread_join( thread_id[1], NULL);
	pthread_join( thread_id[2], NULL);
	pthread_join( pollServerId, NULL);
	printf ("\n");

	for(int i =0 ; i<NPERIODICTASKS; i++){
		printf ("thread periodic %d deadline missed: %d\n", i+1, deadline_missed[i]);
	}
	for(int i = NPERIODICTASKS ; i<NTASK; i++){
		printf ("thread aperiodic %d deadline missed: %d\n", i+1, deadline_missed[i]);
	}

	return 0;
}


void task1_code()
{
	int i,j;
	int uno;
	for (i = 0; i < 10; i++)
	{
		for (j = 0; j < 10000; j++)
		{
			uno = (rand());
    	}
	}
	printf("[1");

	// when the random variable uno<(1/10 RANDMAX), then aperiodic task 4 must
	// be executed
	if (uno < (RAND_MAX/10))
    {
		printf(":4 must be execute by next poll server]\n");fflush(stdout);
		aperiodicArrival[0]++;
    }

	// when the random variable uno>(14/15 RANDMAX), then aperiodic task 5 must
	// be executed
	else if (uno > ((RAND_MAX/15)*14))
    {
		printf(":5 must be executed by next poll server]\n");fflush(stdout);
		aperiodicArrival[1]++;
    } else {
		printf("] ");
	}
	fflush(stdout);
}

void *task1( void *ptr)
{
	int i=0;
	struct timespec waittime;
	waittime.tv_sec=0; /* seconds */
	waittime.tv_nsec = periodi[0]; /* nanoseconds */

	//multithread processor thing
	cpu_set_t cset;
	CPU_ZERO( &cset );
	CPU_SET( 0, &cset);
	pthread_setaffinity_np( pthread_self(), sizeof(cpu_set_t), &cset);

	//exec 100 times
	for (i=0; i < 100; i++)
    {
		task1_code();

		struct timeval ora;
		struct timezone zona;
		gettimeofday(&ora, &zona);

		//finished execution, calculate how much time remains until next period
		long int timetowait= 1000*((next_pronto[0].tv_sec - ora.tv_sec)*1000000
				 +(next_pronto[0].tv_usec-ora.tv_usec));

		if (timetowait <0)
			deadline_missed[0]++;

		waittime.tv_sec = timetowait/1000000000;
		waittime.tv_nsec = timetowait%1000000000;

		//put task in waiting until next period
		nanosleep(&waittime, NULL);

		//task again ready
		//calculate next arrival of task1
		long int periodi_micro=periodi[0]/1000;
		next_pronto[0].tv_sec = next_pronto[0].tv_sec +
			periodi_micro/1000000;
		next_pronto[0].tv_usec = next_pronto[0].tv_usec +
			periodi_micro%1000000;
    }

    //when task1 ended, polling server does not need anymore
    polServerEnabled = 0;
}


void task2_code()
{
	int i;
	for (i = 0; i < 10; i++)
    {
		int j;
		for (j = 0; j < 10000; j++)
		{
			double uno = rand()*rand();
		}
    }

	printf("[2] ");
	fflush(stdout);
}


void *task2( void *ptr )
{
	int i=0;
	struct timespec waittime;
	waittime.tv_sec=0; /* seconds */
	waittime.tv_nsec = periodi[1]; /* nanoseconds */

	cpu_set_t cset;
	CPU_ZERO( &cset );
	CPU_SET( 0, &cset);
	pthread_setaffinity_np( pthread_self(), sizeof(cpu_set_t), &cset);

	for (i=0; i < 100; i++)
    {
		task2_code();
		struct timeval ora;
		struct timezone zona;
		gettimeofday(&ora, &zona);
		long int timetowait= 1000*((next_pronto[1].tv_sec -
				  ora.tv_sec)*1000000 +(next_pronto[1].tv_usec-ora.tv_usec));

		if (timetowait <0)
			deadline_missed[1]++;

		waittime.tv_sec = timetowait/1000000000;
		waittime.tv_nsec = timetowait%1000000000;
		nanosleep(&waittime, NULL);
		long int periodi_micro=periodi[1]/1000;
		next_pronto[1].tv_sec = next_pronto[1].tv_sec + periodi_micro/1000000;
		next_pronto[1].tv_usec = next_pronto[1].tv_usec + periodi_micro%1000000;
    }
}


void task3_code()
{
	int i;
	for (i = 0; i < 10; i++)
    {
		int j;
		for (j = 0; j < 10000; j++);
		double uno = rand()*rand();
    }
	printf("[3] ");
	fflush(stdout);
}

void *task3( void *ptr)
{
	int i=0;
	struct timespec waittime;
	waittime.tv_sec=0; /* seconds */
	waittime.tv_nsec = periodi[2]; /* nanoseconds */

	cpu_set_t cset;
	CPU_ZERO( &cset );
	CPU_SET( 0, &cset);
	pthread_setaffinity_np( pthread_self(), sizeof(cpu_set_t), &cset);

	for (i=0; i < 100; i++)
    {
		task3_code();
		struct timeval ora;
		struct timezone zona;
		gettimeofday(&ora, &zona);
		long int timetowait= 1000*((next_pronto[2].tv_sec - ora.tv_sec)*1000000
				 +(next_pronto[2].tv_usec-ora.tv_usec));

		if (timetowait <0)
			deadline_missed[2]++;

		waittime.tv_sec = timetowait/1000000000;
		waittime.tv_nsec = timetowait%1000000000;
		nanosleep(&waittime, NULL);
		long int periodi_micro=periodi[2]/1000;
		next_pronto[2].tv_sec = next_pronto[2].tv_sec +
			periodi_micro/1000000;
		next_pronto[2].tv_usec = next_pronto[2].tv_usec +
			periodi_micro%1000000;
    }
}


void task4_code()
{
	double uno;
	for (int i = 0; i < 10; i++)
		{
			for (int j = 0; j < 100; j++){
			uno = rand()*rand();
		}
    }

	printf("[aperiodic 4] ");
	fflush(stdout);
}


void task5_code()
{
	double uno;
	for (int i = 0; i < 1000; i++)
	{
		for (int j = 0; j < 10; j++){
			uno = rand()*rand();
		}
	}

	printf("[aperiodic 5] ");
	fflush(stdout);
}


void pollServer_code()
{

	struct timeval now;
	struct timeval startPeriod;
	struct timezone zone;
	double timePassed;
	int noMoreTime[2];
	noMoreTime[0] = 0;
	noMoreTime[1] = 0;
	gettimeofday(&startPeriod,&zone);

	printf("[polenrty] ");

	//exec only if theare are some aperiodic task to execute
	while ( aperiodicArrival[0] > 0 || aperiodicArrival[1] > 0){

		if (aperiodicArrival[0] > 0){ //task 4 must be executed

			gettimeofday(&now,&zone);
			timePassed = 1000*((now.tv_sec - startPeriod.tv_sec)*1000000
				       +(now.tv_usec - startPeriod.tv_usec));

			//tempodicalcolo of aperiodic task + timePassed from beginning of execution
			//of server must be less than total capacity to continue execution
			if (timePassed >= pollServerCapacity - tempodicalcolo[3]){
				//then no more time to execute task4
				noMoreTime[0] = 1;
				//if also noMoreTime for task5... finish execution of polling server
				if (noMoreTime[1] == 1){
					return;
				}
			}
			task4_code();
			aperiodicArrival[0]--;
		}

		if (aperiodicArrival[1] > 0){

			gettimeofday(&now,&zone);
			timePassed = 1000*((now.tv_sec - startPeriod.tv_sec)*1000000
				       +(now.tv_usec - startPeriod.tv_usec));

			if (timePassed >= pollServerCapacity - tempodicalcolo[4]){
				//then no more time to execute task5
				noMoreTime[1] = 1;
				if (noMoreTime[0] == 1){
					return;
				}
			}
			task5_code();
			aperiodicArrival[1]--;
		}
	}
	printf("[polfinish] ");
	fflush(stdout);
}


void *polling_server( void *ptr)
{

	struct timespec waittime;
	waittime.tv_sec=0; /* seconds */
	waittime.tv_nsec = periodi[0]; /* nanoseconds */

	//multithread processor thing
	cpu_set_t cset;
	CPU_ZERO( &cset );
	CPU_SET( 0, &cset);
	pthread_setaffinity_np( pthread_self(), sizeof(cpu_set_t), &cset);

	//exec for ever (it will be stopped after task1 (the only who produces aperiodic
	//task) has finished)
	while(1)
    {
		//if task1 has finished and no more aperiodic requests remain
		//polling server does not need anymore
		if (polServerEnabled == 0 &&
			aperiodicArrival[0] == 0 &&
			aperiodicArrival[1] == 0){

			break;
		}

		pollServer_code(pollServerNext_pronto);

		struct timeval ora;
		struct timezone zone;
		gettimeofday(&ora, &zone);

		//finished execution, calculate how much time remains until next period
		long int timetowait= 1000*((pollServerNext_pronto.tv_sec - ora.tv_sec)*1000000
				 +(pollServerNext_pronto.tv_usec-ora.tv_usec));

		waittime.tv_sec = timetowait/1000000000;
		waittime.tv_nsec = timetowait%1000000000;

		//mette il task in attesa fino al periodo successivo
		nanosleep(&waittime, NULL);

		//server again ready
		//calculate next arrival of server
		long int periodi_micro=pollServerPeriod/1000;
		pollServerNext_pronto.tv_sec = pollServerNext_pronto.tv_sec +
			periodi_micro/1000000;
		pollServerNext_pronto.tv_usec = pollServerNext_pronto.tv_usec +
			periodi_micro%1000000;
    }
}
