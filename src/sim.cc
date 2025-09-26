#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include "cache.h"

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main (int argc, char *argv[]) {
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
                // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   // printf("\n");
   // printf("\n");
   

    Cache* cache_l1 = new Cache(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC);
    Cache* cache_l2 = new Cache(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC);

    // cache_l1->display();
    // cache_l2->display();
   //based on the L2 size paramenter, decide whether L2 is present
   bool l2_exists = false;

   if (params.L2_SIZE != 0)
   {
       l2_exists = true;
   }

   if (l2_exists == true)
   {
       
       cache_l1->next_mem_hier = cache_l2;
   }

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r')
         // printf("r %x\n", addr);
         cache_l1->request(addr,'r');
      else if (rw == 'w')
         // printf("w %x\n", addr);
         cache_l1->request(addr,'w');
      else {
         printf("Error: Unknown request type %c.\n", rw);
     exit(EXIT_FAILURE);
      }

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////
    }

   // Cache* L2 = new Cache(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC);
   // L1->next_mem_hier = L2;
    printf("===== L1 contents =====\n");
    cache_l1->print_cache_contents();
    uint32_t memory_traffic = 0;

    if (l2_exists == false)
    {
        memory_traffic = cache_l1->cache_measurements.write_backs + cache_l1->cache_measurements.read_misses + cache_l1->cache_measurements.write_misses;
        // printf("\n");
        // printf("===== Measurements =====\n");
        // printf("a. L1 reads:                   %u\n",cache_l1->cache_measurements.reads);
        // printf("b. L1 read misses:             %u\n",cache_l1->cache_measurements.read_misses);
        // printf("c. L1 writes:                  %u\n",cache_l1->cache_measurements.writes);
        // printf("d. L1 write misses:            %u\n",cache_l1->cache_measurements.write_misses);
        // printf("e. L1 miss rate:               %f\n",cache_l1->cache_measurements.miss_rate);
        // printf("f. L1 writebacks:              %u\n",cache_l1->cache_measurements.write_backs);
        // printf("g. L1 prefetches:              %u\n",cache_l1->cache_measurements.prefetches);
        // printf("h. L2 reads (demand):          0\n");
        // printf("i. L2 read misses (demand):    0\n");
        // printf("j. L2 reads (prefetch):        0\n");
        // printf("k. L2 read misses (prefetch):  0\n");
        // printf("l. L2 writes:                  0\n");
        // printf("m. L2 write misses:            0\n");
        // printf("n. L2 miss rate:               0.0000\n");
        // printf("o. L2 writebacks:              0\n");
        // printf("p. L2 prefetches:              0\n");
        // printf("q. memory traffic:             %u\n",memory_traffic);
    }
    else 
    {
        printf("\n");
        printf("===== L2 contents =====\n");
        memory_traffic = cache_l2->cache_measurements.write_backs + cache_l2->cache_measurements.read_misses + cache_l2->cache_measurements.write_misses;
        cache_l2->print_cache_contents();
    }

    printf("\n");
    printf("===== Measurements =====\n");
    printf("a. L1 reads:                   %u\n",cache_l1->cache_measurements.reads);
    printf("b. L1 read misses:             %u\n",cache_l1->cache_measurements.read_misses);
    printf("c. L1 writes:                  %u\n",cache_l1->cache_measurements.writes);
    printf("d. L1 write misses:            %u\n",cache_l1->cache_measurements.write_misses);
    printf("e. L1 miss rate:               %.4f\n",cache_l1->cache_measurements.miss_rate);
    printf("f. L1 writebacks:              %u\n",cache_l1->cache_measurements.write_backs);
    printf("g. L1 prefetches:              %u\n",cache_l1->cache_measurements.prefetches);
    printf("h. L2 reads (demand):          %u\n",cache_l2->cache_measurements.reads);
    printf("i. L2 read misses (demand):    %u\n",cache_l2->cache_measurements.read_misses);
    printf("j. L2 reads (prefetch):        0\n");
    printf("k. L2 read misses (prefetch):  0\n");
    printf("l. L2 writes:                  %u\n",cache_l2->cache_measurements.writes);
    printf("m. L2 write misses:            %u\n",cache_l2->cache_measurements.write_misses);
    printf("n. L2 miss rate:               %.4f\n",cache_l2->cache_measurements.miss_rate);
    printf("o. L2 writebacks:              %u\n",cache_l2->cache_measurements.write_backs);
    printf("p. L2 prefetches:              %u\n",cache_l2->cache_measurements.prefetches);
    printf("q. memory traffic:             %u\n",memory_traffic);

    // printf("tag of address = %x\n",L1->get_tag(addr=addr));
    // printf("index of address = %x\n",L1->get_index(addr=addr));
    return(0);
}
