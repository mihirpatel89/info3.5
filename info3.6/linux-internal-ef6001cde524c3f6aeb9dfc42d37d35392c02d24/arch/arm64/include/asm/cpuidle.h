#ifndef __ASM_CPUIDLE_H
#define __ASM_CPUIDLE_H

#ifdef CONFIG_CPU_IDLE
extern int arm_cpuidle_init(unsigned int cpu);
extern int arm_cpuidle_suspend(int index);
#else
static inline int arm_cpuidle_init(unsigned int cpu)
{
	return -EOPNOTSUPP;
}

static inline int arm_cpuidle_suspend(int index)
{
	return -EOPNOTSUPP;
}

static inline int cpu_suspend(unsigned long arg)
{
	return -EOPNOTSUPP;
}
#endif
#endif
