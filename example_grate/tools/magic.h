/*
This is an auto-generated file. Do not modify.
*/

#include <stdlib.h>
#include <unistd.h>
#include <cp_data_between_cages.h>
#include <register_handler.h>

#define thiscage	getpid()

#define COPY(src, dst, len, type) \
    cp_data_between_cages(thiscage, (src).cage, \
                          (src).value, (src).cage, \
                          (dst).value, (dst).cage, \
                          (len), (type))

typedef struct lvar_t {
	uint64_t value;
	uint64_t cage;
} lvar_t;

struct generic_args {
	uint64_t cageid;
	lvar_t arg1;
	lvar_t arg2;
	lvar_t arg3;
	lvar_t arg4;
	lvar_t arg5;
	lvar_t arg6;
};

void grate_init(void);
lvar_t l_alloc(int);
void l_free(lvar_t);
void register_handlers(int, int);

/* Types get attached down below */
['IN', 'char*', 'pathname']
['N', 'int', 'flags']
['N', 'mode_t', 'mode']
['}']
['']
['N', 'int', 'fd']
['}']
['']
['N', 'int', 'fd']
['OUT', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['N', 'int', 'fd']
['IN', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['IN', 'char*', 'pathname']
['N', 'int', 'flags']
['N', 'mode_t', 'mode']
['}']
['']
['N', 'int', 'fd']
['}']
['']
['N', 'int', 'fd']
['OUT', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['N', 'int', 'fd']
['IN', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
int open_syscall(cageid, pathname, flags, mode);
int close_syscall(cageid, fd);
int read_syscall(cageid, fd, buf, count);
int write_syscall(cageid, fd, buf, count);
