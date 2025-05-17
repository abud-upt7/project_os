#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define CMD_FILE "monitor_cmd.txt"
pid_t monitor_pid = -1;
int monitor_exiting = 0;
int pipefd[2];

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

    // Read output from pipe
    char buffer[512];
    ssize_t n;
    close(pipefd[1]);
    while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
    close(pipefd[0]);
}

void calculate_score() {
    DIR *d = opendir("hunt");
    if (!d) {
        perror("opendir");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        char path[512];
        snprintf(path, sizeof(path), "hunt/%s", entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            int fd[2];
            pipe(fd);
            pid_t pid = fork();
            if (pid == 0) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
                char path[256];
                snprintf(path, sizeof(path), "hunt/%s/hunt_treasures.db", entry->d_name);
                execl("./score_calc", "./score_calc", path, NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            } else {
                close(fd[1]);
                char buffer[512];
                ssize_t n;
                printf("Scores for hunt %s:\n", entry->d_name);
                while ((n = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[n] = '\0';
                    printf("%s", buffer);
                }
                close(fd[0]);
                waitpid(pid, NULL, 0);
            }
        }
    }
    closedir(d);
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
            pipe(pipefd);
            monitor_pid = fork();
            if (monitor_pid == 0) {
                char fd_str[10];
                snprintf(fd_str, sizeof(fd_str), "%d", pipefd[1]);
                dup2(pipefd[1], STDOUT_FILENO);
                execl("./monitor", "./monitor", fd_str, NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            }
            close(pipefd[1]);
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
        } else if (strcmp(input, "calculate_score") == 0) {
            calculate_score();
        } else if (monitor_pid == -1 || monitor_exiting) {
            printf("Monitor not running or is stopping. Command blocked.\n");
        } else {
            write_command(input);
        }
    }

    return 0;
}
