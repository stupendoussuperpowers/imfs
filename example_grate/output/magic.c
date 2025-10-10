
/*
This is an auto-generated file. Do not modify.
*/

#include <stdint.h>
#include <stdio.h>
#include "magic.h"

extern int grate_syscalls[];
extern size_t grate_syscalls_len;

lvar alloc(int size) {
	char *val = malloc(size);
	return (lvar) {
		.value = (uint64_t)val,
		.cage = thiscage,
	};
}

void register_handlers(int cageid, int grateid) {
	int ret;
	for(int i=0; i < grate_syscalls_len; i++) {
		ret = register_handler(cageid, grate_syscalls[i], 0, grateid);
		printf("Registered: %d\n", grate_syscalls[i]);
	}
}

int open_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage, uint64_t arg2, uint64_t arg2cage, uint64_t arg3, uint64_t arg3cage, uint64_t arg4, uint64_t arg4cage, uint64_t arg5, uint64_t arg5cage, uint64_t arg6, uint64_t arg6cage){

        struct open_args args = (struct open_args) {
                .cageid = cageid,
                .pathname = { .value = arg1, .cage = arg1cage },
		.flags = { .value = arg2, .cage = arg2cage },
		.mode = { .value = arg3, .cage = arg3cage }
        };

        return _open_grate(args);
}

int close_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage, uint64_t arg2, uint64_t arg2cage, uint64_t arg3, uint64_t arg3cage, uint64_t arg4, uint64_t arg4cage, uint64_t arg5, uint64_t arg5cage, uint64_t arg6, uint64_t arg6cage){

        struct close_args args = (struct close_args) {
                .cageid = cageid,
                .fd = { .value = arg1, .cage = arg1cage }
        };

        return _close_grate(args);
}

