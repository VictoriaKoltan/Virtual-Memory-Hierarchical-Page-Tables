#pragma once
#include "PhysicalMemory.h"
#include "VirtualMemory.h"
#include "MemoryConstants.h"

/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    for(uint64_t i =0;i < PAGE_SIZE; i++){
        PMwrite( i, 0);
    }

}

/* Finds an available physical frame (either an empty table or an unused frame).
 * Returns the frame index, or 0 if an eviction is required.
 */
static word_t findEmptyFrame(...) { ... }


/* Evicts a page using the maximal cyclical distance algorithm.
 * Removes its reference from the parent table and returns the freed frame index.
 */
static word_t evictPage(...) { ... }


/* Resolves a page fault by allocating a frame (finding empty or evicting).
 * Creates missing tables, maps the page, and returns the new frame index.
 */
static word_t handlePageFault(uint64_t virtualPage) {
    // 1. חפש מסגרת פנויה בעזרת findEmptyFrame
    // 2. אם אין, בצע פינוי בעזרת evictPage
    // 3. חבר את המסגרת החדשה לעץ הטבלאות
    // 4. החזר את מספר המסגרת שהוקצתה
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
        int shift_amount = OFFSET_WIDTH * (TABLES_DEPTH - i -1);   //to get to the root
        uint64_t index = (virtualPage >> shift_amount) & (PAGE_SIZE - 1);//the frame (in the RAM) that corresponds to the page 
        word_t next_frame;
        PMread(current_frame * PAGE_SIZE + index, &next_frame);//the physical address of the frame  
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
    word_t current_frame = VMgetMapping(virtualPage);    if (current_frame == 0) {
        return 0; 
    }
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
    word_t current_frame = VMgetMapping(virtualPage);
    if (current_frame == 0) {
        return 0; 
    }
    unsigned int offset = calOffset(virtualAddress);
    PMwrite(current_frame * PAGE_SIZE + offset, value); //accessing the exact place in the frame
    return 1;
}


