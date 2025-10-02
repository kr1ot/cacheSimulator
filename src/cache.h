#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"


//radio bits associated with each memory block in cache
typedef struct 
{
   //using the smallest unsigned value -> 8bits
   //because the radio bits are 1 bit wide
   uint8_t valid_flag;   //set when a memory address is cached
   uint8_t dirty_flag;   //set when a write from CPU is performed and is yet not sent to lower memory/cache 
   uint32_t lru_counter; //counter tracking the lru value for the block. Used for replacement/eviction
   uint32_t memory_block; //memory block storing the tag in the cache
} cache_with_radio_bits_t;

//measurements for a given cache
typedef struct
{
    uint32_t reads;
    uint32_t read_misses;
    uint32_t writes;
    uint32_t write_misses;
    float miss_rate;
    uint32_t write_backs;
    uint32_t prefetches;
} cache_measurements_t;

//stream buffer associated with cache
typedef struct
{
    //=1 -> stream buffer is valid
    uint8_t valid_flag;
    //pointer to the start of the stream buffer
    uint32_t* ptr_to_stream_buffer;
    //lru counter to check the recency
    //0 -> MRU, N-1 -> LRU
    uint32_t lru_counter;
} stream_buffer_t;


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

    public:

        //2d array that emulates the cache of size #sets x Associativity
        //declaring using a double pointer
        cache_with_radio_bits_t** cache;
        //points to the next memory hierarchy
        Cache* next_mem_hier;
        //cache measurements for the read/write request
        cache_measurements_t cache_measurements;
        
        //stream buffer storing the prefetched address
        stream_buffer_t* stream_buffer;

        //------Function definitions------//

        //parameterized constructor
        //passing blocksize, cache size and associativity
        Cache(uint32_t, uint32_t, uint32_t);

        //debug function to check if I'm passing and using the correct values
        void display();
        void print_stream_buffer_contents(uint32_t, uint32_t);
        void print_cache_contents();
 
        //calculates all the cache properties 
        void calc_cache_properties();
 
        //calculate the tag from given address
        uint32_t get_tag(uint32_t);
 
        //calculate the index from given address
        uint32_t get_index(uint32_t);
 
        //initialize the cache with default values
        void generate_cache();
        void generate_stream_buffer(uint32_t, uint32_t);
        void initialize_cache_params();

        //handle the request from the upper level -> CPU/upper cache
        void request(uint32_t, char);

        bool is_miss(uint32_t, uint32_t);

        void evict_and_update_lru(uint32_t, uint32_t, uint32_t, char);
        uint32_t get_addr_from_tag_index(uint32_t, uint32_t);
};

#endif