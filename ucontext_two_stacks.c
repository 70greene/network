#define _GNU_SOURCE
#include <stdio.h>
#include <ucontext.h>

int count = 0;
ucontext_t ctx[2];
ucontext_t main_ctx;

void func1(void) {
	while (count++ < 10) {
		printf("1\n");
		swapcontext(&ctx[0], &ctx[1]);
	}
}

void func2(void) {
	while (count++ < 10) {
		printf("2\n");
		swapcontext(&ctx[1], &ctx[0]);
	}
}

int main() {
	char stack1[2048] = {0};
	char stack2[2048] = {0};

	getcontext(&ctx[0]);
	ctx[0].uc_stack.ss_sp = stack1;
	ctx[0].uc_stack.ss_size = sizeof(stack1);
	ctx[0].uc_link = &main_ctx;
	makecontext(&ctx[0], func1, 0);

	getcontext(&ctx[1]);
	ctx[1].uc_stack.ss_sp = stack2;
	ctx[1].uc_stack.ss_size = sizeof(stack2);
	ctx[1].uc_link = &main_ctx;
	makecontext(&ctx[1], func2, 0);

	printf("swapcontext\n");
	swapcontext(&main_ctx, &ctx[0]);
}