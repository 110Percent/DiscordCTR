
#ifndef NODEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#define DEBUGS(wat) puts(wat)
#define DEBUGR(wat) fputs(wat, stdout)
#else
#define DEBUG(...)
#define DEBUGS(wat)
#defien DEBUGR(wat)
#endif

#ifdef __cplusplus
#define SSWITCH(wat) do { string __switch_dummy__ = wat;
#define SCASE(wat) if(__switch_dummy__ == wat)
#define SSEND() } while(0)
#endif
