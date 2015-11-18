TEST_PROGS := fpu_syscall vmx_syscall

all: $(TEST_PROGS)

$(TEST_PROGS): ../harness.c
$(TEST_PROGS): CFLAGS += -O2 -g

fpu_syscall: fpu_asm.S
vmx_syscall: vmx_asm.S

include ../../lib.mk

clean:
	rm -f $(TEST_PROGS) *.o
