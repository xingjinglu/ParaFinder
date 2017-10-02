#ifndef H_CY_RDSTC
#define H_CY_RDSTC



#define _RDTSC(high ,low) \
    __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high));

inline unsigned long long rdtsc(){
   unsigned long lo, hi;
   __asm__ __volatile__(
     "xorl %%eax, %%eax\n"
     "cpuid\n"
     "rdtsc\n"
     :"=a"(lo), "=d"(hi)
     :
     :"%ebx", "%ecx");
   return ((unsigned long long)hi)<<32 | lo;
}

#define GET_DURATION_RDTSC(START, END)  (( END - START) / 3500.0 / 1000.0)

/*
#ifdef SYNC_RDTSC

#define RDTSC(highlow) do{highlow = rdtsc();}while(false);

#else

#define RDTSC(highlow) \
    do\
{\
    unsigned int high, low;\
    _RDTSC(high, low);\
    unsigned long long h = 0, l = 0;\
    h = high; l = low;\
    highlow = ((h<<32)|(l));\
}while(false);

#endif // SYNC_RDTSC
*/


#endif
    
