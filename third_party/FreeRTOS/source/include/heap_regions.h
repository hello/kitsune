static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
__attribute__((section(".lowheap"))) static uint8_t ucHeap_BSS[ 0x3ff7 ];

#define heap_regions_init(xHeapRegions) \
xHeapRegions[0].pucStartAddress = ucHeap_BSS; \
xHeapRegions[0].xSizeInBytes =  sizeof(ucHeap_BSS); \
xHeapRegions[1].pucStartAddress = ucHeap; \
xHeapRegions[1].xSizeInBytes =  sizeof(ucHeap); \
xHeapRegions[2].pucStartAddress = NULL; \
xHeapRegions[2].xSizeInBytes =  0;

#define NUM_HEAP_REGIONS 2
