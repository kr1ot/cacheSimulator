#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"

//typedef structure
//pointers to the current memory address and next memory hierarchy
typedef struct 
{  
   uint32_t* cache_base_addr_ptr; //pointer to the current cache base addr
   uint32_t* next_mem_hierarchy_ptr; //pointer to next memory hierarchy
} cache_t;

//radio bits associated with each memory block in cache
typedef struct 
{
   //using the smallest unsigned value -> 8bits
   //because the radio bits are 1 bit wide
   uint8_t valid_flag;   //set when a memory address is cached
   uint8_t dirty_flag;   //set when a write from CPU is performed and is yet not sent to lower memory/cache 
   uint32_t lru_counter; //counter tracking the lru value for the block. Used for replacement/eviction
   uint32_t memory_block; //memory block storing the address in the cache
} cache_with_radio_bits_t;


//Cache class -> 
//  constructor (size, blocksize, associativity)
//  initializes member variables:
//  - #sets in the cache
//  - bits for block offset

//Contains-
// 1) getTag(addr) -> gets the tag from the memory address
// 2) getIndex(addr) -> gets the index from memory address
// 3) request("r/w",addr) -> performs the read or write request

class Cache {
    private:
        uint8_t ADDR_BIT_WIDTH = 32;
        uint32_t block_size;    //block size 
        uint32_t cache_size;    //cache size 
        uint32_t associativity; //associativity 
        
        //cache properties
        uint8_t number_of_sets; //number of sets associated to cache
        uint8_t tag_bits;       //number of bits used for tags    
        uint8_t index_bits;     //number of bits used for index
        uint8_t block_offset_bits; //number of bits used for block offset

        //2d array that emulates the cache of size #sets x Associativity
        //declaring using a double pointer
        cache_with_radio_bits_t **cache;
    public:
        //Function definitions

        //parameterized constructor
        //passing blocksize, cache size and associativity
        Cache(uint32_t, uint32_t, uint32_t);
        // Cache (uint32_t blk_size, uint32_t cache_size, uint32_t assoc) :
        //     block_size(blk_size),
        //     cache_size(cache_size),
        //     associativity(assoc) {}

        //debug function to check if I'm passing and using the correct values
        void display();
        //calculates all the cache properties 
        void calc_cache_properties();
        //calculate the tag from given address
        uint32_t get_tag(uint32_t);
        //calculate the index from given address
        uint32_t get_index(uint32_t);
        //initialize the cache with default values
        void initialize_cache();


};

#endif