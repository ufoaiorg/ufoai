void Z_Free(void *ptr);
void *Z_Malloc(size_t size);		/* returns 0 filled memory */
void *Z_TagMalloc(size_t size, int tag);
void Z_FreeTags(int tag);


