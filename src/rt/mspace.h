//
//  Copyright (C) 2022  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _RT_MSPACE_H
#define _RT_MSPACE_H

#include "prim.h"

#define MPTR_INVALID NULL
typedef struct _mptr *mptr_t;

typedef void *UNSAFE_MPTR;

typedef void (*mspace_oom_fn_t)(mspace_t *, size_t);

#define TLAB_SIZE (64 * 1024)

// The code generator knows the layout of this struct
typedef struct _tlab {
   mspace_t *mspace;
   uint32_t  alloc;
   uint32_t  limit;
   char      data[0];
} tlab_t;

#define tlab_reset(t) do {                      \
      assert((t)->alloc <= (t)->limit);         \
      (t)->alloc = 0;                           \
   } while (0)

#define tlab_mark(t) (t)->alloc                 \

#define tlab_trim(t, mark) do {                 \
      assert((t)->alloc >= (mark));             \
      (t)->alloc = (mark);                      \
   } while (0)

mspace_t *mspace_new(size_t size);
void mspace_destroy(mspace_t *m);
void *mspace_alloc(mspace_t *m, size_t size);
void *mspace_alloc_array(mspace_t *m, int nelems, size_t size);
void *mspace_alloc_flex(mspace_t *m, size_t fixed, int nelems, size_t size);
void mspace_set_oom_handler(mspace_t *m, mspace_oom_fn_t fn);
void *mspace_find(mspace_t *m, void *ptr, size_t *size);

tlab_t *tlab_acquire(mspace_t *m);
void tlab_release(tlab_t *t);
void *tlab_alloc(tlab_t *t, size_t size);

mptr_t mptr_new(mspace_t *m, const char *name);
void mptr_free(mspace_t *m, mptr_t *ptr);
void **mptr_get(mptr_t ptr);

#define MSPACE_CURRENT_FRAME __builtin_frame_address(0)

void mspace_stack_limit(void *limit);

#endif   // _RT_MSPACE_H
