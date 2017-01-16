/**
 * toaster.h
 *
 * Copyright (c) 2017 <Anatoly Yakovenko> aeyakovenko@gmail.com
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef TOASTER_H
#define TOASTER_H

#ifdef TOASTER_SHOW_LOG
#include <stdio.h>
#endif

#define TOASTER_NOOP (void)0

#define TOASTER_PRINTLN(format, ...) \
    fprintf(stderr, __FILE__ ":%d:" format "\n", __LINE__, ##__VA_ARGS__)

#ifdef TOASTER_SHOW_LOG
#define TOASTER_LOG(format, ...)  TOASTER_PRINTLN("toaster:" format,  ##__VA_ARGS__)
#else
#define TOASTER_LOG(format, ...)  TOASTER_NOOP
#endif

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

int toaster_check(void);
void toaster_set(int cnt);
int toaster_get();
void toaster_end(void);
int toaster_run_max(int max, int (*test)(void));
int toaster_run_range(int min, int max, int (*test)(void));
int toaster_run(int (*test)(void));


#endif //TOASTER_H
