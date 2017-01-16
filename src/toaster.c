/**
 * toaster.c
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
#define _GNU_SOURCE
#define TOASTER_SHOW_LOG
#include "toaster.h"

static int gcnt;
static int gset;

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

int toaster_get(void) {
    if(gset) {
        return gcnt;
    }
    return -1;
}

void toaster_end(void) {
    gcnt = 0;
    gset = 0;
}

int toaster_run(int (*test)(void)) {
    return test();
}

int toaster_run_max(int max, int (*test)(void)) {
    return toaster_run_range(0, max, test);
}

int toaster_run_range(int min, int max, int (*test)(void)) {
    int i;
    int err = -1;
    for(i = min; i <= max && err != 0; ++i) {
        TOASTER_LOG("test count: %d", i);
        toaster_set(i);
        err = test(); 
    }
    toaster_end();
    return err;
}
