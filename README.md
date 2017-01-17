Incremental Failure Injection in C
==================================

Most of the challenge in writing bug free C code is dealing with safe error handling.  This technique encapsulates all the error handling into a single pattern that can be used to programatically inject errors into every possible error condition.  This approach is not as thorough as covering every branch by testing a large input space.  But it allows a small number of tests to cover a large set of common programming bugs.

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
.......

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

Each time `toaster_check` is called its counter is decremented.  When the check counter  hits `0`, `toaster_check` returns 0 as failure. `toaster_run` initializes the check counter from 0 up to `max` until `test` returns 0.

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
    TOASTER_LOG("inject failure: bind");
    return -1;
}
```
Since `bind` was linked into the main program, that definition will be used by default.  I can use `RTLD_NEXT` to find the real definition and programatically inject a failure into that call.

Use valgrind!
-------------

```bash
valgrind --leak-check=yes --error-exitcode=5 -q ./test
```        

Valgrind is a great tool that will catch leaks and uninitialized memory access errors.  In combination with incremental failure injection, valgrind will spot any tests that have leaked memory during a simulated failure.  Suck as a `free` of an uninitialized pointer that never got set during an error.

Running the Example
--------------------

Simple `make` should run this code on most systems that have `gcc` and `gcov` installed.

```bash
Lines executed:100.00% of 49
Branches executed:100.00% of 52
Taken at least once:84.62% of 52
Calls executed:96.67% of 30
Creating 'test.c.gcov'
```

The difference between incremental failure injection and no failure injection can be seen by modifying the `main` function to look like

```C

int main(int _argc, char * const _argv[]) {
    assert(0 == toaster_run(test_talk));
    return 0;
}
```

The resulting output should be

```bash
Lines executed:89.80% of 49
Branches executed:100.00% of 52
Taken at least once:50.00% of 52
Calls executed:86.67% of 30
Creating 'test.c.gcov'
```

License
--------

Copyright (c) 2017 <Anatoly Yakovenko> aeyakovenko@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
