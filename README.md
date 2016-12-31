Incremental Failure Injection in C
==================================

I've been programing in C for the past 20 years, and I discovered this technique about 8 years ago and it has significantly helped me reduce bugs in production code.  Most of the challenge in writing bug free C code is dealing with safe error handling.  This technique allows me to encapsulate all the error handling into a single pattern that allows me to programatically inject errors into every possible error condition.

Single Exit Point and Error Handling Macros
-------------------------------------------

Here is a simple example of an error handling macro

```C
#define TEST(err, expr) \
    if(!(expr)) {\
      if(!err) {\
        err = -1;\
      }\
      goto CHECK(err); \
    } 
  } while(0)

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
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST(err, s >= 0);
    addr.sun_family = AF_UNIX;
    TEST(err, sz <= sizeof(addr.sun_path));
    memmove(addr.sun_apth, path, sz);
    TEST(err, !bind(s, &addr, sizeof(addr)));
    *fd = s;
    s = -1;
CHECK(err):
    if(s != -1) {
        close(s);
    }
    return err;
}

The function has a single return statement at the end, returning the `err` variable.

Test with Failure Injection
----------------------------

The main reason to use a standard macro for all the error handling is give me the ability to inject a failure any time that macro is evaluated.  To do incremental failure injection I wrote a simple test library.

```C
//TOASTER, a simple test library

//return 0 only if check passes
int toaster_check(void);

//run the test with `max` number checks passing until `test` returns 0
int toaster_run(int max, int (*test)(void));
```

Each time `toaster_check` is called its counter is decremented.  When the check counter  hits `0`, `toaster_check` returns a failure. `toaster_run` initializes the check counter from 0 up to `max` until `test` returns 0.

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
int toaster_run(int max, int (*test)(void)) {
    int i;
    int err = -1;
    for(i = 0; i <= max && err != 0; ++i) {
        LOG(TRACE, "test count: %d", i);
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
      LOG(DEBUG, "inject:%s", #expr); \
      goto CHECK(err); \
    } else
#else 
#define TOASTER_INJECT_FAILURE(err, expr)
#endif

#define TEST(err, expr) \
  do {\
    LOG(TRACE, "call:%s", #expr); \
    TOASTER_INJECT_FAILURE(err, expr) \
    if(!(expr)) {\
      if(!err) {\
        err = -1;\
      }\
      LOG(DEBUG, "fail:%s", #expr); \
      goto CHECK(err); \
    } else {\
        LOG(TRACE, "pass:%s", #expr); \
    }\
  } while(0)

#define CHECK(err) __ ## err ## _test_check
```

Since the test runs with an incremental number of `TEST` macros passing we are able to verify that our `unix_sock_create_and_bind` funciton can handle and error on line `TEST(err, s >= 0);`, and `TEST(err, !bind(s, &addr, sizeof(addr)));`. 

Mock out external APIs
----------------------
GNUs dlfcn defines an `RTLD_NEXT` macro that allows you to load the next symbol in the symbol list for a particular api.  So you can write tests that override the default implementation of an externally linked api.  This is funcitonally equivalent to using LD_PRELOAD.

```C
#define _GNU_SOURCE
#include <dlfcn.h>

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int (*real)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "bind");
    if(!toaster_check()) {
        return real(sockfd, addr, addrlen);
    }
    return -1;
}
```
Since `bind` was linked into the main program, that definition will be used by default.  I can use RTLD_NEXT to find the real defintion and programatically inject a failure into that call.

Use valgrind!
-------------

```bash
valgrind --leak-check=yes --error-exitcode=5 -q ./test
```        

Valgrind is a great tool that will catch leaks and uninitialized memory access errors.  In combination with incremental failure injection, valgrind will spot any tests that have leaked memory durring a simulated failure.

