This is an example of a grate written with the new APIs. Runs the in-memory file system grate with an example program.

### Compile

`./tools/compile.sh`

### Run

- Compile `runopen.c` into `LIND_ROOT`
- `./tools/run.sh`

### File Structure 

```
imfs_grate.c // Grate source code with wrappers for individual syscalls.
imfs.* // IMFS source code.
runopen.c // Sample cage for testing

tools/
    - wand.py // Python script that generates bindings for syscall conversions. 
    - magic.*tmpl // Template files for .h and .c files with the necessary bindings.
    - syscall_descs // List of syscalls along with the arguments required.
```

### Writing a syscall wrapper

Consider the example of the `open_grate` syscall wrapper written with the new API:

```
int _open_grate(struct open_args args) {
	uint64_t cageid = args.cageid;

	lvar_t pathname = args.pathname;
	lvar_t flags = args.flags;
	lvar_t mode = args.mode;
	
	lvar_t _path = l_alloc(256);

	// src, dst, len, type
	COPY(pathname, _path, 256, 1);

	int ifd = imfs_open(cageid, (char *)_path.value, flags.value, mode.value);

	l_free(_path);

	return ifd;
}
```

Here, `lvar_t` is:

```
struct lvar_t {
    uint64_t value;
    uint64_t cage;
};
```

And `open_args` is:

```
struct open_args {
    uint64_t cageid;
    lvar_t pathname;
    lvar_t flags;
    lvar_t mode;
    // Padding
};
```

To access the value of a given input, we use `args.<parameter>.value`. 

With this approach, we don't need to remember the order of arguments, and  we do not expose unnecessary parameters. 

Typically the use of the `argXcage` parameter is only useful for lind's runtime in order for address translations, and not for an individual syscall's implementation.

In order to copy data between cages, we can use the macro `COPY(src, dst, len, mode)` which takes as input two `lvar_t` variables. This macro internally calls the `copy_data_between_cages()` function with the source and destination cages interpolated from the arguments passed to it.

`l_alloc` and `l_free` are simple wrappers over `malloc` and `free` but they return a `lvar_t` struct instead. 

### Internals

The current approach for generating this bindings is just proof-of-concept, which can be improved significantly. It uses a python script and some template files. 

In order to convert this function into a type that `register_handler` expects, we need to generate some bindings. 

First, we need to generate the `<syscall>_args` structs. Second, we need to generate a function that does roughly the following:

```
int <syscall>_grate(uint64_t cageid, ...[argX, argXcage]...) {
    struct arguments = (<syscall_grate>) {
        .cageid = cageid,
        ...
    };
    
    return _open_grate(arguments);
}
```

Currently this is managed by the `magic.*tmpl` and the `wand.py` python scripts. This approach can be changed later. 

The compilation script first reads the `syscall_desc` file to generate the required argument structs and appends it to the `magic.h` file. It then generates the wrapper functions and appends it to `magic.c`. These files are compiled along with the source code for the grates when generating the final `.wasm` binary. 

The `syscall_desc` consists of syscall descriptions in the following format:

`open = pathname, flags, mode`
