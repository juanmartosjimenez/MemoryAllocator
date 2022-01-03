# MemoryAllocator

This project is an implementation of malloc, realloc and free in C. This memory allocator is optimized via bucket sorting and power of 2 memory allocations. So if the user requests 10 bytes via malloc, a 4096 byte page is created via mmap and a 16 byte slot is returned to the user. Metadata stores the size of the slots in the bucket at the beginning of each page. After 256 10 byte memory allocations, the 4096 byte bucket is filled and a new page is requested via mmap. This reduces the amount of system calls dramatically which leads to an improved time performance. Mutexes are used to make the memory allocator thread safe. For malloc calls bigger or equal to 4096, an mmap pointer with the size requested by the user is returned instead of using the bucket sorting approach.
</br>
For frees smaller than 4096 bytes, the free call uses the metadata at the beginning of the page to find the corresponding slot in the bucket and adds the freed slot to a struct that keeps track of freed slots. For frees greater than 4096 bytes, munmap is called.

#### Installation
1. Clone the repository
2. Make the makefile
3. Run opt_malloc for an optimized version of malloc, realloc and free
4. Run hwx_malloc for a naive implementation of malloc, realloc and free
