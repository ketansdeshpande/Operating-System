#include<linux/init.h>
#include<linux/module.h>
#include<linux/sched.h>
#include<linux/mm_types.h>
#include<asm/pgtable.h>
MODULE_LICENSE("GPL");

static int process_ID = 1;
module_param(process_ID, int, S_IRUGO);

int __init init_module()
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long vaddr = 0, pfn;
	struct pid *pid;
	struct task_struct *pid_struct;
	struct mm_struct *pid_mm_struct;
	struct vm_area_struct *vma;

	pid = find_get_pid(process_ID);
	pid_struct = pid_task(pid, PIDTYPE_PID);
	pid_mm_struct = pid_struct->mm;

	for(vma=pid_mm_struct->mmap; vma; vma=vma->vm_next) {
		for(vaddr=vma->vm_start; vaddr < vma->vm_end; vaddr++) {
			pgd = pgd_offset(pid_struct->mm, vaddr);
			if(pgd_none(*pgd)) {
				printk(KERN_ALERT "not mapped in pgd\n");
				return -1;
			} else {
				printk(KERN_ALERT "pgd = 0x%lx\n", pgd_val(*pgd));
			}

			p4d = p4d_offset(pgd, vaddr);
			if(p4d_none(*p4d)) {
				printk(KERN_ALERT "not mapped in p4d\n");
				return -1;
			} else {
				printk(KERN_ALERT "p4d = 0x%lx\n", p4d_val(*p4d));
			}

			pud = pud_offset(p4d, vaddr);
			if(pud_none(*pud)) {
				printk(KERN_ALERT "not mapped in pud\n");
				return -1;
			} else {
				printk(KERN_ALERT "pud = 0x%lx\n", pud_val(*pud));
			}

			pmd = pmd_offset(pud, vaddr);
			if(pmd_none(*pmd)) {
				printk(KERN_ALERT "not mapped in pmd\n");
				return -1;
			} else {
				printk(KERN_ALERT "pmd = 0x%lx\n", pmd_val(*pmd));
			}

			pte = pte_offset_map(pmd, vaddr);
			if(pte_present(*pte)) {
				pfn = pte_pfn(*pte);
				printk(KERN_ALERT "Page frame no is %lu\n", pfn);
				return 0;
			}
		}
	}

	return 0;
}

void __exit cleanup_module()
{
	printk("module exit");
}