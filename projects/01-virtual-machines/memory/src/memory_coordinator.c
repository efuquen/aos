#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

int is_exit = 0; // DO NOT MODIFY THE VARIABLE

void MemoryScheduler(virConnectPtr conn, int interval);

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
int main(int argc, char *argv[])
{
	virConnectPtr conn;

	if (argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	signal(SIGINT, signal_callback_handler);

	while (!is_exit)
	{
		// Calls the MemoryScheduler function after every 'interval' seconds
		MemoryScheduler(conn, interval);
		sleep(interval);
	}

	// Close the connection
	virConnectClose(conn);
	return 0;
}

/*
COMPLETE THE IMPLEMENTATION
*/
void MemoryScheduler(virConnectPtr conn, int interval)
{
	virDomainPtr *domains;
	size_t i;
	int ret;
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_RUNNING |
						 VIR_CONNECT_LIST_DOMAINS_PERSISTENT;
	ret = virConnectListAllDomains(conn, &domains, flags);
	if (ret < 0) {
		fprintf(stderr, "Failed to list domains\n");
		return;
	}

	

	for (i = 0; i < ret; i++)
	{
		if (virDomainSetMemoryStatsPeriod(domains[i], interval, VIR_DOMAIN_AFFECT_LIVE) < 0) {
			fprintf(stderr, "Failed to set memory stats period.");
			return;
        }
		double max_mem_mb = (double) virDomainGetMaxMemory(domains[i]) / 1024;
		printf("Domain %s Max Memory: %.2f MB\n", virDomainGetName(domains[i]), max_mem_mb);

		virDomainMemoryStatStruct stats[VIR_DOMAIN_MEMORY_STAT_NR];
        int nr_stats = virDomainMemoryStats(domains[i], stats, VIR_DOMAIN_MEMORY_STAT_NR, 0);

        if (nr_stats < 0) {
            fprintf(stderr, "Error fetching memory stats.\n");
            break;  
        }

        long long available = -1, usable = -1;//, unused = -1;

        // 2. Parse the returned stats array
        for (int i = 0; i < nr_stats; ++i) {
            if (stats[i].tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE)
                available = stats[i].val;
            if (stats[i].tag == VIR_DOMAIN_MEMORY_STAT_USABLE)
                usable = stats[i].val;
            /*if (stats[i].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED)
                unused = stats[i].val;*/
        }

        // 3. Display calculations (Values are in KiB)
        if (available != -1 && usable != -1) {
            double used_mb = (double)(available - usable) / 1024;
            double total_mb = (double)available / 1024 ;
			double pct      = (used_mb / total_mb) * 100.0;

			printf("Domain %s Memory Usage: %.2f MB / %.2f MB [%.1f%%]\n", virDomainGetName(domains[i]), used_mb, total_mb, pct);
            fflush(stdout); // Keep line updated in-place using \r
        } else {
			printf("\rWaiting for balloon driver telemetry...");
            fflush(stdout);
        }

		virDomainFree(domains[i]);
	}
	free(domains);
}