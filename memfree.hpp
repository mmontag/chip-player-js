#ifndef _WINRAR_MEMFREE_
#define _WINRAR_MEMFREE_

#ifndef USE_MEMFREE

#define rmalloc malloc
#define rcalloc calloc
#define rrealloc realloc
#define rfree free
#define rstrdup strdup

#else

void* rmalloc(size_t size);
void* rcalloc(size_t nitems,size_t size);
void* rrealloc(void *block,size_t size);
void rfree(void *block);
char* rstrdup(const char *s);

class MemoryFree
{
  private:
    void **Allocated;
    int AllocSize;
  public:
    MemoryFree();
    ~MemoryFree();
    void Free();
    void Add(void *Addr);
    void Delete(void *Addr);
};

#endif //USE_MEMFREE

#endif
