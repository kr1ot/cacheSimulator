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

void Cache::generate_stream_buffer(uint32_t number_of_stream_buffers, uint32_t depth_of_stream_buffer){

    //assign the class variables
    this->number_of_stream_buffers = number_of_stream_buffers;
    this->depth_of_stream_buffer = depth_of_stream_buffer;

    //create stream buffer associated with the cache
    //iterate through the required number of stream buffers
    stream_buffer = new stream_buffer_t[number_of_stream_buffers];
    for (uint32_t rows = 0; rows < number_of_stream_buffers; rows++)
    {
        //generate a buffer with given depth
        stream_buffer[rows].ptr_to_stream_buffer = new uint32_t[depth_of_stream_buffer];
    }

    //initialize each of the element of the stream buffer with 0
    for (uint32_t rows = 0; rows < number_of_stream_buffers; rows++)
    {
        stream_buffer[rows].valid_flag = 0;
        stream_buffer[rows].lru_counter = rows;
        for(uint32_t colms = 0; colms < depth_of_stream_buffer; colms++)
        {
            stream_buffer[rows].ptr_to_stream_buffer[colms] = 0;
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

bool Cache::is_cache_miss(uint32_t tag, uint32_t index){
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

//check if the given address is missed in stream buffer
bool Cache::is_stream_buffer_miss(uint32_t addr){
    bool is_miss = true;
    //iterate through all the stream buffers
    for (uint32_t rows=0; rows < number_of_stream_buffers; rows++)
    {
        //if the stream buffer is invalid, dont look for contents as it
        //will be a miss
        if (stream_buffer[rows].valid_flag == 0) {continue;}
        //if valid look through all the contents of the buffer
        else
        {
            for (uint32_t colms=0;colms<depth_of_stream_buffer; colms++)
            {
                if (stream_buffer[rows].ptr_to_stream_buffer[colms] == addr){
                    //if found in stream buffer, it is a hit
                    is_miss = false;
                    //do not look for further. exit from the loop
                    break;
                }
            }
            //if already found the buffer, exit from the loop
            if (is_miss == false){break;}
        }
    }

    return is_miss;
}

void Cache::update_stream_buffer(bool cache_miss, bool stb_miss, uint32_t addr)
{
    uint32_t stb_lru_count_to_evict = 0; //lru count that needs to be replaced
    uint32_t addr_to_bring_in_stb = 0; //starting address with which stb starts filling
    //variables for iterating through stream buffers
    uint32_t rows = 0;  
    uint32_t colms = 0;

    //variables storing the values of row,colm for which
    //stream buffer hits
    bool update_stream_buffer_flag = true;
    bool found_addr_in_stb_flag = false;
    //1. cache miss, stb miss
    //2. cache miss, stb hit
    //3. cache hit, stb miss
    //4. cache hit, stb hit

    //#2 and #4 are same in terms of stream buffer update
    
    //if stb hits, 
    //- update the recency order of stream buffer 
    //- remove the addresses lesser than "hit" address
    //- bring in more addresses depending on the length

    //if stb misses, -> cache misses
    //- remove the contents of the LRU buffer
    //- bring in M elements from addr+1 to addr+M
    if (stb_miss == true)
    {
        //incase cache misses
        if (cache_miss == true)
        {
            //update the values for the least recently used buffer
            stb_lru_count_to_evict = number_of_stream_buffers-1;
            addr_to_bring_in_stb = addr + 1;
            cache_measurements.prefetches += depth_of_stream_buffer;
            update_stream_buffer_flag = true;
        }
        //incase cache hit,
        // dont do anything
        else update_stream_buffer_flag = false;
    }
    //stream buffer hit
    else
    {
        update_stream_buffer_flag = true;
        //loop through based on recency order
        for (uint32_t lru_count =0 ; lru_count < number_of_stream_buffers; lru_count++)
        {
            //get the buffer number and index of buffer at which
            //stream buffer hits
            for (rows = 0; rows < number_of_stream_buffers; rows++)
            {
                //if lru count is not the one required, skip the loop.
                if (stream_buffer[rows].lru_counter != lru_count) continue;
                //if flag is invalid, exit from the loop and go to next count value
                if (stream_buffer[rows].valid_flag == 0) break;

                //check only the valid buffers
                found_addr_in_stb_flag = false;
                for (colms=0; colms < depth_of_stream_buffer; colms++)
                {
                    if (stream_buffer[rows].ptr_to_stream_buffer[colms] == addr)
                    {
                        stb_lru_count_to_evict = lru_count;
                        addr_to_bring_in_stb = addr+1;
                        found_addr_in_stb_flag = true;
                        //total number of prefetches required 
                        //if hit on index colm, remove all the elements above colm
                        //new elements required would be the number removed
                        cache_measurements.prefetches += colms+1;
                        //break from the loop once found the element in buffer
                        break;
                    }
                }
                //break if could not find in a given buffer
                break;
            }
            //do not search for any other recency
            if (found_addr_in_stb_flag == true) break;
        }
    }


    //only update in required
    if (update_stream_buffer_flag == true)
    {
        //update stream buffer
        for (rows=0; rows < number_of_stream_buffers; rows++)
        {
            //increment the lru counter of values lesser than hit
            if (stream_buffer[rows].lru_counter < stb_lru_count_to_evict)
            {
                stream_buffer[rows].lru_counter += 1;
            }
            else if(stream_buffer[rows].lru_counter == stb_lru_count_to_evict)
            {
                stream_buffer[rows].lru_counter = 0;
                stream_buffer[rows].valid_flag = 1;
                //bring in all the addresses
                for (colms=0; colms<depth_of_stream_buffer; colms++)
                {
                    stream_buffer[rows].ptr_to_stream_buffer[colms] = addr_to_bring_in_stb + colms;
                }
            }
            //else do not change the lru_counter value
        }
    }
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

uint32_t Cache::get_addr_from_tag_index(uint32_t tag, uint32_t index)
{
    //addr = tag + index + offset
    //to make the address again, perform left shift operations
    uint32_t addr;
    addr = (tag << (block_offset_bits + index_bits)) | (index << block_offset_bits) | (uint32_t)(pow(2,block_offset_bits)-1);
    return addr;
}

//handle the read or write request to the given cache
void Cache::request(uint32_t addr, char r_w)
{
    bool miss = true; //cache miss
    uint32_t index = get_index(addr = addr);
    uint32_t tag = get_tag(addr = addr);
    uint32_t lru_count_to_be_evicted = 0;
    uint32_t addr_to_be_evicted = 0;
    
    bool stb_exists = (stream_buffer != nullptr) ? true : false;
    bool stb_miss = true;
    //Debugs-
    // printf("%x\n",addr);

    //check if the memory blocks misses in cache
    miss = is_cache_miss(tag = tag, index = index);
    
    if (stb_exists == true) 
    {
        stb_miss = is_stream_buffer_miss((addr >> block_offset_bits));
        //perform required operation in stream buffer based on
        //hit or miss
        update_stream_buffer(miss, stb_miss, (addr>>block_offset_bits));
    }
    else 
        stb_miss = true;
    
    //for requests from upper memory hierarchy
    if (r_w == 'w')
        cache_measurements.writes += 1; 
    else
        cache_measurements.reads += 1;


    
    //check if the request is a miss in cache
    if (miss == true)
    {
        if (stb_miss == true)
        {
            if (r_w == 'w')
               cache_measurements.write_misses += 1;
            else
                cache_measurements.read_misses +=1;
        }
        
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
                    if (next_mem_hier != nullptr)
                    {
                        //send the address of the block to next mem that is being evicted
                        addr_to_be_evicted = get_addr_from_tag_index(cache[index][colm].memory_block,index);
                        next_mem_hier->request(addr_to_be_evicted,'w');
                    }
                    cache[index][colm].dirty_flag = 0;
                }
                break;
            }
        }
        if (next_mem_hier != nullptr)
        {
            //bring the memory block from  lower hierarchy irrespective of dirty flag
            next_mem_hier->request(addr,'r');
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
            }
        }
    }
    //no explicit read issued to memory in the simulator for miss/hit
    //update the memory block and LRU with the new block based on the r/w request
    evict_and_update_lru(tag,lru_count_to_be_evicted,index, r_w);
    
}

void Cache::print_cache_contents()
{
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
        //else skip the value of the set in loop. Do not print anything
        else
        {
            continue;
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
    //TODO: Delete the cache after printing contents
}

void Cache::print_stream_buffer_contents(){
    //print the contents based on recency order MRU -> LRU
    for (uint32_t lru_count = 0; lru_count < number_of_stream_buffers; lru_count++)
    {
        //print the contents of stream buffers
        for (uint32_t rows = 0; rows < number_of_stream_buffers; rows++)
        {
            //do not print the buffer if lru count does not match
            if (stream_buffer[rows].lru_counter != lru_count) continue;
            //if stream buffer is invalid, do not print it
            if (stream_buffer[rows].valid_flag == 0) break;
            for(uint32_t colms = 0; colms < depth_of_stream_buffer; colms++)
            {
                printf(" %x ",stream_buffer[rows].ptr_to_stream_buffer[colms]);
            }
            printf("\n");
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
        printf("set = %u -> ",row);
        for (uint32_t colm = 0; colm < associativity; colm++)
        {
            printf("lru counter = %u : %x \t",cache[row][colm].lru_counter,cache[row][colm].memory_block);
        }
        printf("\n");
    }
} 