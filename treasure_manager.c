#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define HUNT_DIR_PREFIX "hunt_"
#define TREASURE_FILE "treasures.db"
#define LOG_FILE "logged_hunt"
#define LOG_SYMLINK_PREFIX "logged_hunt-"

typedef struct {
    int id;
    char username[50];
    float lat, lon;
    char clue[256];
    int value;
} Treasure;

// Utility: log operations to file and create/update symlink
void log_operation(const char *hunt_id, const char *operation) {
    char logdir[256], logfile[512], linkname[256];
    snprintf(logdir, sizeof(logdir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(logfile, sizeof(logfile), "%s/%s", logdir, LOG_FILE);
    snprintf(linkname, sizeof(linkname), "%s%s", LOG_SYMLINK_PREFIX, hunt_id);

    int fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    time_t now = time(NULL);
    char logline[512];
    snprintf(logline, sizeof(logline), "%s: %s\n", ctime(&now), operation);
    write(fd, logline, strlen(logline));
    close(fd);

    // Create or update symlink in root directory
    unlink(linkname);
    symlink(logfile, linkname);
}

// Add a new treasure to the hunt
void add_treasure(const char *hunt_id) {
    char dir[256], file[512];
    snprintf(dir, sizeof(dir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(file, sizeof(file), "%s/%s", dir, TREASURE_FILE);
    mkdir(dir, 0755);

    Treasure t;
    printf("Enter Treasure ID (int): "); scanf("%d", &t.id);
    printf("Enter username: "); scanf("%s", t.username);
    printf("Enter latitude: "); scanf("%f", &t.lat);
    printf("Enter longitude: "); scanf("%f", &t.lon);
    printf("Enter clue: "); getchar(); fgets(t.clue, sizeof(t.clue), stdin);
    t.clue[strcspn(t.clue, "\n")] = 0;
    printf("Enter value: "); scanf("%d", &t.value);

    int fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) { perror("open"); return; }
    write(fd, &t, sizeof(t));
    close(fd);

    char op[512]; snprintf(op, sizeof(op), "Added treasure ID %d", t.id);
    log_operation(hunt_id, op);
    printf("Treasure added to hunt %s\n", hunt_id);
}

// List all treasures in the hunt
void list_treasures(const char *hunt_id) {
    char dir[256], file[512];
    snprintf(dir, sizeof(dir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(file, sizeof(file), "%s/%s", dir, TREASURE_FILE);

    struct stat st;
    if (stat(file, &st) < 0) { perror("stat"); return; }
    printf("Hunt: %s\n", hunt_id);
    printf("File size: %ld bytes\n", st.st_size);
    printf("Last modified: %s", ctime(&st.st_mtime));

    int fd = open(file, O_RDONLY);
    if (fd < 0) { perror("open"); return; }
    Treasure t;
    printf("\nTreasures:\n");
    while (read(fd, &t, sizeof(t)) == sizeof(t)) {
        printf("ID: %d, User: %s, Lat: %.2f, Lon: %.2f, Clue: %s, Value: %d\n",
            t.id, t.username, t.lat, t.lon, t.clue, t.value);
    }
    close(fd);

    log_operation(hunt_id, "Listed treasures");
}

// View a specific treasure
void view_treasure(const char *hunt_id, int tid) {
    char dir[256], file[512];
    snprintf(dir, sizeof(dir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(file, sizeof(file), "%s/%s", dir, TREASURE_FILE);

    int fd = open(file, O_RDONLY);
    if (fd < 0) { perror("open"); return; }
    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(t)) == sizeof(t)) {
        if (t.id == tid) {
            printf("ID: %d, User: %s, Lat: %.2f, Lon: %.2f, Clue: %s, Value: %d\n",
                t.id, t.username, t.lat, t.lon, t.clue, t.value);
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found) printf("Treasure not found\n");

    char op[512]; snprintf(op, sizeof(op), "Viewed treasure ID %d", tid);
    log_operation(hunt_id, op);
}

// Remove a specific treasure (by rewriting the file)
void remove_treasure(const char *hunt_id, int tid) {
    char dir[256], file[512], tmpfile[512];
    snprintf(dir, sizeof(dir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(file, sizeof(file), "%s/%s", dir, TREASURE_FILE);
    snprintf(tmpfile, sizeof(tmpfile), "%s/%s.tmp", dir, TREASURE_FILE);

    int fd = open(file, O_RDONLY);
    int tfd = open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || tfd < 0) { perror("open"); if(fd>=0)close(fd); if(tfd>=0)close(tfd); return; }
    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(t)) == sizeof(t)) {
        if (t.id == tid) { found = 1; continue; }
        write(tfd, &t, sizeof(t));
    }
    close(fd); close(tfd);
    rename(tmpfile, file);

    if (found)
        printf("Treasure ID %d removed.\n", tid);
    else
        printf("Treasure ID %d not found.\n", tid);

    char op[512]; snprintf(op, sizeof(op), "Removed treasure ID %d", tid);
    log_operation(hunt_id, op);
}

// Remove an entire hunt directory and contents
void remove_hunt(const char *hunt_id) {
    char dir[256], file[512], logf[512], linkname[256];
    snprintf(dir, sizeof(dir), "%s%s", HUNT_DIR_PREFIX, hunt_id);
    snprintf(file, sizeof(file), "%s/%s", dir, TREASURE_FILE);
    snprintf(logf, sizeof(logf), "%s/%s", dir, LOG_FILE);
    snprintf(linkname, sizeof(linkname), "%s%s", LOG_SYMLINK_PREFIX, hunt_id);

    unlink(file);
    unlink(logf);
    rmdir(dir);
    unlink(linkname);
    printf("Hunt %s removed.\n", hunt_id);

    char op[512]; snprintf(op, sizeof(op), "Removed hunt");
    log_operation(hunt_id, op);
}

// Main: parse arguments
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n"
                "  %s add <hunt_id>\n"
                "  %s list <hunt_id>\n"
                "  %s view <hunt_id> <id>\n"
                "  %s remove_treasure <hunt_id> <id>\n"
                "  %s remove_hunt <hunt_id>\n",
                argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "add") == 0) {
        add_treasure(argv[2]);
    } else if (strcmp(argv[1], "list") == 0) {
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "view") == 0 && argc == 4) {
        view_treasure(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "remove_treasure") == 0 && argc == 4) {
        remove_treasure(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "remove_hunt") == 0) {
        remove_hunt(argv[2]);
    } else {
        fprintf(stderr, "Invalid command.\n");
        return 1;
    }
    return 0;
}