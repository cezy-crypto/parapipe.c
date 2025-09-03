#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_CMDS 32
#define MAX_LINE 1024

struct worker_args {
    int id;
    int num_cmds;
    char ***commands;
};

// Receiver thread: prints results from worker output
void *receiver_thread(void *arg) {
    int fd = *(int *)arg;
    char buffer[MAX_LINE];
    ssize_t n;

    while (1) {
        n = read(fd, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        } else if (n == 0) {
            break; // EOF
        } else {
            usleep(1000); // non-blocking wait
        }
    }
    return NULL;
}

// Worker thread: executes the command pipeline for one input stream
void *worker_thread(void *arg) {
    struct worker_args *w = (struct worker_args *)arg;
    int pipes[MAX_CMDS+1][2];
    pid_t pids[MAX_CMDS];

    // Create pipes for commands
    for (int i = 0; i <= w->num_cmds; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(1);
        }
    }

    // Launch processes for each command
    for (int i = 0; i < w->num_cmds; i++) {
        if ((pids[i] = fork()) == 0) {
            // Child
            if (i == 0) {
                dup2(pipes[0][0], STDIN_FILENO);
            } else {
                dup2(pipes[i][0], STDIN_FILENO);
            }

            if (i == w->num_cmds-1) {
                dup2(pipes[w->num_cmds][1], STDOUT_FILENO);
            } else {
                dup2(pipes[i+1][1], STDOUT_FILENO);
            }

            // Close all pipes
            for (int j = 0; j <= w->num_cmds; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(w->commands[i][0], w->commands[i]);
            perror("execvp");
            exit(1);
        }
    }

    // Parent closes unused pipe ends
    for (int i = 0; i <= w->num_cmds; i++) {
        if (i != 0) close(pipes[i][0]);
        if (i != w->num_cmds) close(pipes[i][1]);
    }

    // Feed stdin to first pipe
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), stdin)) {
        write(pipes[0][1], line, strlen(line));
    }
    close(pipes[0][1]);

    // Receiver thread for final output
    pthread_t recv;
    fcntl(pipes[w->num_cmds][0], F_SETFL, O_NONBLOCK);
    pthread_create(&recv, NULL, receiver_thread, &pipes[w->num_cmds][0]);

    pthread_join(recv, NULL);

    for (int i = 0; i < w->num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }
    return NULL;
}

// Parse command string into array of commands
int parse_commands(char *cmdline, char ***commands) {
    char *cmds[MAX_CMDS];
    int count = 0;

    char *token = strtok(cmdline, "->");
    while (token && count < MAX_CMDS) {
        while (*token == ' ') token++; // trim leading spaces
        cmds[count++] = token;
        token = strtok(NULL, "->");
    }

    for (int i = 0; i < count; i++) {
        char *part = strdup(cmds[i]);
        char *arg;
        int argc = 0;
        char **argv = malloc(64 * sizeof(char *));
        arg = strtok(part, " \t\n");
        while (arg) {
            argv[argc++] = strdup(arg);
            arg = strtok(NULL, " \t\n");
        }
        argv[argc] = NULL;
        commands[i] = argv;
    }

    return count;
}

int main(int argc, char *argv[]) {
    int nthreads = 1;
    char *cmdline = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            nthreads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i+1 < argc) {
            cmdline = argv[++i];
        }
    }

    if (!cmdline) {
        fprintf(stderr, "Usage: %s -n num_threads -c \"command -> command ...\"\n", argv[0]);
        exit(1);
    }

    char **commands[MAX_CMDS];
    int num_cmds = parse_commands(cmdline, commands);

    pthread_t threads[nthreads];
    struct worker_args args[nthreads];

    for (int i = 0; i < nthreads; i++) {
        args[i].id = i;
        args[i].num_cmds = num_cmds;
        args[i].commands = commands;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}


