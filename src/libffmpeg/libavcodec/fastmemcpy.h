#if 0
extern void *xine_fast_memcpy(void *to, const void *from, size_t len);
#define memcpy(a,b,c) xine_fast_memcpy(a,b,c)
#endif
