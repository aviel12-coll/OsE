#include "VirtualMemory.h"
#include "MemoryConstants.h"

/*
 * Initialize the virtual memory
 */

static word_t rootTable[PAGE_SIZE]; // the root page table 

void VMinitialize() {
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        rootTable[i] = 0;
    }
}
void VMinitializeSecondTable(uint64_t rootIndex, word_t* secondTable) {
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        secondTable[i] = 0;
    }
    rootTable[rootIndex] = (word_t)secondTable;
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value);

/* writes a word to the given virtual address
 *s
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */

int VMwrite(uint64_t virtualAddress, word_t value);

/* Returns the physical frame index that virtualPage currently maps to,
 * or 0 if the page is not resident in RAM (never written or was evicted).
 * Does not allocate or restore anything — purely a read-only table walk.
 */
uint64_t VMgetMapping(uint64_t virtualPage);