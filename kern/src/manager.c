/*
 * Copyright (c) 2009 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 */


#include <ros/common.h>
#include <smp.h>
#include <arch/init.h>
#include <mm.h>
#include <elf.h>

#include <kmalloc.h>
#include <assert.h>
#include <manager.h>
#include <process.h>
#include <schedule.h>
#include <syscall.h>
#include <ktest.h>
#include <stdio.h>
#include <time.h>
#include <monitor.h>
#include <string.h>
#include <pmap.h>
#include <arch/console.h>
#include <time.h>

/*
 * Currently, if you leave this function by way of proc_run (process_workqueue
 * that proc_runs), you will never come back to where you left off, and the
 * function will start from the top.  Hence the hack 'progress'.
 */
void manager(void)
{
	// LoL
	#define MANAGER_FUNC(dev) PASTE(manager_,dev)

	#if !defined(DEVELOPER_NAME) && \
	    (defined(CONFIG_KERNEL_TESTING) || \
	     defined(CONFIG_USERSPACE_TESTING))
		#define DEVELOPER_NAME jenkins
	#endif

	#ifndef DEVELOPER_NAME
		#define DEVELOPER_NAME brho
	#endif

	void MANAGER_FUNC(DEVELOPER_NAME)(void);
	MANAGER_FUNC(DEVELOPER_NAME)();
}

char *p_argv[] = {0, 0, 0};
/* Helper macro for quickly running a process.  Pass it a string, *file, and a
 * *proc. */
#define quick_proc_run(x, p, f)                                              \
	(f) = do_file_open((x), O_READ, 0);                                  \
	assert((f));                                                         \
	p_argv[0] = file_name((f));                                          \
	(p) = proc_create((f), p_argv, NULL);                                \
	kref_put(&(f)->f_kref);                                              \
	spin_lock(&(p)->proc_lock);                                          \
	__proc_set_state((p), PROC_RUNNABLE_S);                              \
	spin_unlock(&(p)->proc_lock);                                        \
	proc_run_s((p));                                                     \
	proc_decref((p));

#define quick_proc_create(x, p, f)                                           \
	(f) = do_file_open((x), O_READ, 0);                                  \
	assert((f));                                                         \
	p_argv[0] = file_name((f));                                          \
	(p) = proc_create((f), p_argv, NULL);                                \
	kref_put(&(f)->f_kref);                                              \
	spin_lock(&(p)->proc_lock);                                          \
	__proc_set_state((p), PROC_RUNNABLE_S);                              \
	spin_unlock(&(p)->proc_lock);

#define quick_proc_color_run(x, p, c, f)                                     \
	(f) = do_file_open((x), O_READ, 0);                                  \
	assert((f));                                                         \
	p_argv[0] = file_name((f));                                          \
	(p) = proc_create((f), p_argv, NULL);                                \
	kref_put(&(f)->f_kref);                                              \
	spin_lock(&(p)->proc_lock);                                          \
	__proc_set_state((p), PROC_RUNNABLE_S);                              \
	spin_unlock(&(p)->proc_lock);                                        \
	p->cache_colors_map = cache_colors_map_alloc();                      \
	for (int i = 0; i < (c); i++)                                        \
		cache_color_alloc(llc_cache, p->cache_colors_map);           \
	proc_run_s((p));                                                     \
	proc_decref((p));

#define quick_proc_color_create(x, p, c, f)                                  \
	(f) = do_file_open((x), O_READ, 0);                                  \
	assert((f));                                                         \
	p_argv[0] = file_name((f));                                          \
	(p) = proc_create((f), p_argv, NULL);                                \
	kref_put(&(f)->f_kref);                                              \
	spin_lock(&(p)->proc_lock);                                          \
	__proc_set_state((p), PROC_RUNNABLE_S);                              \
	spin_unlock(&(p)->proc_lock);                                        \
	p->cache_colors_map = cache_colors_map_alloc();                      \
	for (int i = 0; i < (c); i++)                                        \
		cache_color_alloc(llc_cache, p->cache_colors_map);

void manager_brho(void)
{
	static bool first = TRUE;
	struct per_cpu_info *pcpui = &per_cpu_info[core_id()];

	if (first) {
		printk("*** IRQs must be enabled for input emergency codes ***\n");
		#ifdef CONFIG_X86
		printk("*** Hit ctrl-g to enter the monitor. ***\n");
		printk("*** Hit ctrl-q to force-enter the monitor. ***\n");
		printk("*** Hit ctrl-b for a backtrace of core 0 ***\n");
		#else
		printk("*** Hit ctrl-g to enter the monitor. ***\n");
		#warning "***** ctrl-g untested on riscv, check k/a/r/trap.c *****"
		#endif
		first = FALSE;
	}
	/* just idle, and deal with things via interrupts.  or via face. */
	smp_idle();
	/* whatever we do in the manager, keep in mind that we need to not do
	 * anything too soon (like make processes), since we'll drop in here
	 * during boot if the boot sequence required any I/O (like EXT2), and we
	 * need to PRKM() */
	assert(0);

}

void manager_jenkins()
{
	#ifdef CONFIG_KERNEL_TESTING
		printk("<-- BEGIN_KERNEL_TESTS -->\n");
		run_registered_ktest_suites();
		printk("<-- END_KERNEL_TESTS -->\n");
	#endif

	// Run userspace tests (from config specified path).
	#ifdef CONFIG_USERSPACE_TESTING
	if (strlen(CONFIG_USERSPACE_TESTING_SCRIPT) != 0) {
		char exec[] = "/bin/ash";
		char *p_argv[] = {exec, CONFIG_USERSPACE_TESTING_SCRIPT, 0};

		struct file *program = do_file_open(exec, O_READ, 0);
		struct proc *p = proc_create(program, p_argv, NULL);
		proc_wakeup(p);
		/* let go of the reference created in proc_create() */
		proc_decref(p);
		kref_put(&program->f_kref);
		run_scheduler();
	    // Need a way to wait for p to finish
	} else {
		printk("No user-space launcher file specified.\n");
	}
	#endif
	smp_idle();
	assert(0);
}

void manager_klueska()
{
	static struct proc *envs[256];
	static volatile uint8_t progress = 0;

	if (progress == 0) {
		progress++;
		panic("what do you want to do?");
		//envs[0] = kfs_proc_create(kfs_lookup_path("fillmeup"));
		__proc_set_state(envs[0], PROC_RUNNABLE_S);
		proc_run_s(envs[0]);
		warn("DEPRECATED");
	}
	run_scheduler();

	panic("DON'T PANIC");
}

void manager_waterman()
{
	static bool first = true;

	if (first)
		mon_shell(0, 0, 0);
	smp_idle();
	assert(0);
}

void manager_yuzhu()
{

	static uint8_t progress = 0;
	static struct proc *p;

	// for testing taking cores, check in case 1 for usage
	uint32_t corelist[MAX_NUM_CORES];
	uint32_t num = 3;

	//create_server(init_num_cores, loop);

	monitor(0);

	// quick_proc_run("hello", p);

}
