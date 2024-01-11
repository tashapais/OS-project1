#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 1024

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
typedef struct tlbe_t {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
	//I can't assume the number of TLB_ENTRIES so I'll use 2 unsigned longs to hold the va data and pa data
	unsigned long tag;
	pte_t* pte;



}tlbe_t;
// piazza question #267 - instructor follow up said to move this variable from here to a global variable in my_vm.c
//struct tlb tlb_store;


void set_physical_mem();
pte_t* translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
pte_t* check_TLB(void *va);
int add_TLB(void *va, pte_t* pte);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
//not default
static unsigned int get_top_bits(unsigned long value, int num_bits);
static void set_bit_at_index(unsigned long* bitmap, int index);
static void clear_bit_at_index(unsigned long* bitmap, int index);
static int get_bit_at_index(unsigned long *bitmap, int index);
int get_next_ppn();

#endif
