/**
 * test.c
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "toaster.h"

int unix_sock_create_and_bind(const char *path, int *fd) {
    int err = 0;
    struct sockaddr_un addr = {};
    size_t sz = strlen(path);
    int s = socket(AF_UNIX, SOCK_DGRAM, 0);
    TEST(err, s >= 0);
    addr.sun_family = AF_UNIX;
    TEST(err, sz <= sizeof(addr.sun_path));
    memmove(addr.sun_path, path, sz);
    TEST(err, !bind(s, (struct sockaddr *)&addr, sizeof(addr)));
    *fd = s;
    s = -1;
CHECK(err):
    if(s != -1) {
        close(s);
    }
    return err;
}

int test_talk(void) {
    int err = 0;
    int a = -1, b = -1;
    socklen_t len;
    struct sockaddr_un addr;
    const char *send = "hello world";
    char recv[sizeof("hello world")] = {};
    TEST(err, !unix_sock_create_and_bind("foo", &a));
    TEST(err, !unix_sock_create_and_bind("bar", &b));

    memmove(addr.sun_path, "bar", strlen("bar"));
    TEST(err, !sendto(a, send, strlen(send), 0, (struct sockaddr*)&addr, sizeof(addr)));
    TEST(err, !recvfrom(b, recv, sizeof(recv), 0, (struct sockaddr*)&addr, &len));

    assert(len == strlen("foo"));
    assert(0 == memcmp(addr.sun_path, "foo", strlen("foo")));
    assert(0 == memcmp(send, recv, strlen(send)));

CHECK(err):
    if(-1 != a) {
        close(a);
    }
    if(-1 != b) {
        close(b);
    }
    return err;
}

int main(int _argc, char * const _argv[]) {
    assert(0 == toaster_run(1000, test_talk));
    return 0;
}
