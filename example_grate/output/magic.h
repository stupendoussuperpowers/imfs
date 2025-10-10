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

typedef struct lchar_t {
	char *value;
	uint64_t cage;
} lchar_t;

typedef struct lvar {
	uint64_t value;
	uint64_t cage;
} lvar;

struct generic_args {
	uint64_t cageid;
	lvar arg1;
	lvar arg2;
	lvar arg3;
	lvar arg4;
	lvar arg5;
	lvar arg6;
};

void grate_init(void);
lvar alloc(int);
void register_handlers(int, int);

/* Types get attached down below */


struct open_args {
        uint64_t cageid;

        lvar pathname;
	lvar flags;
	lvar mode;

        uint64_t u6, uc6, u5, uc5, u4, uc4;
};

int _open_grate(struct open_args);

struct close_args {
        uint64_t cageid;

        lvar fd;

        uint64_t u6, uc6, u5, uc5, u4, uc4, u3, uc3, u2, uc2;
};

int _close_grate(struct close_args);
