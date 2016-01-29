//------------------------------------------------------------------------------
// Copyright (c) 2016 by Lukasz Janyst <lukasz@jany.st>
//------------------------------------------------------------------------------
// This file is part of thread-bites.
//
// thread-bites is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// thread-bites is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with thread-bites.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "tb.h"

#include <linux/time.h>
#include <linux/futex.h>
#include <asm-generic/param.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

//------------------------------------------------------------------------------
// A futex for serializing the writes
//------------------------------------------------------------------------------
static void futex_lock(int *futex)
{
  while(1) {
    int val = *futex;
    if(val == 0 && __sync_bool_compare_and_swap(futex, val, 1))
      return;
    SYSCALL3(__NR_futex, futex, FUTEX_WAIT, 1);
  }
}

static void futex_unlock(int *futex)
{
  __sync_bool_compare_and_swap(futex, 1, 0);
  SYSCALL3(__NR_futex, futex, FUTEX_WAKE, 1);
}

//------------------------------------------------------------------------------
// Print unsigned int to a string
//------------------------------------------------------------------------------
static void printNum(uint64_t num, int base)
{
  if(base <= 0 || base > 16)
    return;
  if(num == 0) {
    tbwrite(1, "0", 1);
    return;
  }
  uint64_t n = num;
  char str[32]; str[31] = 0;
  char *cursor = str+30;
  char digits[] = "0123456789abcdef";
  while(n && cursor != str) {
    int rem = n % base;
    *cursor = digits[rem];
    n /= base;
    --cursor;
  }
  ++cursor;
  tbwrite(1, cursor, 32-(cursor-str));
}

//------------------------------------------------------------------------------
// Print signed int to a string
//------------------------------------------------------------------------------
static void printNumS(int64_t num)
{
  if(num == 0) {
    tbwrite(1, "0", 1);
    return;
  }
  uint64_t n = num;
  char str[32]; str[31] = 0;
  char *cursor = str+30;
  char digits[] = "0123456789";
  while(n && cursor != str) {
    int rem = n % 10;
    *cursor = digits[rem];
    n /= 10;
    --cursor;
  }
  ++cursor;
  tbwrite(1, cursor, 32-(cursor-str));
}

//------------------------------------------------------------------------------
// Print something to stdout
//------------------------------------------------------------------------------
static int print_lock;
void tbprint(const char *format, ...)
{
  futex_lock(&print_lock);
  va_list ap;
  int length = 0;
  int sz     = 0;
  int base   = 0;
  int sgn    = 0;
  const char *cursor = format;
  const char *start  = format;

  va_start(ap, format);
  while(*cursor) {
    if(*cursor == '%') {
      tbwrite(1, start, length);
      ++cursor;
      if(*cursor == 0)
        break;

      if(*cursor == 's') {
        const char *str = va_arg(ap, const char*);
        tbwrite(1, str, strlen(str));
      }

      else {
        while(*cursor == 'l') {
          ++sz;
          ++cursor;
        }
        if(sz > 2) sz = 2;

        if(*cursor == 'x')
          base = 16;
        else if(*cursor == 'u')
          base = 10;
        else if(*cursor == 'o')
          base = 8;
        else if(*cursor == 'd')
          sgn  = 1;

        if(!sgn) {
          uint64_t num;
          if(sz == 0) num = va_arg(ap, unsigned);
          else if(sz == 1) num = va_arg(ap, unsigned long);
          else num = va_arg(ap, unsigned long long);
          printNum(num, base);
        }
        else {
          int64_t num;
          if(sz == 0) num = va_arg(ap, int);
          else if(sz == 1) num = va_arg(ap, long);
          else num = va_arg(ap, long long);
          printNumS(num);
        }
        sz = 0; base = 0; sgn = 0;
      }
      ++cursor;
      start = cursor;
      length = 0;
      continue;
    }
    ++length;
    ++cursor;
  }
  if(length)
    tbwrite(1, start, length);
  va_end (ap);
  futex_unlock(&print_lock);
}

//------------------------------------------------------------------------------
// Write
//------------------------------------------------------------------------------
int tbwrite(int fd, const char *buffer, unsigned long len)
{
  return SYSCALL3(__NR_write, fd, buffer, len);
}

//------------------------------------------------------------------------------
// Sleep
//------------------------------------------------------------------------------
void tbsleep(int secs)
{
  struct timespec ts, rem;
  ts.tv_sec = secs; ts.tv_nsec = 0;
  rem.tv_sec = 0; rem.tv_nsec = 0;
  while(SYSCALL2(__NR_nanosleep, &ts, &rem) == -EINTR)
  {
    ts.tv_sec = rem.tv_sec;
    ts.tv_nsec = rem.tv_nsec;
  }
}

//------------------------------------------------------------------------------
// Time
//------------------------------------------------------------------------------
uint64_t tbtime()
{
  return SYSCALL1(__NR_time, 0);
}

//------------------------------------------------------------------------------
// Random
//------------------------------------------------------------------------------
uint32_t tbrandom(uint32_t *seed)
{
  *seed = 1103515245 * (*seed) + 12345;
  return *seed;
}

//------------------------------------------------------------------------------
// Mmap
//------------------------------------------------------------------------------
void *tbmmap(void *addr, unsigned long length, int prot, int flags, int fd,
  unsigned long offset)
{
  return (void *)SYSCALL6(__NR_mmap, addr, length, prot, flags, fd, offset);
}

//------------------------------------------------------------------------------
// Munmap
//------------------------------------------------------------------------------
int tbmunmap(void *addr, unsigned long length)
{
  return SYSCALL2(__NR_munmap, addr, length);
}

//------------------------------------------------------------------------------
// Brk
//------------------------------------------------------------------------------
void *tbbrk(void *addr)
{
  return (void *)SYSCALL1(__NR_brk, addr);
}

//------------------------------------------------------------------------------
// Malloc helper structs
//------------------------------------------------------------------------------
typedef struct memchunk
{
  struct memchunk *next;
  uint64_t         size;
} memchunk_t;

static memchunk_t  head;
static void       *heap_limit;
#define MEMCHUNK_USED 0x4000000000000000

//------------------------------------------------------------------------------
// Malloc
//------------------------------------------------------------------------------
static int memory_lock;
void *malloc(size_t size)
{
  futex_lock(&memory_lock);

  //----------------------------------------------------------------------------
  // Allocating anything less than 16 bytes is kind of pointless, the
  // book-keeping overhead is too big. We will also align to 8 bytes.
  //----------------------------------------------------------------------------
  size_t alloc_size = (((size-1)>>3)<<3)+8;
  if(alloc_size < 16)
    alloc_size = 16;

  //----------------------------------------------------------------------------
  // Try to find a suitable chunk that is unused
  //----------------------------------------------------------------------------
  memchunk_t *cursor = &head;
  memchunk_t *chunk  = 0;
  while(cursor->next) {
    chunk = cursor->next;
    if(!(chunk->size & MEMCHUNK_USED) && chunk->size >= alloc_size)
      break;
    cursor = cursor->next;
  }

  //----------------------------------------------------------------------------
  // No chunk found, ask Linux for more memory
  //----------------------------------------------------------------------------
  if (!cursor->next) {
    //--------------------------------------------------------------------------
    // We have been called for the first time and don't know the heap limit yet.
    // On Linux, the brk syscall will return the previous heap limit on error.
    // We try to set the heap limit at 0, which is obviously wrong, so that we
    // could figure out what the current heap limit is.
    //--------------------------------------------------------------------------
    if(!heap_limit)
      heap_limit = tbbrk(0);

    //--------------------------------------------------------------------------
    // We will allocate at least one page at a time
    //--------------------------------------------------------------------------
    size_t chunk_size = (size+sizeof(memchunk_t)-1)/EXEC_PAGESIZE;
    chunk_size *= EXEC_PAGESIZE;
    chunk_size += EXEC_PAGESIZE;

    void     *new_heap_limit = tbbrk((char*)heap_limit + chunk_size);
    uint64_t  new_chunk_size = (char *)new_heap_limit - (char *)heap_limit;

    if(heap_limit == new_heap_limit)
    {
      futex_unlock(&memory_lock);
      return 0;
    }

    cursor->next = heap_limit;
    chunk        = cursor->next;
    chunk->size  = new_chunk_size-sizeof(memchunk_t);
    chunk->next  = 0;
    heap_limit   = new_heap_limit;
  }

  //----------------------------------------------------------------------------
  // Split the chunk if it's big enough to contain one more header and at least
  // 16 more bytes
  //----------------------------------------------------------------------------
  if(chunk->size > alloc_size + sizeof(memchunk_t) + 16)
  {
    memchunk_t *new_chunk = (memchunk_t *)((char *)chunk+sizeof(memchunk_t)+alloc_size);
    new_chunk->size = chunk->size-alloc_size-sizeof(memchunk_t);
    new_chunk->next = chunk->next;
    chunk->next = new_chunk;
    chunk->size = alloc_size;
  }

  //----------------------------------------------------------------------------
  // Mark the chunk as used and return the memory
  //----------------------------------------------------------------------------
  chunk->size |= MEMCHUNK_USED;
  futex_unlock(&memory_lock);
  return (char*)chunk+sizeof(memchunk_t);
}

//------------------------------------------------------------------------------
// Free
//------------------------------------------------------------------------------
void free(void *ptr)
{
  if(!ptr)
    return;

  futex_lock(&memory_lock);
  memchunk_t *chunk = (memchunk_t *)((char *)ptr-sizeof(memchunk_t));
  chunk->size &= ~MEMCHUNK_USED;
  futex_unlock(&memory_lock);
}

//------------------------------------------------------------------------------
// Calloc
//------------------------------------------------------------------------------
void *calloc(size_t nmemb, size_t size)
{
  size_t alloc_size = nmemb*size;
  void *ptr = malloc(alloc_size);
  char *cptr = ptr;
  if(!ptr)
    return ptr;
  for(int i = 0; i < alloc_size; ++i, *cptr++ = 0);
  return ptr;
}

//------------------------------------------------------------------------------
// Realloc
//------------------------------------------------------------------------------
void *realloc(void *ptr, size_t size)
{
  memchunk_t *chunk = (memchunk_t *)((char *)ptr-sizeof(memchunk_t));
  void       *new_ptr = malloc(size);
  char       *s = ptr;
  char       *d = new_ptr;
  size_t      min = chunk->size > size ? size : chunk->size;

  for(int i = 0; i < min; ++i, *d++ = *s++);
  free(ptr);
  return new_ptr;
}

//------------------------------------------------------------------------------
// Heap state for diagnostics
//------------------------------------------------------------------------------
void tb_heap_state(uint64_t *total, uint64_t *allocated)
{
  memchunk_t *cursor = &head;
  memchunk_t *chunk  = 0;

  *total    = 0;
  *allocated = 0;

  while(cursor->next) {
    chunk = cursor->next;
    if(chunk->size & MEMCHUNK_USED)
      ++(*allocated);
    ++(*total);
    cursor = cursor->next;
  }
}
