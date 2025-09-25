#include "cache.h"
#include "math.h"

Cache::Cache(uint32_t blk_size_p, uint32_t cache_size_p, uint32_t assoc_p){
    block_size = blk_size_p;
    cache_size = cache_size_p;
    associativity = assoc_p;
    calc_cache_properties();
    initialize_cache();
}

void Cache::calc_cache_properties(){
    number_of_sets = cache_size/(block_size * associativity);
    index_bits = log2(number_of_sets);
    block_offset_bits = log2(block_size);
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
}

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
        for (uint32_t colm = 0; colm < associativity; colm++)
        {
            printf("row = %u : %x \t",row,cache[row][colm].memory_block);
        }
        printf("\n");
    }
} 