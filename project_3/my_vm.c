//Keith Lehman kpl56
//Benjamin Wiggins bcw62


#include "my_vm.h"

void* memstart = NULL; 
pde_t* page_dir = NULL;
int firstlayerbits;
int secondlayerbits;
int offsetbits;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
struct tlb *tlbs;
int tlbindex = 0;
double tlbhits = 0;
double tlbmisses = 0;

//last directory pgdir[x] index used 
int max_dir_idx = -1;
//last table pgdir[max_dir_idx][x] index used
int max_tbl_idx = -1;


// MEMSIZE/PGSIZE = num of pages
// PGSIZE/8 = num of chars needed for all page entries
char* bitmap = NULL;

int pow2(int base, int exponent){
	int result = 1;
	for(int i = 0; i < exponent; i++){
		result*=base;
	}
	return result;
}

int logtoo(int number){
	int result = 0;
	int num = number;
	while(num != 1){
		num /= 2;
		result++;
	}
	return result;
}


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    offsetbits = (int) (logtoo(PGSIZE));
    secondlayerbits = logtoo(PGSIZE / 4);
    firstlayerbits = 32 - secondlayerbits - offsetbits;
    memstart = malloc(MEMSIZE);
    page_dir = (pde_t*) memstart;
    bitmap = malloc(MEMSIZE/PGSIZE/8);
    bitmap[0] |= 1;

    tlbs = malloc(sizeof(struct tlb) * TLB_ENTRIES);


}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{
    tlbs[tlbindex].va = (pte_t*) va;
    tlbs[tlbindex].pa = (pte_t*) pa;
    tlbindex++;
    if(tlbindex == TLB_ENTRIES){tlbindex = 0;}
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

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

    for(int i = 0; i < TLB_ENTRIES; i++){
        if(va == (void*)tlbs[i].va){
            tlbhits++;
            return tlbs[i].pa;
        }
    }
    tlbmisses++;
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
    double miss_rate = tlbmisses / (tlbmisses + tlbhits);
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    pte_t* check = check_TLB(va);
    if(check != NULL){return check;}
    int quavo =  ((long) va >> (long)(offsetbits + secondlayerbits));
    int takeoff =  (((long) va >> offsetbits) & (unsigned int)(pow2(2, secondlayerbits) - 1));
    pte_t* pgtable = (pte_t*) (page_dir[quavo]);// & 0xFFFFF000);
    void* phys_addr = (void*) pgtable[takeoff];//(pte & 0xFFFFF000) + offset;
    return (void*) phys_addr;
}


int page_map(pde_t *pgdir, void *va, void *pa)
{
    int quavo =  ((long) va >> (long)(offsetbits + secondlayerbits));
    int takeoff =  (((long) va >> offsetbits) & (unsigned int)(pow2(2, secondlayerbits) - 1));
    if(!pgdir[quavo]){//!(pgdir[quavo] & 0x1)){//Check to see if a new page table is necessary
        pde_t new_table = (pde_t) get_next_avail(1);
        pgdir[quavo] = new_table;
        long physical_offset =  ((void*)new_table - memstart); //offset from start in bytes
        long bm_index = ((physical_offset / PGSIZE) / 8);
        int bm_bit = ((physical_offset / PGSIZE) % 8);
        bitmap[bm_index] |= (1 << bm_bit);	
    }
    pte_t* pgtable = (pde_t*) pgdir[quavo];
    pgtable[takeoff] =  (pte_t) pa;
    max_dir_idx = quavo;
    max_tbl_idx = takeoff;
    add_TLB(va, pa);
}


void* get_next_avail(int num_pages) {
    for(int i = 0; i < MEMSIZE/PGSIZE/8; i++){
        for (int j = 0; j < 8; j++) { // Iterate over each bit in myChar
            if ((bitmap[i] & (1 << j)) != 0) {
                    continue;
            } else {
                    return (void*) memstart + i * PGSIZE * 8 + j * PGSIZE;
            }
        }
    }
}

pte_t* get_usable_va(int num_pages){
    for (int i = 0; i < pow2(2, firstlayerbits); i++){
        void* start = NULL;
        int consec = 0;
        for (int j = 0; j < pow2(2, secondlayerbits); j++){
            pde_t va = i << (secondlayerbits + offsetbits);
            va += j << offsetbits;
            if(i > max_dir_idx || (i == max_dir_idx && j > max_tbl_idx)){
                start = (void*) va;
                return start;
            }else{
                void* physical = translate(page_dir, (void*) va);
                long physical_offset =  (physical - memstart); //offset from start in bytes
                long bm_index = ((physical_offset / PGSIZE) / 8);
                int bm_bit = ((physical_offset / PGSIZE) % 8);
                if ((bitmap[bm_index] & (1 << bm_bit)) != 0) {
                    if(consec != 0) consec = 0;
                    continue;
                } else {
                    if(consec != 0) consec++;
                    else {
                        start = (void*) va;
                        consec = 1;
                    }
                    if(consec == num_pages) return start;
                }
            }

        }
    }
}


void *t_malloc(unsigned int num_bytes) {
    pthread_mutex_lock(&lock);
    if(memstart == NULL) {
        set_physical_mem();
    }
    unsigned int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;
    pte_t* virt_addr = get_usable_va(num_pages);
    for (int pg = 0; pg < num_pages; pg++){
        void* phys_addr = get_next_avail((num_pages));
        long physical_offset =  (phys_addr - memstart); //offset from start in bytes
        long bm_index = ((physical_offset / PGSIZE) / 8);
        int bm_bit = ((physical_offset / PGSIZE) % 8);
        bitmap[bm_index] |= (1 << bm_bit);
        int quavo =  ((long) virt_addr >> (long)(offsetbits + secondlayerbits));
        int takeoff =  (((long) virt_addr >> offsetbits) & (unsigned int)(pow2(2, secondlayerbits) - 1));
	    page_map(page_dir, (void*) virt_addr + pg * PGSIZE, phys_addr);
    }
    pthread_mutex_unlock(&lock);
    return (void*) virt_addr;
}

void t_free(void *va, int size) {
    pthread_mutex_lock(&lock);
    unsigned int num_pages = (size + PGSIZE - 1) / PGSIZE;
    for (int pg = 0; pg < num_pages; pg++){
        void* curr_va = (void*) va + pg * PGSIZE;
        void* phys_addr = translate(page_dir, curr_va);
        long physical_offset =  (phys_addr - memstart); //offset from start in bytes
        long bm_index = ((physical_offset / PGSIZE) / 8);
        int bm_bit = ((physical_offset / PGSIZE) % 8);
        bitmap[bm_index] &= ~(1 << bm_bit);
    }
    pthread_mutex_unlock(&lock);
}


int put_value(void *va, void *val, int size) {
    unsigned int num_pages = (size + PGSIZE - 1) / PGSIZE;
    for(int i = 0; i < num_pages; i++){
        void* pa = translate(page_dir, va + i * PGSIZE);
        if(i == num_pages - 1){
            memcpy(pa, val + i * PGSIZE, size - i * PGSIZE);
        }else{
            memcpy(pa, val + i * PGSIZE, PGSIZE);
        }
    }
    return 0; 
}


void get_value(void *va, void *val, int size) {
    unsigned int num_pages = (size + PGSIZE - 1) / PGSIZE;
    for(int i = 0; i < num_pages; i++){
        void* pa = translate(page_dir, va + i * PGSIZE);
        if(i == num_pages - 1){
            memcpy(val + i * PGSIZE, pa, size - i * PGSIZE);
        }else{
            memcpy(val + i * PGSIZE, pa, PGSIZE);
        }
    }
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
                // //printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // //printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}