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

//------------------------------------------------------------------------------
// Thread attirbutes
//------------------------------------------------------------------------------
typedef struct
{
  uint32_t  stack_size;
} tbthread_attr_t;

//------------------------------------------------------------------------------
// Thread descriptor
//------------------------------------------------------------------------------
typedef struct tbthread
{
  void *stack;
  uint32_t stack_size;
  void *(*fn)(void *);
  void *arg;
} *tbthread_t;

//------------------------------------------------------------------------------
// Function prototypes, look in the c files for descriptions
//------------------------------------------------------------------------------
void tbthread_attr_init(tbthread_attr_t *attr);
int tbthread_create(tbthread_t *thread, const tbthread_attr_t *attrs,
  void *(*f)(void *), void *arg);

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------
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
