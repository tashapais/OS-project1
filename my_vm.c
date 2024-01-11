#include "my_vm.h"
#include <string.h>
#include <pthread.h>

//struct tlb tlb_store;      // piazza question #267 - instructor follow up said to move this variable from my_vm.h to here
tlbe_t* tlb;
int tlb_index_bits;
int tlb_tag_bits;
double tlb_lookups;
double tlb_misses;
int init = 0; 		   // flag thats set to 1 when the library if first used
unsigned long num_p_pages; // number of physical pages
unsigned long num_v_pages; // number of virtual pages
int pd_bits; 		   // number of bits used for the PDE
int pt_bits;		   // number of bits used for PTEs
int pg_offset_bits; 	   // number of bits used to offset into a page
void* p_mem;		   // large region of contigous memory that acts as physical memory 
unsigned long* p_map;	   // physical bitmap to keep track of allocation status of physical pages
unsigned long* v_map;	   // virtual bitmap to keep track of allocation status of virtual pages
int entry_ppn_bits;	   // number of bits used for PPN in PDEs and PTEs -> lg(num_p_pages) 
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;	//mutex for t_malloc
pthread_mutex_t free_mutex = PTHREAD_MUTEX_INITIALIZER;		//mutex for t_free
pthread_mutex_t put_get_val_mutex = PTHREAD_MUTEX_INITIALIZER;	//mutex for put_value and get_value
						


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    	//32-bit address space PGSIZE pages, MEMSIZE physical memory  
	//calculate number of physical pages
	num_p_pages = MEMSIZE / PGSIZE;
	
	//calculate number of virtual pages
	int pg_size_shift = PGSIZE;
	pg_offset_bits = 0;
	while (pg_size_shift >>= 1) pg_offset_bits += 1;
	num_v_pages = 1 << (32 - pg_offset_bits);
	
	//allocate physical memory, physical bitmap, and virtual bitmap
	/*with calloc, we rely on unset bits in p_mem (valid bit), 
	            p_map and v_map (garbage value) at init time*/
	//p_mem = calloc(MEMSIZE, 1); //will be going through p_mem in page size increments so I didn't cast
	//p_map = (unsigned long*) calloc(num_p_pages / (sizeof(unsigned long) * 8), sizeof(unsigned long));
	//v_map = (unsigned long*) calloc(num_v_pages / (sizeof(unsigned long) * 8), sizeof(unsigned long));
	p_mem = malloc(MEMSIZE);
	memset(p_mem, 0, MEMSIZE);
	p_map = (unsigned long*) malloc(num_p_pages / 8);
	memset(p_map, 0, num_p_pages / 8);
	v_map = (unsigned long*) malloc(num_v_pages / 8);
	memset(v_map, 0, num_v_pages / 8);

	//calculate number of bits used for the PDE and PTEs	
	unsigned long num_ptes = PGSIZE / sizeof(pte_t);
	pt_bits = 0;
	while (num_ptes >>= 1) pt_bits += 1;
	pd_bits = 32 - pg_offset_bits - pt_bits;
	//calculate number of bits used for PPN in PDEs and PTEs
	int num_p_pages_shift = num_p_pages;
	entry_ppn_bits = 0;
	while (num_p_pages_shift >>= 1) entry_ppn_bits += 1;


	//set bit for PD in physical bitmap
	set_bit_at_index(p_map, 0);	
	set_bit_at_index(v_map, 0); //setting this bit so our virtual addresses dont start from 0x0
	//void* p_map = malloc(num_p_pages / 32);
	//void* v_map = malloc(num_v_pages / 32);

	//printf("pg_offset_bits=%d, num_v_pages=%ld, num_p_pages=%ld\n", pg_offset_bits,num_v_pages,num_p_pages);	
	tlb = malloc(sizeof(tlbe_t) * TLB_ENTRIES);
	memset(tlb, 0, TLB_ENTRIES * sizeof(tlbe_t));
	int tlb_size_shift = TLB_ENTRIES;
	tlb_index_bits = 0;
	while (tlb_size_shift >>= 1) tlb_index_bits += 1;
	tlb_tag_bits = 32 - pg_offset_bits - tlb_index_bits;
	tlb_lookups = 0; 
	tlb_misses = 0;
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, pte_t* pte)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
	//get va index bits
	tlb_misses += 1;
	int tlb_index = get_top_bits((unsigned long)va << tlb_tag_bits, tlb_index_bits);
	//get va tag bits
	int tlb_tag = get_top_bits((unsigned long)va, tlb_tag_bits);
	memset(&tlb[tlb_index], 0, sizeof(tlbe_t));
	tlb[tlb_index].tag = (unsigned long)tlb_tag;
	tlb[tlb_index].pte = (unsigned long)pte;
    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */

	tlb_lookups += 1;
	//get va index bits
	int tlb_index = get_top_bits((unsigned long)va << tlb_tag_bits, tlb_index_bits);
	//get va tag bits
	int tlb_tag = get_top_bits((unsigned long)va, tlb_tag_bits);
	//printf("va=%p index=%d tag=%d\n",va,tlb_index,, tlb_tag);
	if (tlb[tlb_index].tag == (unsigned long)tlb_tag)
	{
		return tlb[tlb_index].pte;
	}
	//tlb_misses += 1;
	return NULL;

   /*This function should return a pte_t pointer*/
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = tlb_misses / tlb_lookups;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/




    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

	//check TLB
	pte_t* tlb_translation = check_TLB(va);;
	if (tlb_translation != NULL) 
	{
		return tlb_translation;
	}
	// Part 1
	// get pde index
	int pde_index = get_top_bits((unsigned long)va, pd_bits);
	// get pte index
	int pte_index = get_top_bits((unsigned long)va << (unsigned long)pd_bits, pt_bits);
	
	//get pde (memory access)
	pde_t* pd_entry  = (pde_t*)&pgdir[pde_index];

//
	//printf("va=%x, pde_index=%d, pte_index=%d pd_entry=%lu\n",va, pde_index, pte_index, (unsigned long)*pd_entry);
//
	
	//check if page directory entry is valid
	//valid bit will be the MSB
	if (get_top_bits((unsigned long)*pd_entry, 1) == 1)
	{
		//valid pd_entry -> get ppn of page table
		//left shift one to truncate MSB (valid bit)
		int pt_ppn = get_top_bits((unsigned long)*pd_entry << 1, entry_ppn_bits); 		
		//get addr of page table with page table phyical page number (pt_ppn)

		pte_t* pgtbl = (pte_t*)((char*)pgdir + (pt_ppn * PGSIZE));
		//get pte (memory access)
		pte_t* pt_entry = &pgtbl[pte_index];

		//check if page table entry is valid
		//valid bit will be the MSB
		if (get_top_bits((unsigned long)*pt_entry, 1) == 1)
		{
			add_TLB(va, pt_entry);
			return pt_entry;

		}
		else
		{
			//invalid pt_entry
			return NULL;
		}
	}
	else
	{
		//invalid pd_entry
		return NULL;
	}
		
    //If translation not successful, then return NULL
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

	//check to see if there already is an existing mapping for the given virtual address
	//no mapping exists so we create one
	//with the same method as translate(), we get page directory index and page table index, and index into pd
	int pde_index = get_top_bits((unsigned long)va, pd_bits);
	int pte_index = get_top_bits((unsigned long)va << (unsigned long)pd_bits, pt_bits);
	pde_t* pd_entry  = (pde_t*)&pgdir[pde_index];


	//get pt_ppn from pd_entry
	int pt_ppn;
	//check if page directory entry valid bit is unset
	//valid bit will be the MSB
	if (get_top_bits((unsigned long)*pd_entry, 1) == 0)
	{
		/*page directory entry (va) is invalid -> map a new page table's ppn to an unsed page directory entry,
			and set pt_ppn in pd_entry*/
		pt_ppn = get_next_ppn();
		//printf("new pt at ppn=%d\n", pt_ppn);
		set_bit_at_index(p_map, pt_ppn);
		//set valid bit and pt_ppn
		memset(pd_entry, 0, sizeof(pde_t));
		*pd_entry |= (1 << 31) + (pt_ppn << (sizeof(unsigned long) * 8) - entry_ppn_bits - 1);	
	}
	else
	{
		//valid pd_entry -> get ppn of page table
		//left shift one to truncate MSB (valid bit)
		pt_ppn = get_top_bits((unsigned long)*pd_entry << 1, entry_ppn_bits); 		
	}

	//get addr of page table with page table phyical page number (pt_ppn)
	pte_t* pgtbl = (pte_t*)((char*)pgdir + (pt_ppn * PGSIZE));

	//get pte (memory access)
	pte_t* pt_entry = &pgtbl[pte_index];
	if (get_top_bits((unsigned long)*pt_entry, 1) == 0)
	{
		//get ppn for new page that was passed in from t_malloc. (its bit in p_map was already set in t_malloc)
		int ppn = (pa - p_mem) / PGSIZE;	
		memset(pt_entry, 0, sizeof(pte_t));
		//set valid bit and ppn
		*pt_entry |= (1 << 31) + (ppn << (sizeof(unsigned long) * 8) - entry_ppn_bits - 1);	
		//printf("new pt_entry=%lu*****pte_index=%d***** pd_entry=%lu*****pde_index=%d*****ppn=%d\n", (unsigned long)pgtbl[pte_index],pte_index,pgdir[pde_index],pde_index,ppn);
		//add new translation to TLB
		add_TLB(va, pt_entry);		
		return 1; //va successfully mapped to pa
	}
	else
	{
		return -1;
	}
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    	//Use virtual address bitmap to find the next free page
	int consec = 0;
	for (int i = 0; i < num_v_pages / 32; ++i) 
	{
		//printf("checking index  %d\n", i );
        	unsigned long curMap = v_map[i];
        	for (int j = 0; j < sizeof(unsigned long) * 8; ++j) 
		{

			//printf("checking bit %d in %d, that is %d\n",j, i, (curMap & (1UL << j)) );
            		if ((curMap & (1UL << j)) == (unsigned long)0) 
			{
                		consec++;
                		if (consec == num_pages) 
				{

					//printf("consec=%d, v_map index= %d\n",consec, (i*sizeof(unsigned long)*8+j-num_pages+1));
                    			unsigned long v_page_addr = (i * sizeof(unsigned long) * 8 + j - num_pages + 1)*PGSIZE;
					for (int k = i*sizeof(unsigned long)*8+j+1-num_pages; k < i*sizeof(unsigned long)*8+j+1; k++)
					{
						set_bit_at_index(v_map, k);
					}
					return (void*)v_page_addr;
                		}
            		} 
			else 
			{

                		consec = 0;
			}
		}
	}

    	return NULL;
}

/*NOT DEFAULT Function that I think should be combined with get_next_avail (add bitmap parameter)
this function is used to get the next available phyical page number*/
int get_next_ppn()
{	
	for (int i = 0; i < num_p_pages; i++)
	{
		if (get_bit_at_index(p_map,i) == 0) return i;
	}
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
	pthread_mutex_lock(&malloc_mutex);
	if (!init)
	{       
		set_physical_mem();
		init = 1;
	}
	
	//compute number of pages needed and get base address of va
	int num_pages;
	if (num_bytes <= PGSIZE)
	{
		num_pages = 1;
	}
	else
	{
		num_pages = num_bytes / PGSIZE;
		if (num_bytes % PGSIZE != 0) num_pages += 1;
	}
	//printf("getting %d pages\n", num_pages);
	void* va = get_next_avail(num_pages);

	//map each va to an unsed pa
	for(int i = 0; i < num_pages; i++)
	{
		void* cur_va = va + (i * PGSIZE);
		int cur_pa_ppn = get_next_ppn();
		set_bit_at_index(p_map, cur_pa_ppn);
		void* cur_pa = (char*)p_mem + (cur_pa_ppn * PGSIZE);

		if (page_map(p_mem, cur_va, cur_pa) == 1)
		{
			//va mapped to pa 
			//do something with page_map return value
			//printf("va=%p mapped to pa=%p\n\n\n", cur_va, cur_pa);
		}
		else
		{
			//va couldnt be mapped to a pa	
			//do something with page_map return value
			//printf("va=%p cant be mapped to pa=%p\n\n\n", cur_va, cur_pa);
		}	
	}

	pthread_mutex_unlock(&malloc_mutex);
	return va;
   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
 
       //PART 1
       pthread_mutex_lock(&free_mutex);
       int offset = 0;
//       unsigned long cur_va = (unsigned long)va;
       while(offset < size)
       {
		unsigned long cur_va = (unsigned long)va + offset;
		//printf("freeing pages corresponsing to va=%x\n", cur_va);
		pte_t* cur_pte = translate(p_mem, cur_va);
		if (cur_pte != NULL)
		{
			int vpn = cur_va / PGSIZE;
			clear_bit_at_index(v_map, vpn);
			int ppn = get_top_bits((unsigned long)*cur_pte << 1, entry_ppn_bits); 		
		//	printf("freeing/clearing bit in pmap-ppn=%d vpn=%d, pte=%lu\n", ppn,vpn,*cur_pte);
			clear_bit_at_index(p_map, ppn);
			memset(cur_pte, 0, sizeof(pte_t));
		}
		offset += PGSIZE;
       } 

       pthread_mutex_unlock(&free_mutex);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
	//compute number of pages the size of value is
	pthread_mutex_lock(&put_get_val_mutex);
	int num_pages;
	if (size <= PGSIZE) 
	{
		num_pages = 1;
	}
	else
	{
		num_pages = size / PGSIZE;
		if (size%PGSIZE != 0) num_pages += 1;
	}

	for(int i = 0; i < num_pages; i++)
	{
		
		pte_t* cur_pte;
		if ((cur_pte = translate(p_mem, (char*)va + (i * PGSIZE))) != NULL)
		{	
			int cur_size = size >= PGSIZE ? PGSIZE : size;
			size -= PGSIZE;
			int ppn = get_top_bits((unsigned long)*cur_pte << 1, entry_ppn_bits);
			char* physical_addr = (char*)p_mem + (ppn * PGSIZE);
			memcpy(physical_addr, val, cur_size);
			
			//printf("put data at %p of size %d in ppn=%d\n", physical_addr,cur_size,ppn);
			//printf("put data from %p to %p of size %d\n", val,physical_addr,cur_size);
		}
		else
		{

			pthread_mutex_unlock(&put_get_val_mutex);
			return -1;
		}
		
	}	
	pthread_mutex_unlock(&put_get_val_mutex);
	return 0;


    /*return -1 if put_value failed and 0 if put is successfull*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

	pthread_mutex_lock(&put_get_val_mutex);
	int num_pages;
	if (size <= PGSIZE) 
	{
		num_pages = 1;
	}
	else
	{
		num_pages = size / PGSIZE;
		if (size%PGSIZE != 0) num_pages += 1;
	}


	for(int i = 0; i < num_pages; i++)
	{
		
		pte_t* cur_pte;
		if ((cur_pte = translate(p_mem, (char*)va + (i * PGSIZE))) != NULL)
		{	
			int cur_size = size >= PGSIZE ? PGSIZE : size;
			size -= PGSIZE;
			int ppn = get_top_bits((unsigned long)*cur_pte << 1, entry_ppn_bits);
			char* physical_addr = (char*)p_mem + (ppn * PGSIZE);
			memcpy(val, physical_addr, cur_size);
			//printf("got data from %p of size %d in ppn=%d\n", physical_addr,cur_size,ppn);
		}
		
	}	
	pthread_mutex_unlock(&put_get_val_mutex);


}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}


//aux functions added/modified from project 1
static unsigned int get_top_bits(unsigned long value,  int num_bits)
{
        return (value >> (unsigned long)(sizeof(value)*8)-num_bits);
}

static void set_bit_at_index(unsigned long *bitmap, int index)
{

        bitmap[index/(sizeof(unsigned long) * 8)] |= 1 << index%(sizeof(unsigned long) * 8);
        return;
}

static void clear_bit_at_index(unsigned long *bitmap, int index)
{
        bitmap[index/(sizeof(unsigned long) * 8)] &= ~((1UL << index%(sizeof(unsigned long) * 8)));
        return;
}

static int get_bit_at_index(unsigned long *bitmap, int index)
{
    return (bitmap[index/(sizeof(unsigned long) * 8)] >> index%(sizeof(unsigned long) * 8)) & 1;
}

