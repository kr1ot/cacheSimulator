#ifndef SIM_CACHE_H
#define SIM_CACHE_H

// cache parameters passed from command line
typedef 
struct {
   uint32_t BLOCKSIZE;    //Blocksize common for both L1 and L2
   uint32_t L1_SIZE;      //L1 cache total size
   uint32_t L1_ASSOC;     //L1 associativity. 1-> direct mapped, size/block_size = 1-> fully associative
   uint32_t L2_SIZE;      //L2 cache total size
   uint32_t L2_ASSOC;     //L2 associativity. 1-> direct mapped, size/block_size = 1-> fully associative
   uint32_t PREF_N;       //Prefetch unit number. 0-> no prefetch unit, +ve int -> number of prefetch units
   uint32_t PREF_M;       //Depth of each prefetch unit
} cache_params_t; 

// Put additional data structures here as per your requirement.



#endif
