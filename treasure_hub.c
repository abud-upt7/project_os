#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define CMD_FILE "monitor_cmd.txt"
pid_t monitor_pid = -1;
int monitor_exiting = 0;

void sigchld_handler(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
        printf("Monitor exited with status %d\n", WEXITSTATUS(status));
        monitor_exiting = 0;
        monitor_pid = -1;
    }
}

void write_command(const char *command) {
    FILE *f = fopen(CMD_FILE, "w");
    if (!f) {
        perror("fopen");
        return;
    }
    fprintf(f, "%s\n", command);
    fclose(f);
    kill(monitor_pid, SIGUSR1);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[128];
    while (1) {
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_pid != -1) {
                printf("Monitor already running.\n");
                continue;
            }
            monitor_pid = fork();
            if (monitor_pid == 0) {
                execl("./monitor", "./monitor", NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            }
            printf("Monitor started with PID %d\n", monitor_pid);
        } else if (strcmp(input, "stop_monitor") == 0) {
            if (monitor_pid == -1) {
                printf("Monitor is not running.\n");
                continue;
            }
            write_command("stop_monitor");
            monitor_exiting = 1;
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_pid != -1 || monitor_exiting) {
                printf("Cannot exit while monitor is running or stopping.\n");
                continue;
            }
            break;
        } else if (monitor_pid == -1 || monitor_exiting) {
            printf("Monitor not running or is stopping. Command blocked.\n");
        } else {
            write_command(input);
        }
    }

    return 0;
}
