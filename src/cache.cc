#include "cache.h"
#include "math.h"

Cache::Cache(uint32_t blk_size_p, uint32_t cache_size_p, uint32_t assoc_p){
    block_size = blk_size_p;
    cache_size = cache_size_p;
    associativity = assoc_p;
    next_mem_hier = nullptr;
    calc_cache_properties();
    initialize_cache();
}

void Cache::calc_cache_properties(){
    //get the number of sets
    number_of_sets = cache_size/(block_size * associativity);
    //bits required to represent index
    index_bits = log2(number_of_sets);
    //bits required to represent block
    block_offset_bits = log2(block_size);
    //bits required to represent tag
    tag_bits = ADDR_BIT_WIDTH - index_bits - block_offset_bits;
}

uint32_t Cache::get_tag(uint32_t addr){
    return (addr >> (block_offset_bits + index_bits));
}

uint32_t Cache::get_index(uint32_t addr){
    //create a mask for extracting index
    uint32_t index_mask = pow(2,index_bits) - 1;
    //remove block offset and then mask with index_mask
    //it gives the index for a given address
    return ((addr >> block_offset_bits) & index_mask);
}

void Cache::initialize_cache(){
    //create the cache 
    //dynamically allocate cache with rows = number of sets
    cache = new cache_with_radio_bits_t*[number_of_sets];
    //loop through each row and dynamically allocate the columns = associativity
    for (uint32_t rows = 0; rows < number_of_sets; rows++){
        cache[rows] = new cache_with_radio_bits_t[associativity];
    }

    //initialize the memory with 0 and assign the radio flags
    for (uint32_t rows = 0; rows < number_of_sets; rows++)
    {
        for (uint32_t colms = 0; colms < associativity; colms++)
        {
            cache[rows][colms].memory_block = 0;
            cache[rows][colms].dirty_flag = 0;
            cache[rows][colms].valid_flag = 0;
            //assigning the lru counter with the colms 
            //this gives MRU -> LRU for n-way associative
            cache[rows][colms].lru_counter = colms;
        }
    }

    //initialize the cache measurements with 0
    cache_measurements.reads = 0;
    cache_measurements.read_misses = 0;
    cache_measurements.writes = 0;
    cache_measurements.write_misses = 0;
    cache_measurements.miss_rate = 0;
    cache_measurements.write_backs = 0;
    cache_measurements.prefetches = 0;
}

bool Cache::is_miss(uint32_t addr){
    bool miss = true;
    uint32_t index = get_index(addr = addr);
    //check the address in the cache
    
    //loop through the columns for a given set
    for (int column = 0; column < associativity; column++)
    {
        //check if the address is present and valid flag = 1 for a hit
        if ((cache[index][column].memory_block == addr) && (cache[index][column].valid_flag == 1)) {
            //return false if it hits
            miss = false;
            //do not iterate once memory block is found
            break;
        }
    }
    //else miss = true
    return miss;
}

//handle the read or write request to the given cache
void Cache::request(uint32_t addr, char r_w){
    bool miss = true;
    uint32_t index = get_index(addr = addr);

    //check if the memory blocks misses in cache
    miss = is_miss(addr);

    
    //for read requests
    if (r_w == 'r')
    {
        cache_measurements.reads += 1;
        //for misses
        if(miss == true)
        {
            cache_measurements.read_misses += 1;
            //check for the LRU in a given set
            //if dirty bit = 1-> write back to next mem hierarchy
            if (cache[index][associativity-1].dirty_flag == 1)
            {
                //increment the write back measurement
                cache_measurements.write_backs += 1;
                if (next_mem_hier != nullptr){
                    next_mem_hier->request(addr,'w');
                } 
            }
            //no need for write back if dirty flag = 0
            //issue a read request to the next hierarchy
            if (next_mem_hier != nullptr){
                next_mem_hier->request(addr, 'r');
            }
            
        }

    }
}
    //for write requests


void Cache::display(){
    printf("block_size = %u\n",block_size);
    printf("cache_size = %u\n",cache_size);
    printf("associativity = %u\n",associativity); 
    printf("number of sets = %u\n",number_of_sets);
    printf("number of index_bits = %u\n",index_bits);
    printf("number of block_offset_bits = %u\n",block_offset_bits);
    printf("number of tag_bits = %u\n",tag_bits);  

    printf("Cache contents: \n");
    //print the cache 
    for (uint32_t row=0; row < number_of_sets; row++)
    {
        printf("set = %u -> ",row);
        for (uint32_t colm = 0; colm < associativity; colm++)
        {
            printf("lru counter = %u : %x \t",cache[row][colm].lru_counter,cache[row][colm].memory_block);
        }
        printf("\n");
    }
} 