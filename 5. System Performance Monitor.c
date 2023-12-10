/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>  // Include pthread library
#include "system.h"
#include <sys/types.h>

static volatile int done;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 

static void
_signal_(int signum)
{
	assert( SIGINT == signum );
	done = 1;
}

double
cpu_util(const char *s)
{
	static unsigned sum_, vector_[7];
	unsigned sum, vector[7];
	const char *p;
	double util;
	uint64_t i;


	if (!(p = strstr(s, " ")) ||
	    (7 != sscanf(p,
			 "%u %u %u %u %u %u %u",
			 &vector[0],
			 &vector[1],
			 &vector[2],
			 &vector[3],
			 &vector[4],
			 &vector[5],
			 &vector[6]))) {
		return 0;
	}
	sum = 0.0;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		sum += vector[i];
	}
	util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
	sum_ = sum;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		vector_[i] = vector[i];
	}
	return util;
}

double
memory_util(FILE *file)
{
    char line[256];
    unsigned long total_memory, free_memory, buffers, cached;

    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "MemTotal: %lu kB", &total_memory) == 1)
        {
            continue;
        }
        else if (sscanf(line, "MemFree: %lu kB", &free_memory) == 1)
        {
            continue;
        }
        else if (sscanf(line, "Buffers: %lu kB", &buffers) == 1)
        {
            continue;
        }
        else if (sscanf(line, "Cached: %lu kB", &cached) == 1)
        {
            break;
        }
    }


    double utilized_memory = total_memory - free_memory - buffers - cached;
    double utilization_percentage = (utilized_memory / total_memory) * 100.0;

    fclose(file);

    return utilization_percentage;
}

void
get_process_details(FILE *file)
{
	char line[1024];
	char *p;
	char info_vector[4][1024] = {"", "", "", ""};

	while (fgets(line, sizeof(line), file))
	{
		if ((p = strstr(line, "Name:")))
		{
			sscanf(p, "%*s %s", info_vector[0]);
		}

		else if ((p = strstr(line, "Pid:")) && !strcmp(info_vector[1], ""))
		{
			sscanf(p, "%*s %s", info_vector[1]);
		}

		else if ((p = strstr(line, "VmSize:")))
		{
			sscanf(p, "%*s %s", info_vector[2]);
		}

		else if ((p = strstr(line, "Threads:")))
		{
			sscanf(p, "%*s %s", info_vector[3]);
		}
	}
    fclose(file);
	printf("\tProcessName: %s \t PID: %s \t VMem: %s \t Threads: %s", info_vector[0], info_vector[1], info_vector[2], info_vector[3]);
}


void get_network_details(FILE *file) {
     char *temp;
   
     
        char line[256];
     	char  vector [9][1024]={"","","","","","","","",""};

	while (fgets(line, sizeof(line), file))
	{
		if ((temp = strstr(line, "enp0s1:")))
		{
			sscanf(temp, "%*s %s %s %s %s %s %s %s %s %s ", vector[0],vector[1],vector[2], vector[3],vector[4],vector[5],vector[6],vector[7],vector[8]);
		}
	}
    fclose(file);

	printf("\t Bytes up/down: %s, %s",vector[0],vector[8]);
}

void *new_thread(void *args) {
    UNUSED(args);
    for(int i =0; i<5; i++){
    	us_sleep(50000);
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    const char *const PROC_STAT = "/proc/stat";
    const char* proc_net_path = "/proc/net/dev";
    const char *const PROC_MEMINFO = "/proc/meminfo";
    const char* const PROC = "/proc/self/status";
    char line[1024];
    FILE *file, *file_net, *file_proc, *file_mem;

    UNUSED(argc);
    UNUSED(argv);

    if (SIG_ERR == signal(SIGINT, _signal_))
    {
        TRACE("signal()");
        return -1;
    }

      pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, new_thread, NULL) != 0) {
        fprintf(stderr, "Error creating new thread.\n");
        return -1;
    }

    while (!done)
    {
        if (!(file = fopen(PROC_STAT, "r")) || !(file_net = fopen(proc_net_path, "r")) || !(file_proc = fopen(PROC, "r")) || !(file_mem = fopen(PROC_MEMINFO, "r")))
        {
            TRACE("fopen()");
            return -1;
        }

        if (fgets(line, sizeof(line), file))
        {
            pthread_mutex_lock(&mutex);
            printf("\rCPU Utilization: %5.1f%%\tMemory Utilization: %5.1f%%", cpu_util(line), memory_util(file_mem));
            get_process_details(file_proc);
            get_network_details(file_net);
            pthread_mutex_unlock(&mutex);
            fflush(stdout);
        }

        us_sleep(500000);
        fclose(file);
    }

     pthread_join(thread_id, NULL);

    printf("\rDone!   \n");
    return 0;
}
