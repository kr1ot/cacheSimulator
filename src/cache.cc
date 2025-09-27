#include "cache.h"
#include "math.h"

Cache::Cache(uint32_t blk_size_p, uint32_t cache_size_p, uint32_t assoc_p){
    block_size = blk_size_p;
    cache_size = cache_size_p;
    associativity = assoc_p;
    next_mem_hier = nullptr;
    initialize_cache_params();
    //do not generate cache if cache size = 0
    if (cache_size != 0){
        calc_cache_properties();
        generate_cache();
    }
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

void Cache::generate_cache(){
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

void Cache::initialize_cache_params(){

    //initialize the cache measurements with 0
    cache_measurements.reads = 0;
    cache_measurements.read_misses = 0;
    cache_measurements.writes = 0;
    cache_measurements.write_misses = 0;
    cache_measurements.miss_rate = 0;
    cache_measurements.write_backs = 0;
    cache_measurements.prefetches = 0;
}

bool Cache::is_miss(uint32_t tag, uint32_t index){
    bool miss = true;
    //check the tag in the cache
    //loop through the columns for a given set
    for (uint32_t column = 0; column < associativity; column++)
    {
        //check if the address is present and valid flag = 1 for a hit
        if ((cache[index][column].memory_block == tag) && (cache[index][column].valid_flag == 1)) {
            //return false if it hits
            miss = false;
            //do not iterate once memory block is found
            break;
        }
    }
    //else miss = true
    return miss;
}

void Cache::evict_and_update_lru(uint32_t tag, uint32_t lru_count_to_replace, uint32_t index,char r_w){
    //in the cache perform the eviction and update the lru counters
    
    //run through all the columns for a given index/set
    for (uint32_t colms = 0;colms < associativity; colms++)
    { 
        //increment the lru_counter if lesser than the lru_counter of value
        //of the required memory block
        if(cache[index][colms].lru_counter < lru_count_to_replace)
        {
            cache[index][colms].lru_counter += 1;
        }
        //reset the lru counter to 0, if it hits the required count
        //this indicates the requested memory block is MRU
        else if(cache[index][colms].lru_counter == lru_count_to_replace)
        {
            cache[index][colms].lru_counter = 0;
            cache[index][colms].memory_block = tag;
            //update the valid flag
            cache[index][colms].valid_flag = 1;
            //update the radio flags based on r/w
            if (r_w == 'w')
            {
                cache[index][colms].dirty_flag = 1;
            }
        }
        //keep the value of lru_counter same for counter values greater than 
        //required count value

    }
}

//handle the read or write request to the given cache
void Cache::request(uint32_t addr, char r_w){
    bool miss = true;
    uint32_t index = get_index(addr = addr);
    uint32_t tag = get_tag(addr = addr);
    uint32_t lru_count_to_be_evicted = 0;

    //Debugs-
    // printf("%x\n",addr);

    //check if the memory blocks misses in cache
    miss = is_miss(tag = tag, index = index);

    //check if this is the last memory strcture
    if (next_mem_hier == nullptr)
    {
        //for read requests from upper hierarchy
        if (r_w == 'r')
        {
            // printf("\t r: %x (tag = %x index=%u)\n",addr,tag,index);
            // printf("\t before: set %u: %x\n",index,tag);
            cache_measurements.reads += 1;
            //check if the read request is a miss
            if (miss == true)
            {
                cache_measurements.read_misses += 1;
                //get the column whose lru_counter = associativity - 1
                for (uint32_t colm =0; colm < associativity; colm++)
                {
                    //check before eviction, if the memory block at LRU was dirty
                    //dirty = 1 -> write back to main memory
                    if (cache[index][colm].lru_counter == (associativity-1))
                    {
                        if (cache[index][colm].dirty_flag == 1){
                            cache_measurements.write_backs += 1;
                            cache[index][colm].dirty_flag = 0;
                        }
                        break;
                    }
                }
                
                lru_count_to_be_evicted = associativity-1;
            }
            //if the memory is hit in cache
            else 
            {
                //get the lru counter value of current hit block
                for (uint32_t colms=0; colms < associativity; colms++)
                {
                    if (cache[index][colms].memory_block == tag){
                        lru_count_to_be_evicted = cache[index][colms].lru_counter;
                        break;
                    }
                }
            }
            //no explicit read issued to memory in the simulator for miss/hit
            //update the memory block and LRU with the new block 
            evict_and_update_lru(tag,lru_count_to_be_evicted,index,'r');
            // printf("\t after: set %u: %x\n",index,tag);
        }
        //write request from upper hierarchy
        else
        {
            // printf("\t w: %x (tag = %x index=%u)\n",addr,tag,index);
            cache_measurements.writes += 1;
            // printf("\t before: set %u: %x\n",index,tag);
            //check if the write request is a miss
            if (miss == true)
            {
                cache_measurements.write_misses += 1;

                //get the column whose lru_counter = associativity - 1
                for (uint32_t colm =0; colm < associativity; colm++)
                {
                    //check before eviction, if the memory block at LRU was dirty
                    //dirty = 1 -> write back to main memory
                    if (cache[index][colm].lru_counter == (associativity-1))
                    {
                        if (cache[index][colm].dirty_flag == 1){
                            cache_measurements.write_backs += 1;
                            cache[index][colm].dirty_flag = 0;
                        }
                        break;
                    }
                }
                //eviction incase it was a write miss
                lru_count_to_be_evicted = associativity-1;
                //bring the block from memory incase of miss
                //no explicit read performed
            }
            //write hit
            else 
            {
                //get the lru counter value of current hit block
                for (uint32_t colms=0; colms < associativity; colms++)
                {
                    if (cache[index][colms].memory_block == tag){
                        lru_count_to_be_evicted = cache[index][colms].lru_counter;
                        break;
                    }
                }
            }
            evict_and_update_lru(tag,lru_count_to_be_evicted,index,'w');
            // printf("\t after: set %u: %x D\n",index,tag);
        }
    }
    
    //if next memroy hierarchy exists
    else
    {
        //for read requests from upper hierarchy
        if (r_w == 'r')
        {
            cache_measurements.reads += 1;
            //check if the read request is a miss
            if (miss == true)
            {
                cache_measurements.read_misses += 1;
                 //get the column whose lru_counter = associativity - 1
                for (uint32_t colm =0; colm < associativity; colm++)
                {
                    //check before eviction, if the memory block at LRU was dirty
                    //dirty = 1 -> write back to main memory
                    if (cache[index][colm].lru_counter == (associativity-1))
                    {
                        if (cache[index][colm].dirty_flag == 1)
                        {
                            cache_measurements.write_backs += 1;
                            next_mem_hier->request(addr,'w');
                            cache[index][associativity-1].dirty_flag = 0;
                        }
                        break;
                    }
                }
                //bring the memory block lower hierarchy irrespective of dirty flag
                next_mem_hier->request(addr,'r');
                lru_count_to_be_evicted = associativity-1;
            }
            //if the memory is hit in cache
            else 
            {
                //get the lru counter value of current hit block
                for (uint32_t colms=0; colms < associativity; colms++)
                {
                    if (cache[index][colms].memory_block == tag){
                        lru_count_to_be_evicted = cache[index][colms].lru_counter;
                    }
                }
            }
            //no explicit read issued to memory in the simulator for miss/hit
            //update the memory block and LRU with the new block 
            evict_and_update_lru(tag,lru_count_to_be_evicted,index,'r');
        }
        //write request from upper hierarchy
        else
        {
            cache_measurements.writes += 1;
            //check if the write request is a miss
            if (miss == true)
            {
                cache_measurements.write_misses += 1;
                //get the column whose lru_counter = associativity - 1
                for (uint32_t colm =0; colm < associativity; colm++)
                {
                    //check before eviction, if the memory block at LRU was dirty
                    //dirty = 1 -> write back to main memory
                    if (cache[index][colm].lru_counter == (associativity-1))
                    {
                        if (cache[index][colm].dirty_flag == 1)
                        {
                            cache_measurements.write_backs += 1;
                            next_mem_hier->request(addr,'w');
                            cache[index][associativity-1].dirty_flag = 0;
                        }
                        break;
                    }
                }
                //eviction incase it was a write miss
                lru_count_to_be_evicted = associativity-1;
                //bring the block from lower memory incase of miss
                next_mem_hier->request(addr,'r');
            }
            //write hit
            else 
            {
                //get the lru counter value of current hit block
                for (uint32_t colms=0; colms < associativity; colms++)
                {
                    if (cache[index][colms].memory_block == tag){
                        lru_count_to_be_evicted = cache[index][colms].lru_counter;
                    }
                }
            }
            evict_and_update_lru(tag,lru_count_to_be_evicted,index,'w');
        }
    }
}


void Cache::print_cache_contents()
{
    //caclulate miss rate
    cache_measurements.miss_rate = (float)(cache_measurements.read_misses + cache_measurements.write_misses)/(float)(cache_measurements.reads + cache_measurements.writes);
    // printf("Debugging:\n");
    // printf("read_miss = %u\n",cache_measurements.read_misses);
    // printf("write_miss = %u\n",cache_measurements.write_misses);
    // printf("reads = %u\n",cache_measurements.reads);
    // printf("writes = %u\n",cache_measurements.writes);
    // printf("miss rate = %f\n",cache_measurements.miss_rate);


    for (uint32_t set=0; set < number_of_sets; set++)
    {
        //a set is invalid if all the memory blocks are invalid
        bool set_invalid = true;
        //check if the set has atleast 1 way valid
        for (uint32_t colm=0; colm < associativity; colm++)
        {
            if (cache[set][colm].valid_flag == 1)
            {
                set_invalid = false;
                //no need to iterate through rest of the colms
                break;
            }
        }
        //if set is valid then print the set
        if (set_invalid == false)
        {
            printf("set     %2u: ",set);
        }
        //else break out of the loop. Do not print anything for the set
        else
        {
            break;
        }

        //print the cache based on MRU -> LRU
        //loop through the n-ways
        for (uint32_t lru_count = 0; lru_count < associativity; lru_count++)
        {
            for (uint32_t colm = 0; colm < associativity; colm++)
            {
                //print based on recency
                if ((cache[set][colm].lru_counter == lru_count) &&
                    (cache[set][colm].valid_flag == 1))
                {
                    //check if the bit is dirty
                    if (cache[set][colm].dirty_flag == 1){
                        // printf("   %x D\t (%u)",cache[set][colm].memory_block,cache[set][colm].lru_counter);
                        printf("  %x D",cache[set][colm].memory_block);
                    }
                    else{
                        printf("  %x  ",cache[set][colm].memory_block);
                    }
                    break;
                }
            }
        }
        printf("\n");
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
        printf("set = %u -> ",row);
        for (uint32_t colm = 0; colm < associativity; colm++)
        {
            printf("lru counter = %u : %x \t",cache[row][colm].lru_counter,cache[row][colm].memory_block);
        }
        printf("\n");
    }
} 