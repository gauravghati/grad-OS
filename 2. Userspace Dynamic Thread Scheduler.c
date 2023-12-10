/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

/**
 * Needs:
 *   setjmp()
 *   longjmp()
 */

#undef _FORTIFY_SOURCE

#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "system.h"
#include "scheduler.h"

struct thread {
	jmp_buf ctx;
	enum {
        STATUS_,
        STATUS_RUNNING,
        STATUS_SLEEPING,
        STATUS_TERMINATED
	} status;

	struct stack {
        void *memory_base;		
        void *memory;
	} stack; 

	scheduler_fnc_t function;
	void *args;

    struct thread *next;
};

static struct {
	struct thread *head;
	struct thread *curr_thread;
	jmp_buf ctx;
} state;

int scheduler_create(scheduler_fnc_t fnc, void *arg) {
	size_t pg_size = page_size();
	
	struct thread* new_thread = (struct thread*)malloc(sizeof(struct thread));

	if(!new_thread)
		EXIT("Error Creating the new thread memory !!");

	new_thread->status = STATUS_;
	new_thread->function = fnc;
	new_thread->args = arg;
	new_thread->stack.memory_base = (void *)malloc(1024*1024);

	if(!new_thread->stack.memory_base)
		EXIT("Error creating the stack memory");

	new_thread->stack.memory = memory_align(new_thread->stack.memory_base, pg_size);
	new_thread->next = NULL;

	if(state.head == NULL) {
        state.head = new_thread;
        state.curr_thread = state.head;
    } else {
		struct thread *curr = state.head;
		new_thread->next = curr;
		state.head = new_thread;
	}

	return 0;
}

struct thread *thread_candidate(void) {
	struct thread *curr = state.curr_thread->next;

	while(1) {
		if(curr && (curr->status == STATUS_ || curr->status == STATUS_SLEEPING))
			return curr;

		if(curr == NULL)
			curr = state.head;
		else curr = curr->next;

		if(state.curr_thread && curr == state.curr_thread && state.curr_thread->status == STATUS_TERMINATED) {
			printf("\nAll processes terminated!!\n");
			return NULL;
		}
	}

	return NULL;
}

void schedule (void) {
	struct thread *candidate = thread_candidate();

	if(!candidate)
		return;

	if(candidate->status == STATUS_) {
		void *rsp = candidate->stack.memory;
		__asm__ volatile("mov %[rs], %%rsp \n" : [rs] "+r"(rsp)::);

		state.curr_thread = candidate;
		candidate->status = STATUS_RUNNING;
		candidate->function(candidate->args);
		candidate->status = STATUS_TERMINATED;
		longjmp(state.ctx, 1);
	}
	
	else if(candidate->status == STATUS_SLEEPING) {
		state.curr_thread = candidate;
		state.curr_thread->status = STATUS_RUNNING;
		longjmp(candidate->ctx, 1);
	}
}

static void destroy (void) {
	while(state.head) {
		struct thread *curr = state.head;
		state.head = state.head->next;
		curr->next = NULL;
		FREE(curr->stack.memory_base);
		FREE(curr);
	}
	state.curr_thread=NULL;
	state.head=NULL;
	FREE(state.curr_thread);
	FREE(state.head);
}

void scheduler_execute(void) {
	setjmp(state.ctx); 
	schedule();
	destroy();
}

void scheduler_yield(void) {
	if(0 != setjmp(state.curr_thread->ctx))
		return;
	state.curr_thread->status = STATUS_SLEEPING;
	longjmp(state.ctx, 1);
}
