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

#pragma once

#include <stdint.h>
#include <asm/unistd_64.h>
#include <asm-generic/errno.h>
#include <stddef.h>
#include <asm/signal.h>
#include <asm-generic/siginfo.h>
#include <linux/sched.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define TBTHREAD_MAX_KEYS 1024
#define TBTHREAD_MUTEX_NORMAL 0
#define TBTHREAD_MUTEX_ERRORCHECK 1
#define TBTHREAD_MUTEX_RECURSIVE 2
#define TBTHREAD_MUTEX_DEFAULT 0
#define TBTHREAD_CREATE_DETACHED 0
#define TBTHREAD_CREATE_JOINABLE 1
#define TBTHREAD_CANCEL_ENABLE 1
#define TBTHREAD_CANCEL_DISABLE 0
#define TBTHREAD_CANCEL_DEFERRED 1
#define TBTHREAD_CANCEL_ASYNCHRONOUS 0

#define TBTHREAD_CANCELED ((void*)-1)

#define TBTHREAD_INHERIT_SCHED 1
#define TBTHREAD_EXPLICIT_SCHED 0

#define TBTHREAD_PRIO_NONE 3
#define TBTHREAD_PRIO_INHERIT 4
#define TBTHREAD_PRIO_PROTECT 5

//------------------------------------------------------------------------------
// List struct
//------------------------------------------------------------------------------
typedef struct list {
  struct list *next;
  struct list *prev;
  void        *element;
} list_t;

//------------------------------------------------------------------------------
// Thread attirbutes
//------------------------------------------------------------------------------
typedef struct
{
  uint32_t  stack_size;
  uint8_t   joinable;
  uint8_t   sched_inherit;
  uint8_t   sched_policy;
  uint8_t   sched_priority;
} tbthread_attr_t;

//------------------------------------------------------------------------------
// Thread descriptor
//------------------------------------------------------------------------------
typedef struct tbthread
{
  struct tbthread *self;
  void *stack;
  uint32_t stack_size;
  uint32_t tid;
  void *(*fn)(void *);
  void *arg;
  void *retval;
  struct
  {
    uint64_t seq;
    void *data;
  } tls[TBTHREAD_MAX_KEYS];
  uint8_t join_status;
  uint8_t cancel_status;
  uint16_t sched_info;
  uint16_t user_sched_info;
  struct tbthread *joiner;
  list_t cleanup_handlers;
  list_t protect_mutexes;
  list_t inherit_mutexes;
  uint32_t start_status;
  uint32_t lock;
} *tbthread_t;

//------------------------------------------------------------------------------
// Mutex attributes
//------------------------------------------------------------------------------
typedef struct
{
  uint8_t type;
  uint8_t protocol;
  uint8_t prioceiling;
} tbthread_mutexattr_t;

//------------------------------------------------------------------------------
// Mutex
//------------------------------------------------------------------------------
typedef struct
{
  int        futex;
  uint8_t    type;
  uint8_t    protocol;
  uint16_t   sched_info;
  tbthread_t owner;
  uint64_t   counter;
  uint32_t   internal_futex;
} tbthread_mutex_t;

#define TBTHREAD_MUTEX_INITIALIZER {0, 0, TBTHREAD_PRIO_NONE, 0, 0, 0, 0}

//------------------------------------------------------------------------------
// Once
//------------------------------------------------------------------------------
typedef int tbthread_once_t;

#define TBTHREAD_ONCE_INIT 0

//------------------------------------------------------------------------------
// RW lock
//------------------------------------------------------------------------------
typedef struct {
  int lock;
  int writers_queued;
  int rd_futex;
  int wr_futex;
  tbthread_t writer;
  int readers;
} tbthread_rwlock_t;

#define TBTHREAD_RWLOCK_INIT {0, 0, 0, 0, 0, 0}

//------------------------------------------------------------------------------
// Condvar
//------------------------------------------------------------------------------
typedef struct {
  int lock;
  int futex;
  uint64_t waiters;
  uint64_t signal_num;
  uint64_t broadcast_seq;
  tbthread_mutex_t *mutex;
} tbthread_cond_t;

#define TBTHREAD_COND_INITIALIZER {0, 0, 0, 0, 0, 0}

//------------------------------------------------------------------------------
// General threading
//------------------------------------------------------------------------------
void tbthread_init();
void tbthread_finit();
void tbthread_attr_init(tbthread_attr_t *attr);
int tbthread_attr_setdetachstate(tbthread_attr_t *attr, int state);
int tbthread_create(tbthread_t *thread, const tbthread_attr_t *attrs,
  void *(*f)(void *), void *arg);
void tbthread_exit(void *retval);
int tbthread_detach(tbthread_t thread);
int tbthread_join(tbthread_t thread, void **retval);
int tbthread_equal(tbthread_t t1, tbthread_t t2);
int tbthread_once(tbthread_once_t *once, void (*func)(void));
int tbthread_cancel(tbthread_t thread);
void tbthread_cleanup_push(void (*func)(void *), void *arg);
void tbthread_cleanup_pop(int execute);
int tbthread_setcancelstate(int state, int *oldstate);
int tbthread_setcanceltype(int type, int *oldtype);
void tbthread_testcancel();

//------------------------------------------------------------------------------
// TLS
//------------------------------------------------------------------------------
typedef uint16_t tbthread_key_t;
tbthread_t tbthread_self();
int tbthread_key_create(tbthread_key_t *key, void (*destructor)(void *));
int tbthread_key_delete(tbthread_key_t key);
void *tbthread_getspecific(tbthread_key_t key);
int tbthread_setspecific(tbthread_key_t kay, void *value);

//------------------------------------------------------------------------------
// Mutexes
//------------------------------------------------------------------------------
int tbthread_mutexattr_init(tbthread_mutexattr_t *attr);
int tbthread_mutexattr_destroy(tbthread_mutexattr_t *attr);
int tbthread_mutexattr_gettype(const tbthread_mutexattr_t *attr, int *type);
int tbthread_mutexattr_settype(tbthread_mutexattr_t *attr, int type);

int tbthread_mutex_init(tbthread_mutex_t *mutex,
  const tbthread_mutexattr_t *attr);
int tbthread_mutex_destroy(tbthread_mutex_t *mutex);
int tbthread_mutex_lock(tbthread_mutex_t *mutex);
int tbthread_mutex_trylock(tbthread_mutex_t *mutex);
int tbthread_mutex_unlock(tbthread_mutex_t *mutex);

//------------------------------------------------------------------------------
// Scheduling
//------------------------------------------------------------------------------
int tbthread_setschedparam(tbthread_t thread, int policy, int priority);
int tbthread_getschedparam(tbthread_t thread, int *policy, int *priority);

int tbthread_attr_setschedpolicy(tbthread_attr_t *attr, int policy);
int tbthread_attr_setschedpriority(tbthread_attr_t *attr, int priority);
int tbthread_attr_setinheritsched(tbthread_attr_t *attr, int inheritsched);

int tbthread_mutexattr_setprioceiling(tbthread_mutexattr_t *attr, int ceiling);
int tbthread_mutexattr_setprotocol(tbthread_mutexattr_t *attr, int protocol);

int tbthread_mutex_getprioceiling(const tbthread_mutex_t *mutex, int *ceiling);
int tbthread_mutex_setprioceiling(tbthread_mutex_t *mutex, int ceiling,
  int *old_ceiling);

//------------------------------------------------------------------------------
// RW Lock
//-----------------------------------------------------------------------------
int tbthread_rwlock_init(tbthread_rwlock_t *rwlock);
int tbthread_rwlock_destroy(tbthread_rwlock_t *rwlock);

int tbthread_rwlock_rdlock(tbthread_rwlock_t *rwlock);
int tbthread_rwlock_wrlock(tbthread_rwlock_t *rwlock);
int tbthread_rwlock_unlock(tbthread_rwlock_t *rwlock);

int tbthread_rwlock_tryrdlock(tbthread_rwlock_t *rwlock);
int tbthread_rwlock_trywrlock(tbthread_rwlock_t *rwlock);

//------------------------------------------------------------------------------
// Condvar
//------------------------------------------------------------------------------
int tbthread_cond_broadcast(tbthread_cond_t *cond);
int tbthread_cond_signal(tbthread_cond_t *cond);
int tbthread_cond_wait(tbthread_cond_t *cond, tbthread_mutex_t *mutex);

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void tbprint(const char *format, ...);
int tbwrite(int fd, const char *buffer, unsigned long len);
void tbsleep(int secs);
void *tbmmap(void *addr, unsigned long length, int prot, int flags, int fd,
  unsigned long offset);
int tbmunmap(void *addr, unsigned long length);

int tbclone(int (*fn)(void *), void *arg, int flags, void *child_stack, ...
  /* pid_t *ptid, pid_t *ctid, void *tls */ );

void *tbbrk(void *addr);

uint64_t tbtime();
uint32_t tbrandom(uint32_t *seed);
const char *tbstrerror(int errno);

int tbsigaction(int signum, struct sigaction *act, struct sigaction *old);

//------------------------------------------------------------------------------
// Syscall interface
//------------------------------------------------------------------------------
#define SYSCALL(name, a1, a2, a3, a4, a5, a6)           \
  ({                                                    \
    long result;                                        \
    long __a1 = (long)(a1);                             \
    long __a2 = (long)(a2);                             \
    long __a3 = (long)(a3);                             \
    long __a4 = (long)(a4);                             \
    long __a5 = (long)(a5);                             \
    long __a6 = (long)(a6);                             \
    register long _a1 asm("rdi") = __a1;                \
    register long _a2 asm("rsi") = __a2;                \
    register long _a3 asm("rdx") = __a3;                \
    register long _a4 asm("r10") = __a4;                \
    register long _a5 asm("r8")  = __a5;                \
    register long _a6 asm("r9")  = __a6;                \
    asm volatile (                                      \
      "syscall\n\t"                                     \
      : "=a" (result)                                   \
      : "0" (name), "r" (_a1), "r" (_a2), "r" (_a3),    \
        "r" (_a4), "r" (_a5), "r" (_a6)                 \
      : "memory", "cc", "r11", "cx");                   \
    (long) result; })

#define SYSCALL0(name) \
  SYSCALL(name, 0, 0, 0, 0, 0, 0)
#define SYSCALL1(name, a1) \
  SYSCALL(name, a1, 0, 0, 0, 0, 0)
#define SYSCALL2(name, a1, a2) \
  SYSCALL(name, a1, a2, 0, 0, 0, 0)
#define SYSCALL3(name, a1, a2, a3) \
  SYSCALL(name, a1, a2, a3, 0, 0, 0)
#define SYSCALL4(name, a1, a2, a3, a4) \
  SYSCALL(name, a1, a2, a3, a4, 0, 0)
#define SYSCALL5(name, a1, a2, a3, a4, a5) \
  SYSCALL(name, a1, a2, a3, a4, a5, 0)
#define SYSCALL6(name, a1, a2, a3, a4, a5, a6) \
  SYSCALL(name, a1, a2, a3, a4, a5, a6)

//------------------------------------------------------------------------------
// List ops
//------------------------------------------------------------------------------
int list_add_elem(list_t *list, void *element, int front);
void list_add(list_t *list, list_t *node, int front);
void list_add_here(list_t *list, list_t *node, int (*here)(void*, void*));
void list_rm(list_t *node);
list_t *list_find_elem(list_t *list, void *element);
list_t *list_find_elem_func(list_t *list, void *element,
  int (*func)(void*, void*));
void list_for_each_elem(list_t *list, void (*func)(void *));
void list_clear(list_t *list);
