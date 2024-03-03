#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <../../kernel/sched/sched.h>

extern int hw_stt_dump_ion_memory_info(bool verbose,struct seq_file *m);

static void hw_stt_cpu_info(struct seq_file *m)
{
	int cpu;
	struct rq *cpu_tmp_rq = NULL;
	seq_printf(m, "====================CPU_INFO====================\n");
	for_each_online_cpu(cpu)
	{
		cpu_tmp_rq = cpu_rq(cpu);
		if(cpu_tmp_rq == NULL)
			continue;
		seq_printf(m, "CPU:%d,CURR:%s\n",cpu,cpu_tmp_rq->curr->comm);
		seq_printf(m, "cfs_pending:%d,rt_pending:%d\n",cpu_tmp_rq->cfs.nr_running,cpu_tmp_rq->rt.rt_nr_running);
		seq_printf(m, "\n");
	}
	seq_printf(m, "====================CPU_INFO_END====================\n");
}

void hw_stt_show(struct seq_file *m)
{
#ifdef CONFIG_ION
//	hw_stt_dump_ion_memory_info(true,m);
#endif
	hw_stt_cpu_info(m);
}
EXPORT_SYMBOL(hw_stt_show);
