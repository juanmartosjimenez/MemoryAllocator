#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "xmalloc.h"
#include "bit-ops.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//TODO make program thread safe and implement arenas

int slots[20] = {4, 16, 24, 32, 40, 48, 64, 80, 106, 128, 160, 224, 256, 320, 448, 512, 768, 1024, 1536, 2048};

typedef struct free_list{
    int slots_occupied[32];//keeps track of occupied slots, longest sequence will be at most 128 bytes or 32 ints
    void* data;
    struct free_list* prev;
    struct free_list* next;
    int slot_size;
    int occupied_count;
} free_list;

typedef struct header{
    int slot_size;//size of bytes in every slot
    int slot_len;//len of number of slots for a particular size
    int empty;//bool if data page is empty then 1 else 0 
    free_list* pages;//pointer to free_lists of data TODO change to a list of page pointers
} header;


const size_t MAP = 10000000;
const size_t PAGE_SIZE = 4096;
static int start;
//page num keeps track of the current page with an empty slot
struct header heads[20];

//finds slot in which to put given byte request
int
find_slot(size_t bytes){
    for (int ii = 0; ii<20; ii++){
        if (slots[ii] >= bytes){
            return ii;
        }
    }
    return -1;
}

int
find_slot_len(int ii){
    int bytes = slots[ii];

    return (PAGE_SIZE - sizeof(free_list))/bytes;
}

void
print_headers(){
    printf("------\n");
    for (int ii = 0; ii< 20; ii++){
        printf(" size: %d, slot_len: %d ", heads[ii].slot_size, heads[ii].slot_len);
        free_list* tmp = heads[ii].pages;
        int page_count = 0;
        //TODO INFINITE LOOP next is pointing to prev
        while(tmp){
            page_count+=1;
            tmp = tmp->next;
        }
        printf("Number of pages %d ", page_count);
        if (heads[ii].pages){
            printf("Available slots ");
            int slot_count =0;
            for (int xx = 0; xx < heads[ii].slot_len; xx++){

                if (TestBit(heads[ii].pages->slots_occupied, xx) != 0){
                    slot_count +=1;
                } 
            }
            printf("%d", heads[ii].slot_len -slot_count);
        }
        printf("\n");
       
    }
    printf("------\n");

}


//creates bitmap of len*32
int
make_bitmap(int slot_len, int* slots_occ){
    int len = slot_len/32;
    if (slot_len % 32 != 0){
        len = slot_len/32+1;
    }
    for (int ii = 0; ii< len; ii++){
        slots_occ[ii] = 0;
    }
    return len;
}


//find empty slot in a particular page else return 0
int
find_empty_bit(free_list* page, int slot_len){
    for (int jj = 0; jj < slot_len; jj++ ){
        //bit is 1 or full
        if(TestBit(page->slots_occupied, jj) !=0){
            
        //bit is 0 or empty 
        }else {
            return jj;
        }
    }
    return -1;
}


void*
assign_slot(header* head){
    //if head pointer is Null
    free_list* next = NULL;
    free_list* prev = NULL;
    if (!head->pages){
        //printf("new page \n");
        head->pages = mmap(0,PAGE_SIZE, PROT_READ| PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
        make_bitmap(head->slot_len,head->pages->slots_occupied);
        head->pages->next = NULL;
        head->pages->prev = NULL;
        head->pages->slot_size = head->slot_size;
        head->pages->data = (void *) head->pages+sizeof(free_list);
        head->pages->occupied_count = 1;
        SetBit(head->pages->slots_occupied, 0);//first slot becomes occupied for an empty page
        //no longer empty 
        head->empty = 0;
        return head->pages->data;//return pointer to data
    //data exists search for empty slot
    } else{
        //look through all activated pages to find an empty slot
        int empty_space = -1;
        free_list* current_page = head->pages;
        //look for an open slot in the pages
        while(current_page){
            //remove every page that does not have an empty slot from the free_list
            empty_space = find_empty_bit(current_page,head->slot_len);
            //exit once slot is found
            if (empty_space != -1){
                break;
            } else {
                //remove item from free list since it has no free slots
                next = current_page->next;
                prev = current_page->prev;
                current_page->next = NULL;
                current_page->prev = NULL;

                //pages is one page long, remove that page
                if (!(next || prev)){
                    head->pages = NULL;
                //pages is more than one page, remove first page
                } else if (!prev){
                    next->prev= NULL;
                    head->pages = next;
                //pages is more than one page, remove last page
                } else if (!next){
                    prev->next = NULL;
                } else {
                    next->prev = prev;
                    prev->next = next;
                }
            }
            //cycle next item 
            if (next){
                current_page = next;
            } else {
                break;
            }
        }

        //no empty page in any of the pages
        if (empty_space == -1){
            //initialize new page make the last page point to this one.
            head->pages = mmap(0,PAGE_SIZE, PROT_READ| PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
            //printf("new page %d \n", head->pages->slot_len);
            make_bitmap(head->slot_len, head->pages->slots_occupied);
            head->pages->next = NULL;
            head->pages->prev = NULL;
            head->pages->slot_size = head->slot_size;
            head->pages->data = (void*) head->pages+ sizeof(free_list);
            head->pages->occupied_count = 1;
            SetBit(head->pages->slots_occupied, 0);//first slot becomes occupied for an empty page
            return head->pages->data;
        } else  {
            //return pointer pointing to empty slot
            //printf("head pointer %p data pointer %p return pointer %p \n", (void*) head->pages, (void*) current_page->data, (void *) current_page->data + empty_space*head->slot_size);
            current_page->occupied_count += 1;
            SetBit(current_page->slots_occupied, empty_space);
            return (void *) current_page->data + empty_space*head->slot_size;
        }
    }
}

static
size_t
div_up(size_t xx, size_t yy)
{
     // This is useful to calculate # of pages
     // for large allocations.
     size_t zz = xx / yy;

     if (zz * yy == xx) {
         return zz;
     }
     else {
         return zz + 1;
     }
}

void*
big_alloc(int bytes){
    //printf("bytes %d\n", bytes);
    size_t pages = div_up(bytes,PAGE_SIZE);
    int map = (pages+1)*PAGE_SIZE;
    void* mem = mmap(0,map, PROT_READ| PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
    free_list* head = mem;
    //printf("mem %p data add %p \n", mem, head);
    head->next = NULL;
    head->slot_size = map;
    head->prev = NULL;
    head->data = (void*)head + sizeof(free_list);
    return head->data;

}

void* 
xmalloc(size_t bytes)
{
    pthread_mutex_lock(&lock);
    void* mem;
    //initialize header 
    if (start == 0){
        start = 1;
        for (int ii = 0; ii<20; ii ++){
            heads[ii].slot_size = slots[ii];
            heads[ii].slot_len = find_slot_len(ii);
            heads[ii].empty = 1;
            heads[ii].pages = NULL;
        }
    }

    int index = find_slot(bytes);//holds index of slot for data
    if (index != -1){
        mem = assign_slot(&heads[index]);
    } else {
        mem = big_alloc(bytes);
    }
    
    pthread_mutex_unlock(&lock);
    return mem;
    // TODO: write an optimized malloc
    //return 0;
}

void
xfree(void* ptr)
{
    pthread_mutex_lock(&lock);
    free_list* page = (free_list*) ((uintptr_t)ptr - ((uintptr_t)ptr %PAGE_SIZE));
    if (page->slot_size>PAGE_SIZE){
        munmap(page, page->slot_size);
    } else {
        int empty_space = (ptr - (void*) page->data)/page->slot_size;
        int ii = find_slot(page->slot_size);
        //printf("free %d empty space %d\n", page->slot_size, empty_space);
        free_list* current_page = heads[ii].pages;
        //printf("empty_space %d \n", empty_space/page->slot_size);
        page->occupied_count -= 1;
        ClearBit(page->slots_occupied, empty_space);
        if (page->occupied_count == 0){
            //printf("remove page from stack\n");
            if (page->prev){
                page->prev->next = page->next;
            } else {
                heads[ii].pages = page->next;
                if (page->next){
                    page->next->prev = NULL;
                } 
            }
            if (page->next){
                page->next->prev = page->prev;
            } else {
                if (page->prev){
                    page->prev->next= NULL;
                } else {
                    heads[ii].pages = NULL;
                }
            }
            munmap(page, PAGE_SIZE);
        } else if (current_page){
            //check pages in free list to see if page exists
            while(current_page){
                //page is in the free list
                if (current_page == page){
                    break;
                }
                
                if (current_page == current_page->next){
                    printf("exit \n");
                    exit(-1);
                }
                if ((uintptr_t) current_page == -1){
                    printf("current page %p current page next %p \n",current_page,current_page->next);
                }
                if(current_page->next){
                    current_page = current_page->next;
                //append page to next of current_page
                } else {
                    current_page->next = page;
                    page->prev = current_page;
                    break;
                }
            }
            //if ptr is not on free list then page must be full

        } else{
            heads[ii].pages = page;
            page->next = NULL;
            page->prev = NULL;
        }
    }


    
    pthread_mutex_unlock(&lock);
    //for (int ii =0; ii< pa

}

void*
xrealloc(void* prev, size_t bytes)
{
    void* new = xmalloc(bytes);
    pthread_mutex_lock(&lock);
    free_list* head = (free_list*) ((uintptr_t)prev - ((uintptr_t)prev %PAGE_SIZE));
    //printf("slot size %d, occupied count %d", head->slot_size, head->occupied_count);
    if (head->slot_size > bytes){
        memcpy((void*) new, prev, bytes);
        //printf(" > new %p prev %p bytes %zu slot size %d \n",new, prev, bytes, head->slot_size);
    } else {
        memcpy((void*) new, prev,head->slot_size);
        //printf(" < new %p prev %p bytes %zu slot size %d \n",new, prev, bytes, head->slot_size);
    }
    pthread_mutex_unlock(&lock);
    xfree(prev);
    return (void*) new;
}




