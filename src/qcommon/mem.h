void Mem_Free(void *ptr);
void *Mem_Alloc(size_t size);		/* returns 0 filled memory */
void *Mem_TagMalloc(size_t size, int tag);
void Mem_FreeTags(int tag);


