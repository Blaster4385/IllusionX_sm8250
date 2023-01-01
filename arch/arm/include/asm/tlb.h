/*
 *  arch/arm/include/asm/tlb.h
 *
 *  Copyright (C) 2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Experimentation shows that on a StrongARM, it appears to be faster
 *  to use the "invalidate whole tlb" rather than "invalidate single
 *  tlb" for this.
 *
 *  This appears true for both the process fork+exit case, as well as
 *  the munmap-large-area case.
 */
#ifndef __ASMARM_TLB_H
#define __ASMARM_TLB_H

#include <asm/cacheflush.h>

#ifndef CONFIG_MMU

#include <linux/pagemap.h>

#define tlb_flush(tlb)	((void) tlb)

#include <asm-generic/tlb.h>

#else /* !CONFIG_MMU */

#include <linux/swap.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

static inline void __tlb_remove_table(void *_table)
{
	free_page_and_swap_cache((struct page *)_table);
}

#include <asm-generic/tlb.h>

#ifndef CONFIG_HAVE_RCU_TABLE_FREE
#define tlb_remove_table(tlb, entry) tlb_remove_page(tlb, entry)
#endif

static inline void
__pte_free_tlb(struct mmu_gather *tlb, pgtable_t pte, unsigned long addr)
{
	pgtable_page_dtor(pte);

#ifndef CONFIG_ARM_LPAE
	/*
	 * With the classic ARM MMU, a pte page has two corresponding pmd
	 * entries, each covering 1MB.
	 */
	addr = (addr & PMD_MASK) + SZ_1M;
	__tlb_adjust_range(tlb, addr - PAGE_SIZE, 2 * PAGE_SIZE);
#endif

	tlb_remove_table(tlb, pte);
}

static inline void
__pmd_free_tlb(struct mmu_gather *tlb, pmd_t *pmdp, unsigned long addr)
{
#ifdef CONFIG_ARM_LPAE
	struct page *page = virt_to_page(pmdp);

	tlb_remove_table(tlb, page);
#endif
}

static inline void
tlb_flush_pmd_range(struct mmu_gather *tlb, unsigned long address,
		    unsigned long size)
{
	tlb_add_flush(tlb, address);
	tlb_add_flush(tlb, address + size - PMD_SIZE);
}

#endif /* CONFIG_MMU */
#endif
