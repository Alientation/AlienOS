#ifndef ALIENOS_CPU_CPU_H
#define ALIENOS_CPU_CPU_H

/* Stop execution keep CPU alive. */
void cpu_idle_loop (void);

/* Halt CPU. */
extern void cpu_halt (void);

#endif /* ALIENOS_CPU_CPU_H */