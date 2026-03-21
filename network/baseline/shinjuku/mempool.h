#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#define MEMPOOL_INITIAL_OFFSET (0)
#define MEMPOOL_SANITY_ISPERFG(_a)
#define MEMPOOL_SANITY_ACCESS(_a)
#define MEMPOOL_SANITY_LINK(_a, _b)

enum {
	PGSHIFT_4KB = 12,
	PGSHIFT_2MB = 21,
	PGSHIFT_1GB = 30,
};

enum {
	PGSIZE_4KB = (1 << PGSHIFT_4KB), /* 4096 bytes */
	PGSIZE_2MB = (1 << PGSHIFT_2MB), /* 2097152 bytes */
	PGSIZE_1GB = (1 << PGSHIFT_1GB), /* 1073741824 bytes */
};

#define PGMASK_4KB	(PGSIZE_4KB - 1)
#define PGMASK_2MB	(PGSIZE_2MB - 1)
#define PGMASK_1GB	(PGSIZE_1GB - 1)

/* page numbers */
#define PGN_4KB(la)	(((uintptr_t) (la)) >> PGSHIFT_4KB)
#define PGN_2MB(la)	(((uintptr_t) (la)) >> PGSHIFT_2MB)
#define PGN_1GB(la)	(((uintptr_t) (la)) >> PGSHIFT_1GB)

#define PGOFF_4KB(la)	(((uintptr_t) (la)) & PGMASK_4KB)
#define PGOFF_2MB(la)	(((uintptr_t) (la)) & PGMASK_2MB)
#define PGOFF_1GB(la)	(((uintptr_t) (la)) & PGMASK_1GB)

#define PGADDR_4KB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_4KB))
#define PGADDR_2MB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_2MB))
#define PGADDR_1GB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_1GB))

/*
 * numa policy values: (defined in numaif.h)
 * MPOL_DEFAULT - use the process' global policy
 * MPOL_BIND - force the numa mask
 * MPOL_PREFERRED - use the numa mask but fallback on other memory
 * MPOL_INTERLEAVE - interleave nodes in the mask (good for throughput)
 */

typedef unsigned long machaddr_t; /* host physical addresses */
typedef unsigned long physaddr_t; /* guest physical addresses */
typedef unsigned long virtaddr_t; /* guest virtual addresses */

#define MEM_IX_BASE_ADDR		0x70000000   /* the IX ELF is loaded here */
#define MEM_PHYS_BASE_ADDR		0x4000000000 /* memory is allocated here (2MB going up, 1GB going down) */
#define MEM_USER_DIRECT_BASE_ADDR	0x7000000000 /* start of direct user mappings (P = V) */
#define MEM_USER_DIRECT_END_ADDR	0x7F00000000 /* end of direct user mappings (P = V) */
#define MEM_USER_IOMAPM_BASE_ADDR	0x8000000000 /* user mappings controlled by IX */
#define MEM_USER_IOMAPM_END_ADDR	0x100000000000 /* end of user mappings controlled by IX */
#define MEM_USER_IOMAPK_BASE_ADDR	0x100000000000 /* batched system calls and network mbuf's */
#define MEM_USER_IOMAPK_END_ADDR	0x101000000000 /* end of batched system calls and network mbuf's */

#define MEM_USER_START			MEM_USER_DIRECT_BASE_ADDR
#define MEM_USER_END			MEM_USER_IOMAPM_END_ADDR

#define MEM_ZC_USER_START		MEM_USER_IOMAPM_BASE_ADDR
#define MEM_ZC_USER_END			MEM_USER_IOMAPK_END_ADDR

#ifndef MAP_FAILED
#define MAP_FAILED	((void *) -1)
#endif

struct mempool_hdr {
	struct mempool_hdr *next;
	struct mempool_hdr *next_chunk;
} __packed;

// one per data type
struct mempool_datastore {
	uint64_t                 magic;
	spinlock_t               lock;
	struct mempool_hdr      *chunk_head;
	void			*buf;
	int			nr_pages;
	uint32_t                nr_elems;
	size_t                  elem_len;
	int                     nostraddle;
	int                     chunk_size;
	int                     num_chunks;
	int                     free_chunks;
	int64_t                 num_locks;
	const char             *prettyname;
	struct mempool_datastore *next_ds;
#ifdef __KERNEL__
	void 			*iomap_addr;
	uintptr_t		iomap_offset;
#endif
};

struct mempool {
	// hot fields:
	struct mempool_hdr	*head;
	int                     num_free;
	size_t                  elem_len;

	uint64_t                 magic;
	void			*buf;
	struct mempool_datastore *datastore;
	struct mempool_hdr      *private_chunk;
//	int			nr_pages;
	int                     sanity;
	uint32_t                nr_elems;
	int                     nostraddle;
	int                     chunk_size;
#ifdef __KERNEL__
	void 			*iomap_addr;
	uintptr_t		iomap_offset;
#endif
};
#define MEMPOOL_MAGIC							0x12911776
#define CONTEXT_CAPACITY    			64*1024
#define STACK_CAPACITY      			64*1024
#define STACK_SIZE          	 		4096
#define MEMPOOL_SANITY_GLOBAL    	0
#define MEMPOOL_SANITY_PERCPU    	1
#define MEMPOOL_DEFAULT_CHUNKSIZE 128

void *mempool_alloc_2(struct mempool *m);
void *mempool_alloc(struct mempool *m);
void mempool_free_2(struct mempool *m, void *ptr);
void mempool_free(struct mempool *m, void *ptr);
int mempool_create_datastore(struct mempool_datastore *m, int nr_elems, size_t elem_len, int nostraddle, int chunk_size, const char *prettyname);
int mempool_create(struct mempool *m, struct mempool_datastore *mds, int16_t sanity_type, int16_t sanity_id);
void mempool_destroy(struct mempool *m);

#endif /* __MEMPOOL_H__ */