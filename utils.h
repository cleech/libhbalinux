/*
 * Copyright (c) 2008, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <limits.h>
#include <scsi/sg.h>
#include <hbaapi.h>
#include <vendorhbaapi.h>
#include "fc_types.h"

/*
 * Structure for tables encoding and decoding name-value pairs such as enums.
 */
struct sa_nameval {
	char        *nv_name;
	u_int32_t   nv_val;
};

/*
 * Structure for integer-indexed tables that can grow.
 */
struct sa_table {
	u_int32_t   st_size;        /* number of entries in table */
	u_int32_t   st_limit;       /* end of valid entries in table (public) */
	void        **st_table;     /* re-allocatable array of pointers */
};

/*
 * Function prototypes
 */
extern int sa_sys_read_line(const char *, const char *, char *, size_t);
extern int sa_sys_write_line(const char *, const char *, const char *);
extern int sa_sys_read_u32(const char *, const char *, u_int32_t *);
extern int sa_sys_read_u64(const char *, const char *, u_int64_t *);
extern int sa_dir_read(char *, int (*)(struct dirent *, void *), void *);
extern char *sa_strncpy_safe(char *dest, size_t len,
			     const char *src, size_t src_len);
extern const char *sa_enum_decode(char *, size_t,
				  const struct sa_nameval *, u_int32_t);
extern int sa_enum_encode(const struct sa_nameval *tp,
			const char *, u_int32_t *);
extern const char *sa_flags_decode(char *, size_t,
				   const struct sa_nameval *, u_int32_t);
extern int sa_table_grow(struct sa_table *, u_int32_t index);
extern void sa_table_destroy_all(struct sa_table *);
extern void sa_table_destroy(struct sa_table *);
extern void sa_table_iterate(struct sa_table *tp,
			void (*handler)(void *ep, void *arg), void *arg);
extern void *sa_table_search(struct sa_table *tp,
			void *(*match)(void *ep, void *arg), void *arg);

/** sa_table_init(tp) - initialize a table.
 * @param tp table pointer.
 *
 * This just clears a table structure that was allocated by the caller.
 */
static inline void sa_table_init(struct sa_table *tp)
{
	memset(tp, 0, sizeof(*tp));
}

/** sa_table_lookup(tp, index) - lookup an entry in the table.
 * @param tp table pointer.
 * @param index the index in the table to access
 * @returns the entry, or NULL if the index wasn't valid.
 */
static inline void *sa_table_lookup(const struct sa_table *tp, u_int32_t index)
{
	void *ep = NULL;

	if (index < tp->st_limit)
		ep = tp->st_table[index];
	return ep;
}

/** sa_table_lookup_n(tp, n) - find Nth non-empty entry in a table.
 * @param tp table pointer.
 * @param n is the entry number, the first non-empty entry is 0.
 * @returns the entry, or NULL if the end of the table reached first.
 */
static inline void *sa_table_lookup_n(const struct sa_table *tp, u_int32_t n)
{
	void *ep = NULL;
	u_int32_t   i;

	for (i = 0; i < tp->st_limit; i++) {
		ep = tp->st_table[i];
		if (ep != NULL && n-- == 0)
			return ep;
	}
	return NULL;
}

/** sa_table_insert(tp, index, ep) - Replace or insert an entry in the table.
 * @param tp table pointer.
 * @param index the index for the new entry.
 * @param ep entry pointer.
 * @returns index on success, or -1 if the insert failed.
 *
 * Note: if the table has never been used, and is still all zero, this works.
 *
 * Note: perhaps not safe for multithreading.  Caller can lock the table
 * externally, but reallocation can take a while, during which time the
 * caller may not wish to hold the lock.
 */
static inline int sa_table_insert(struct sa_table *tp,
				  u_int32_t index, void *ep)
{
	if (index >= tp->st_limit && sa_table_grow(tp, index) < 0)
		return -1;
	tp->st_table[index] = ep;
	return index;
}

/** sa_table_append(tp, ep) - add entry to table and return index.
 *
 * @param tp pointer to sa_table structure.
 * @param ep pointer to new entry, to be added at the end of the table.
 * @returns new index, or -1 if table couldn't be grown.
 *
 * See notes on sa_table_insert().
 */
static inline int
sa_table_append(struct sa_table *tp, void *ep)
{
	return sa_table_insert(tp, tp->st_limit, ep);
}

/** sa_table_sort(tp, compare) - sort table in place
 *
 * @param tp pointer to sa_table structure.
 * @param compare function to compare two entries.  It is called with pointers
 * to the pointers to the entries to be compared.  See qsort(3).
 */
static inline void
sa_table_sort(struct sa_table *tp, int (*compare)(const void **, const void **))
{
	qsort(tp->st_table, tp->st_limit, sizeof(void *),
		(int (*)(const void *, const void *)) compare);
}

#endif /* _UTILS_H_ */
