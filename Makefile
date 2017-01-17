#  Makefile
# 
#  Copyright (c) 2017 <Anatoly Yakovenko> aeyakovenko@gmail.com
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.

all:

OBJS+=out/toaster.o
out/toaster.o:src/toaster.c

CEXES+=cov/test
cov/test:src/test.c out/toaster.o

COVS+=cov/test.c.cov
cov/test.c.cov:cov/test

##############################
#rules
all:$(OBJS) $(COVS)

clean:
	rm -rf out cov *.gcno *.gcda *.gcov

CFLAGS+=-Iinc -fPIC -g -Wall -Werror -O3 -std=c99

DEP_FLAGS=-MMD -MP -MF $(@:%=%.d)

OBJ_DEPS=$(OBJS:%=%.d)
-include $(OBJ_DEPS)

$(OBJS):
	@mkdir -p $(@D)
	$(CC) -o $@ -c $(filter %.c, $^) $(CFLAGS) $(DEP_FLAGS)

EXE_DEPS=$(EXES:%=%.d)
-include $(EXE_DEPS)

$(EXES):
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) $(DEP_FLAGS)

DLL_DEPS=$(DLLS:%=%.d)
-include $(DLL_DEPS)

$(DLLS):
	mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) -shared -ldl $(DEP_FLAGS)

export GCOV_PREFIX=cov
export GCOV_PREFIX_STRIP=$(words $(subst /, ,$(PWD)))

VALGRIND=$(shell which valgrind)
ifeq (,$(VALGRIND))
VALGRINDCMD=#
else
VALGRINDCMD=$(VALGRIND) --leak-check=yes --error-exitcode=5 -q
endif
GCOV:=gcov

$(COVS):
	@mkdir -p $(@D)
	$(VALGRINDCMD) $<
	mv *.gcno $(@D)/ || echo ok
	$(GCOV) -r -c -b -o $(@D) $(notdir $(@:%.cov=%)) | tee $<.cov.out
	mv *.gcov $(@D)/ || echo ok
	@grep "Branches executed:100" $<.cov.out
	@grep "Lines executed:100" $<.cov.out
	touch $@

CEXE_DEPS=$(CEXES:%=%.d)
-include $(CEXE_DEPS)

$(CEXES):
	@mkdir -p $(@D)
	$(CC) -o $@ $(filter-out %.h, $^) $(DEP_FLAGS) $(LD_FLAGS) $(CFLAGS) -coverage -ldl -DTOASTER 

$$%:;@$(call true)$(info $(call or,$$$*))
