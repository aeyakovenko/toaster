Incremental Failure Injection in C
==================================

Most of the challenge in writing bug free C code is dealing with safe error handling.  This technique encapsulates all the error handling into a single pattern that can be used to programatically inject errors into every possible error condition.  This approach is not as thorough as covering every branch by testing a large input space, but it allows a small number of tests to cover a large set of common programming bugs, especially when combined with valgrind.

Just run make to try it.

```bash
$ make
cc -o cov/test src/test.c out/toaster.o -MMD -MP -MF cov/test.d  -Iinc -fPIC -g -Wall -Werror -O3 -std=c99 -coverage -ldl -DTOASTER
/usr/bin/valgrind --leak-check=yes --error-exitcode=5 -q cov/test
src/toaster.c:66:toaster:test count: 0
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:81:toaster:inject:!unix_sock_create_and_bind("foo", &a)
```
```bash
src/toaster.c:66:toaster:test count: 1
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:51:toaster:mock failure: socket
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:inject:s >= 0
src/test.c:81:toaster:fail:!unix_sock_create_and_bind("foo", &a)
```
```bash
src/toaster.c:66:toaster:test count: 2
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:inject:s >= 0
src/test.c:81:toaster:fail:!unix_sock_create_and_bind("foo", &a)
```
```bash
src/toaster.c:66:toaster:test count: 3
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:inject:sz <= sizeof(addr.sun_path)
src/test.c:81:toaster:fail:!unix_sock_create_and_bind("foo", &a)
```
...
```bash
gcov -r -c -b -o cov test.c | tee cov/test.cov.out
File 'src/test.c'
Lines executed:100.00% of 49
Branches executed:100.00% of 52
Taken at least once:84.62% of 52
Calls executed:96.67% of 30
Creating 'test.c.gcov'
```

How does it work?
=================

Single Exit Point and Error Handling Macros
-------------------------------------------

Many C functions are written with multiple exit points, and a wide range of error handling approaches.  If all the error handling code is standardized into a single pattern, it becomes easy to follow and programatically control.  Here is a simple example of an error handling macro

```C
/**
 * if expr is false, goto the CHECK(err) label.  Set err to -1 if it's 0.
 */
#define TEST(err, expr) \
    if(!(expr)) {\
      if(!err) {\
        err = -1;\
      }\
      goto CHECK(err); \
    } 
  } while(0)

/**
 * CHECK(err) label.  Should alwasy be at the end of the function before cleanup routines.
 */
#define CHECK(err) __ ## err ## _test_check
```

The `TEST(err, expr)` macro tests if `expr` is true, if not it will `goto` to a label that is generated using the `err` variable name. 

The `CHECK(err)` macro generates the `goto` label out of the `err` name.

Example
-------

Simple function to create a unix tcp socket and bind to it.

```C
int unix_sock_create_and_bind(const char *path, int *fd) {
    int err = 0;
    struct sockaddr_un addr = {};
    size_t sz = strlen(path);
    int s = socket(AF_UNIX, SOCK_DGRAM, 0);
    TEST(err, s >= 0);
    addr.sun_family = AF_UNIX;
    TEST(err, sz <= sizeof(addr.sun_path));
    memmove(addr.sun_apth, path, sz);
    TEST(err, !bind(s, (struct sockaddr*)&addr, sizeof(addr)));
    *fd = s;
    s = -1;
CHECK(err):
    if(s != -1) {
        close(s);
    }
    return err;
}
```

The function has a single return statement at the end, returning the `err` variable.  Everything after the `CHECK(err)` label is cleanup code.  Follwing this pattern makes it really easy to spot the exit points in the function (there is only one at the end), the cleanup code, and all the possible error conditions.

Test with Failure Injection
----------------------------

A simple counter can then be used to incrementally inject errors.  This is a library wrapping a counter for the maximum number of successes each test iteration will allow.

```C
/**
 * TOASTER, a simple library for incremental failure injection
 */

/**
 * @retval, return 0 only if check passes
 */
int toaster_check(void);

/**
 * run the test from 0 to `max` number checks passing until `test` returns 0
 * @retval 0, if test returned 0
 */
int toaster_run_max(int max, int (*test)(void));
```

Each time `toaster_check` is called its counter is decremented.  When the `toaster_check` counter hits `0`, `toaster_check` will return -1 as a failure. `toaster_run_max` runs the `test` in a loop, with the `toaster_check` counter set from 0 up to `max` until the `test` succeeds.

```C
int gcnt;
int gset;

int toaster_check(void) {
   if(gset && --gcnt < 0) {
       return -1;
   }
   return 0;
}
void toaster_set(int cnt) {
    gcnt = cnt;
    gset = 1;
}
void toaster_end(void) {
    gcnt = 0;
    gset = 0;
}
int toaster_run_max(int max, int (*test)(void)) {
    int i;
    int err = -1;
    for(i = 0; i <= max && err != 0; ++i) {
        TOASTER_LOG("test count: %d", i);
        //set the toaster_check counter to i
        toaster_set(i);
        err = test(); 
    }
    toaster_end();
    return err;
}
```

Now we can inject the `toaster_check` calls into every call to the `TEST` macro.

```C
#ifdef TOASTER
#define TOASTER_INJECT_FAILURE(err, expr) \
    if(0 != toaster_check()) {\
      if(!err) {\
        err = -1;\
      }\
      TOASTER_LOG("inject:%s", #expr); \
      goto CHECK(err); \
    } else
#else 
#define TOASTER_INJECT_FAILURE(err, expr)
#endif

#define TEST(err, expr) \
  do {\
    TOASTER_LOG("call:%s", #expr); \
    TOASTER_INJECT_FAILURE(err, expr) \
    if(!(expr)) {\
      if(!err) {\
        err = -1;\
      }\
      TOASTER_LOG("fail:%s", #expr); \
      goto CHECK(err); \
    } else {\
        TOASTER_LOG("pass:%s", #expr); \
    }\
  } while(0)

#define CHECK(err) __ ## err ## _test_check
```

Since the test runs with an incremental number of `TEST` macros passing we are able to verify that our `unix_sock_create_and_bind` function can handle and error on line `TEST(err, s >= 0);`, and `TEST(err, !bind(s, &addr, sizeof(addr)));`. 

Mock out external APIs
----------------------
GNUs dlfcn defines an `RTLD_NEXT` macro that allows you to load the next symbol in the symbol list for a particular api.  So you can write tests that override the default implementation of an externally linked api.  This is functionally equivalent to using `LD_PRELOAD`.

```C
#define _GNU_SOURCE
#include <dlfcn.h>

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int (*real)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "bind");
    if(!toaster_check()) {
        return real(sockfd, addr, addrlen);
    }
    TOASTER_LOG("mock failure: bind");
    return -1;
}
```
Since `bind` was linked into the main program, that definition will be used by default.  I can use `RTLD_NEXT` to find the real definition and programatically inject a failure into that call.

Use valgrind!
-------------

```bash
valgrind --leak-check=yes --error-exitcode=5 -q ./test
```        

Valgrind is a great tool that will catch leaks and uninitialized memory access errors.  In combination with incremental failure injection, valgrind will spot any tests that have leaked memory during a simulated failure.  Such as a `free` of an uninitialized pointer that never got set during an error.

Running the Example
--------------------

Simple `make` should run this code on most systems that have `gcc`, `gcov` and optionally `valgrind` installed.  To build with specific versions of `gcc` and `gcov` you can run `make CC=gcc-6 GCOV=gcov-6`.

```bash
$ make
Lines executed:100.00% of 49
Branches executed:100.00% of 52
Taken at least once:84.62% of 52
Calls executed:96.67% of 30
Creating 'test.c.gcov'
```

Each iteration of the test will inject a failure at the next failure point.

```bash
src/toaster.c:66:toaster:test count: 3
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:inject:sz <= sizeof(addr.sun_path)
```

```bash
src/toaster.c:66:toaster:test count: 4
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:pass:sz <= sizeof(addr.sun_path)
src/test.c:64:toaster:call:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:64:toaster:inject:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
```

```bash
src/toaster.c:66:toaster:test count: 5
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:pass:sz <= sizeof(addr.sun_path)
src/test.c:64:toaster:call:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:41:toaster:mock failure: bind
```

Until the test eventually passes

```bash
src/toaster.c:66:toaster:test count: 17
src/test.c:81:toaster:call:!unix_sock_create_and_bind("foo", &a)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:pass:sz <= sizeof(addr.sun_path)
src/test.c:64:toaster:call:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:64:toaster:pass:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:81:toaster:pass:!unix_sock_create_and_bind("foo", &a)
src/test.c:82:toaster:call:!unix_sock_create_and_bind("bar", &b)
src/test.c:60:toaster:call:s >= 0
src/test.c:60:toaster:pass:s >= 0
src/test.c:62:toaster:call:sz <= sizeof(addr.sun_path)
src/test.c:62:toaster:pass:sz <= sizeof(addr.sun_path)
src/test.c:64:toaster:call:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:64:toaster:pass:!bind(s, (struct sockaddr *)&addr, sizeof(addr))
src/test.c:82:toaster:pass:!unix_sock_create_and_bind("bar", &b)
src/test.c:87:toaster:call:strlen(send) == sendto(a, send, strlen(send), 0, (struct sockaddr*)&addr, sizeof(addr))
src/test.c:87:toaster:pass:strlen(send) == sendto(a, send, strlen(send), 0, (struct sockaddr*)&addr, sizeof(addr))
src/test.c:89:toaster:call:0 < recvfrom(b, recv, sizeof(recv), 0, (struct sockaddr*)&addr, &len)
src/test.c:89:toaster:pass:0 < recvfrom(b, recv, sizeof(recv), 0, (struct sockaddr*)&addr, &len)
src/test.c:91:toaster:call:len < sizeof(addr)
src/test.c:91:toaster:pass:len < sizeof(addr)
src/test.c:92:toaster:call:0 == memcmp(addr.sun_path, "foo", strlen("foo"))
src/test.c:92:toaster:pass:0 == memcmp(addr.sun_path, "foo", strlen("foo"))
src/test.c:93:toaster:call:0 == memcmp(send, recv, strlen(send))
src/test.c:93:toaster:pass:0 == memcmp(send, recv, strlen(send))
```

The difference between incremental failure injection and no failure injection can be seen by modifying the `main` function to look like

```C

int main(int _argc, char * const _argv[]) {
    assert(0 == test_talk());
    return 0;
}
```

The resulting output should be

```bash
$ make
Lines executed:89.80% of 49
Branches executed:100.00% of 52
Taken at least once:50.00% of 52
Calls executed:86.67% of 30
Creating 'test.c.gcov'
```


