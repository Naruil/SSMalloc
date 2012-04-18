static __inline__ int get_core_id() {
    return 0;
    int result;
    __asm__ __volatile__ (
        "mov $1, %%eax\n"
        "cpuid\n"
        :"=b"(result)
        :
        :"eax","ecx","edx");
    return (result>>24)%8;
}


static inline unsigned long
read_tsc(void)
{
    unsigned a, d;
    __asm __volatile("rdtsc":"=a"(a), "=d"(d));
    return ((unsigned long)a) | (((unsigned long) d) << 32);
}
