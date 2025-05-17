#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Kernel/sys/dirent.h>
#include <sys/dirent.h>

#define CMD_FILE "monitor_cmd.txt"
#define TREASURE_FILE "hunt_treasures.db"

int output_fd = STDOUT_FILENO; // Default to stdout, will be overwritten by pipe

void safe_print(const char *msg) {
    dprintf(output_fd, "%s", msg);
}

// Define the Treasure structure before usage
struct Treasure {
    int id;
    char username[50];
    float lat, lon;
    char clue[256];
    int value;
};

void handle_signal(int sig) {
    FILE *f = fopen(CMD_FILE, "r");
    if (!f) {
        perror("fopen");
        return;
    }

    char cmd[128];
    fgets(cmd, sizeof(cmd), f);
    cmd[strcspn(cmd, "\n")] = 0;
    fclose(f);

    if (strcmp(cmd, "list_hunts") == 0) {
        DIR *d = opendir("hunt");
        if (!d) {
            perror("opendir");
            return;
        }
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            struct stat st;
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "hunt/%s", entry->d_name);
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                char path[256];
                snprintf(path, sizeof(path), "hunt/%s/%s", entry->d_name, TREASURE_FILE);
                FILE *tfile = fopen(path, "rb");
                int count = 0;
                if (tfile) {
                    fseek(tfile, 0, SEEK_END);
                    long size = ftell(tfile);
                    count = size / sizeof(struct Treasure); // rough estimate
                    fclose(tfile);
                }
                char line[256];
                snprintf(line, sizeof(line), "Hunt: %s - Treasures: %d\n", entry->d_name, count);
                safe_print(line);
            }
        }
        closedir(d);
    } else if (strncmp(cmd, "list_treasures", 14) == 0) {
        char hunt_id[100];
        sscanf(cmd, "list_treasures %s", hunt_id);
        char path[256];
        snprintf(path, sizeof(path), "hunt/%s/%s", hunt_id, TREASURE_FILE);
        FILE *f = fopen(path, "rb");
        if (!f) {
            perror("fopen");
            return;
        }
        struct Treasure {
            int id;
            char username[50];
            float lat, lon;
            char clue[256];
            int value;
        } t;
        while (fread(&t, sizeof(t), 1, f) == 1) {
            char line[512];
            snprintf(line, sizeof(line), "ID: %d, User: %s, Lat: %.2f, Lon: %.2f, Clue: %s, Value: %d\n",
                     t.id, t.username, t.lat, t.lon, t.clue, t.value);
            safe_print(line);
        }
        fclose(f);
    } else if (strncmp(cmd, "view_treasure", 13) == 0) {
        char hunt_id[100];
        int tid;
        sscanf(cmd, "view_treasure %s %d", hunt_id, &tid);
        char path[256];
        snprintf(path, sizeof(path), "hunt/%s/%s", hunt_id, TREASURE_FILE);
        FILE *f = fopen(path, "rb");
        if (!f) {
            perror("fopen");
            return;
        }
        struct Treasure {
            int id;
            char username[50];
            float lat, lon;
            char clue[256];
            int value;
        } t;
        int found = 0;
        while (fread(&t, sizeof(t), 1, f) == 1) {
            if (t.id == tid) {
                char line[512];
                snprintf(line, sizeof(line), "ID: %d, User: %s, Lat: %.2f, Lon: %.2f, Clue: %s, Value: %d\n",
                         t.id, t.username, t.lat, t.lon, t.clue, t.value);
                safe_print(line);
                found = 1;
                break;
            }
        }
        if (!found)
            safe_print("Treasure not found\n");
        fclose(f);
    } else if (strcmp(cmd, "stop_monitor") == 0) {
        safe_print("Stopping monitor in 2 seconds...\n");
        usleep(2000000);
        exit(0);
    } else {
        safe_print("Unknown command\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        output_fd = atoi(argv[1]);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    safe_print("Monitor ready. Waiting for commands...\n");
    while (1) {
        pause();
    }
}
