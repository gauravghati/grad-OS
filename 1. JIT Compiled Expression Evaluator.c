/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * jitc.c
 */

/**
 * Needs:
 *   fork()
 *   execv()
 *   waitpid()
 *   WIFEXITED()
 *   WEXITSTATUS()
 *   dlopen()
 *   dlclose()
 *   dlsym()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include "jitc.h"
#include "system.h"

struct jitc {
    void *module_handle;
};

int jitc_compile(const char *input, const char *output) {
    pid_t child_pid = fork();

    if (child_pid == -1) {
        TRACE("Fork failed");
        return -1;
    }

    if (child_pid == 0) {
        const char *args[] = {"/usr/bin/gcc", "-ansi", "-O3", "-fpic", "-shared", NULL, "-o", NULL, NULL};
        args[5] = input;
        args[7] = output;

        execv("/usr/bin/gcc",(char *const *)args);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                TRACE("Compilation failed");
                return exit_status;
            }
        } else {
            TRACE("Child process did not exit normally");
            return -1;
        }
    }
    return 0;
}

struct jitc *jitc_open(const char *pathname) {
    struct jitc *jitc = (struct jitc *)malloc(sizeof(struct jitc));
    if (!jitc) {
        TRACE("Failed to allocate memory for struct jitc");
        return NULL;
    }

    jitc->module_handle = dlopen(pathname, RTLD_LAZY | RTLD_LOCAL);
    if (!jitc->module_handle) {
        TRACE(dlerror());
        FREE(jitc);
        return NULL;
    }

    return jitc;
}

void jitc_close(struct jitc *jitc) {
    if (jitc) {
        if (jitc->module_handle) {
            dlclose(jitc->module_handle);
        }
        FREE(jitc);
    }
}

long jitc_lookup(struct jitc *jitc, const char *function_name) {
    long *function_address = dlsym(jitc->module_handle, function_name);

    if (!function_address) {
        TRACE(dlerror());
        return 0;
    }
    return (long)function_address;
}
