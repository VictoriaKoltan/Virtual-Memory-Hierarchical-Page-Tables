#include "PhysicalMemory.h"
#include "VirtualMemory.h"
#include "MemoryConstants.h"

struct TraversalContext {
    uint64_t page_to_add;
    word_t protected_frame;

    word_t first_unused_frame = 0;
    word_t empty_table_frame = 0;
    uint64_t emptys_parent = 0;

    uint64_t page_to_evict = 0;
    word_t frame_of_evicted = 0;
    uint64_t parent_entry_of_evicted = 0;
    uint64_t max_dist = 0; 
};
/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    for(uint64_t i =0;i < PAGE_SIZE; i++){
        PMwrite( i, 0);
    }

}
/* Checks if a given table/frame contains only zeros */
static bool isTableEmpty(word_t frame) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        word_t value;
        PMread(frame * PAGE_SIZE + i, &value);
        if (value != 0) {
            return false; // Found a non-zero entry, table is not empty
        }
    }
    return true; // All entries are zero
}

/* Calculates the cyclical distance between two virtual pages */
static uint64_t getCyclicalDistance(uint64_t page_swapped_in, uint64_t current_page) {
    uint64_t dist;
    
    // Calculate the direct absolute distance
    if (page_swapped_in >current_page) {
        dist = page_swapped_in -current_page;
    } 
    else {
        dist =current_page-page_swapped_in;
    }
    
    // Calculate the wrap-around distance
    uint64_t wrap_around_dist = NUM_PAGES - dist;
    
    // Return the shortest cyclical distance
    if (dist < wrap_around_dist) {
        return dist;
    } 
    else {
        return wrap_around_dist;
    }
}

/* Updates the context with the page to evict based on max cyclical distance */
static void updateEvictionCandidate(TraversalContext& scan_results, uint64_t current_page, word_t current_frame, uint64_t parent_entry_addr) {
    uint64_t current_dist = getCyclicalDistance(scan_results.page_to_add, current_page);

    // Update if the distance is strictly greater, 
    // OR if it's equal but the page index is strictly smaller (tie-breaker)
    if (current_dist > scan_results.max_dist || 
       (current_dist == scan_results.max_dist &&current_page < scan_results.page_to_evict)) {
        
        scan_results.max_dist = current_dist;
        scan_results.page_to_evict = current_page;
        scan_results.frame_of_evicted = current_frame;
        scan_results.parent_entry_of_evicted = parent_entry_addr;
    }
}

static void traverseTree(word_t current_frame, 
                         int depth, 
                         uint64_t virtual_page, 
                         uint64_t parent_entry_addr, 
                         TraversalContext& scan_results) {
    
    // 1. Update the first unused frame (max frame index + 1)
    if (current_frame >= scan_results.first_unused_frame) {
        scan_results.first_unused_frame = current_frame + 1;
    }

    // 2. Base Case: We reached a leaf (a data page, not a table)
    if (depth == TABLES_DEPTH) {
        // Use our helper function to update the eviction candidate if needed
        updateEvictionCandidate(scan_results, virtual_page, current_frame, parent_entry_addr);
        return;
    }

    // 3. We are at an internal node (a table). Check if it's an empty table.
    // We cannot evict the root table (frame 0) or the frame we are currently protecting.
    if (current_frame != 0 && current_frame != scan_results.protected_frame) {
        if (isTableEmpty(current_frame)) {
            scan_results.empty_table_frame = current_frame;
            scan_results.emptys_parent = parent_entry_addr;
            // Note: We don't return here. If it's empty, the loop below simply won't find any children.
        }
    }

    // 4. Traverse all entries (children) of the current table
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        word_t next_frame;
        PMread(current_frame * PAGE_SIZE + i, &next_frame); // Read child frame index

        if (next_frame != 0) { // If the child exists
            // Calculate the virtual page number for this specific path
            uint64_t shift_amount = OFFSET_WIDTH * (TABLES_DEPTH - depth - 1);
            uint64_t child_virtual_page = virtual_page + (i << shift_amount);

            // Recursive call for the child
            uint64_t current_entry_addr = current_frame * PAGE_SIZE + i;
            traverseTree(next_frame, depth + 1, child_virtual_page, current_entry_addr, scan_results);
        }
    }
}
/* Finds an available physical frame (either an empty table or an unused frame).
 * Returns the frame index, or 0 if an eviction is required.
 */
static word_t findEmptyFrame(TraversalContext& scan_results) {
    //first option: finding a zeros frame that once was in use
    if (scan_results.empty_table_frame != 0) {
        //disconnect the frame form its past parent
        PMwrite(scan_results.emptys_parent, 0);
        return scan_results.empty_table_frame;
    }

    //second option: finding the next empty frame
    if (scan_results.first_unused_frame < NUM_FRAMES) {
        return scan_results.first_unused_frame;
    }

    //third option: physical memory is full, we need to find a victim
    return 0; 

}

/* Evicts a page using the maximal cyclical distance algorithm.
 * Removes its reference from the parent table and returns the freed frame index.
 */
static word_t evictPage(TraversalContext &scan_results) {

    //we save the virtual page that is evicted 
    PMevict(scan_results.frame_of_evicted, scan_results.page_to_evict);

    //disconnect the evicted frame from its father
    PMwrite(scan_results.parent_entry_of_evicted, 0);

    return scan_results.frame_of_evicted;
}


/* Resolves a page fault by allocating a frame (finding empty or evicting).
 * Creates missing tables, maps the page, and returns the new frame index.
 */
static word_t handlePageFault(uint64_t page_to_add, word_t protected_frame) {
    TraversalContext scan_results;
    scan_results.protected_frame = protected_frame;
    scan_results.page_to_add = page_to_add;
    //The scan results will be filled during traverse on the tree.
    traverseTree(0, 0, 0, 0, scan_results);
    // Find an available frame using findEmptyFrame
    word_t allocated_frame = findEmptyFrame(scan_results);
    // If there is none, perform eviction using evictPage
    if (allocated_frame==0){
        allocated_frame = evictPage(scan_results);
    }

    // Return the allocated frame index
    return allocated_frame; 
}
/* Performs an active table walk. If a page fault occurs at any level,
 * it resolves it by calling handlePageFault, initializing/restoring the frame,
 * and linking it back into the tree. Returns the final data frame index.
 */
static word_t translateAddress(uint64_t virtualPage) {
    word_t current_frame = 0;

    for (int i = 0; i < TABLES_DEPTH; ++i) {
        int shift_amount = OFFSET_WIDTH * (TABLES_DEPTH - i - 1);
        uint64_t index = (virtualPage >> shift_amount) & (PAGE_SIZE - 1);
        
        uint64_t entry_addr = current_frame * PAGE_SIZE + index;
        word_t next_frame;
        PMread(entry_addr, &next_frame);

        if (next_frame == 0) { 
            next_frame = handlePageFault(virtualPage, current_frame);

            if (i < TABLES_DEPTH - 1) {
                // Initialize a new intermediate table
                for (uint64_t j = 0; j < PAGE_SIZE;++j) {
                    PMwrite(next_frame * PAGE_SIZE + j, 0);
                }
            } else {
                // Restore an actual data page from disk
                PMrestore(next_frame, virtualPage);
            }

            // Link the newly allocated frame to the tree
            PMwrite(entry_addr, next_frame);
        }

        current_frame = next_frame;
    }

    return current_frame;
}

/* Returns the physical frame index that virtualPage currently maps to,
 * or 0 if the page is not resident in RAM (never written or was evicted).
 * Does not allocate or restore anything — purely a read-only table walk.
 */
uint64_t VMgetMapping(uint64_t virtualPage){
    if (virtualPage >= NUM_PAGES) {
        return 0;
    }
    word_t current_frame = 0;
    for(int i=0;i<TABLES_DEPTH;++i){
        int shift_amount = OFFSET_WIDTH * (TABLES_DEPTH - i -1);//to get to the root
        uint64_t index = (virtualPage >> shift_amount) & (PAGE_SIZE - 1);//the frame (in the RAM) that corresponds to the page 
        word_t next_frame;
        uint64_t entry_addr = (uint64_t)current_frame * PAGE_SIZE + index;
        PMread(entry_addr, &next_frame);//the physical address of the frame  
        if (next_frame == 0) {
            return 0; //we tried accessing a non exiting frame
        }
        current_frame = next_frame;
    }
    return current_frame;
}


/* helper function for calculating the offset
 */
static unsigned int calOffset(uint64_t virtualAddress){
    unsigned int offset = virtualAddress & (PAGE_SIZE - 1);

    return offset;
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){
    uint64_t virtualPage = virtualAddress >> OFFSET_WIDTH;
    if (virtualPage >= NUM_PAGES) {
        return 0; 
    }
    word_t current_frame = translateAddress(virtualPage);  
    unsigned int offset = calOffset(virtualAddress);
    PMread(current_frame * PAGE_SIZE + offset, value); //accessing the exact place in the frame
    return 1;
}

/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */

int VMwrite(uint64_t virtualAddress, word_t value){
    uint64_t virtualPage = virtualAddress >> OFFSET_WIDTH;
    if (virtualPage >= NUM_PAGES) {
        return 0; 
    }
    word_t current_frame = translateAddress(virtualPage);
    unsigned int offset = calOffset(virtualAddress);
    PMwrite(current_frame * PAGE_SIZE + offset, value); //accessing the exact place in the frame
    return 1;
}


