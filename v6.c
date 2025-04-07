#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#define MAX_CLUE_LENGTH 256
#define TREASURE_FILE "hunt_treasures.db"
#define LOG_FILE "hunt_activity.log"

typedef struct {
    int id;
    char username[50];
    float latitude;
    float longitude;
    char clue[MAX_CLUE_LENGTH];
    int value;
} Treasure;

void add_treasure(const char *hunt_id, const char *treasure_id);
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, int treasure_id);
void remove_treasure(const char *hunt_id, int treasure_id);
void remove_hunt(const char *hunt_id);
void log_operation(const char *hunt_id, const char *operation, const char *details);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <operation> <hunt_id> [additional arguments]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "help") == 0) {
        printf("Available commands:\n");
        printf("  add <hunt_id> [treasure_id]       - Create a hunt or add a treasure to a hunt\n");
        printf("  list <hunt_id>                    - List all treasures in a hunt\n");
        printf("  view <hunt_id> <treasure_id>      - View details of a specific treasure\n");
        printf("  remove_treasure <hunt_id> <treasure_id> - Remove a specific treasure from a hunt\n");
        printf("  remove_hunt <hunt_id>             - Remove an entire hunt\n");
        return EXIT_SUCCESS;
    }

    const char *operation = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(operation, "add") == 0) {
        if (argc == 3) {
            add_treasure(hunt_id, NULL);
        } else if (argc == 4) {
            const char *treasure_id = argv[3];
            add_treasure(hunt_id, treasure_id);
        } else {
            fprintf(stderr, "Usage: %s add <hunt_id> [treasure_id]\n", argv[0]);
            return EXIT_FAILURE;
        }
    } else if (strcmp(operation, "list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(operation, "view") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s view <hunt_id> <treasure_id>\n", argv[0]);
            return EXIT_FAILURE;
        }
        int treasure_id = atoi(argv[3]);
        view_treasure(hunt_id, treasure_id);
    } else if (strcmp(operation, "remove_treasure") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s remove_treasure <hunt_id> <treasure_id>\n", argv[0]);
            return EXIT_FAILURE;
        }
        int treasure_id = atoi(argv[3]);
        remove_treasure(hunt_id, treasure_id);
    } else if (strcmp(operation, "remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        fprintf(stderr, "Unknown operation: %s\n", operation);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void add_treasure(const char *hunt_id, const char *treasure_id) {
    if (!hunt_id || !treasure_id) {
        fprintf(stderr, "Invalid hunt_id or treasure_id\n");
        return;
    }

    // Prepare paths
    char log_file_path[256];
    snprintf(log_file_path, sizeof(log_file_path), "logs/logs_%s.log", hunt_id);

    char treasure_file_path[256];
    snprintf(treasure_file_path, sizeof(treasure_file_path), "logs/%s.log", treasure_id);

    // Log the operation
    log_operation(hunt_id, "add_treasure", treasure_id);

    // Append to hunt log file
    FILE *hunt_log = fopen(log_file_path, "a");
    if (!hunt_log) {
        perror("Error opening hunt log file");
        return;
    }

    fprintf(hunt_log, "Treasure found: %s\n", treasure_id);
    fclose(hunt_log);

    // Create symlink (overwrite if exists)
    char symlink_path[256];
    snprintf(symlink_path, sizeof(symlink_path), "logs/logs_%s", hunt_id);
    remove(symlink_path); // Safe on macOS

    if (symlink(log_file_path, symlink_path) == -1) {
        perror("Error creating symbolic link in logs folder");
        log_operation(hunt_id, "create_symlink", "Failed to create symlink");
    } else {
        printf("Symlink created: %s -> %s\n", symlink_path, log_file_path);
        log_operation(hunt_id, "create_symlink", "Symlink created successfully");
    }

    // Append to treasure log file
    FILE *treasure_log = fopen(treasure_file_path, "a");
    if (!treasure_log) {
        perror("Error opening treasure log file");
        return;
    }

    fprintf(treasure_log, "Part of hunt: %s\n", hunt_id);
    fclose(treasure_log);
}

void list_treasures(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "hunt/%s/%s", hunt_id, TREASURE_FILE);

    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Error retrieving file information");
        return;
    }

    printf("Hunt: %s\n", hunt_id);
    printf("File Size: %ld bytes\n", file_stat.st_size);
    printf("Last Modified: %s", ctime(&file_stat.st_mtime));

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening treasure file");
        return;
    }

    Treasure treasure;
    printf("Treasures:\n");
    while (read(fd, &treasure, sizeof(Treasure)) > 0) {
        printf("ID: %d, Username: %s, Latitude: %.2f, Longitude: %.2f, Clue: %s, Value: %d\n",
               treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.clue, treasure.value);
    }

    close(fd);
    char log_details[512];
    snprintf(log_details, sizeof(log_details), "Viewed treasure ID %d, Username: %s, Latitude: %.2f, Longitude: %.2f, Clue: %s, Value: %d",
             treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.clue, treasure.value);
    log_operation(hunt_id, "view_treasure", log_details);
}

void view_treasure(const char *hunt_id, int treasure_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "hunt/%s/%s", hunt_id, TREASURE_FILE);
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening treasure file");
        return;
    }

    Treasure treasure;
    int found = 0;
    while (read(fd, &treasure, sizeof(Treasure)) > 0) {
        if (treasure.id == treasure_id) {
            printf("ID: %d, Username: %s, Latitude: %.2f, Longitude: %.2f, Clue: %s, Value: %d\n",
                   treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.clue, treasure.value);
            found = 1;
        }
    }
    if (!found) {
        printf("Treasure with ID %d not found.\n", treasure_id);
    }
    if (found) {
        char log_details[512];
        snprintf(log_details, sizeof(log_details), "Viewed treasure ID %d, Username: %s, Latitude: %.2f, Longitude: %.2f, Clue: %s, Value: %d",
                 treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.clue, treasure.value);
        log_operation(hunt_id, "view_treasure", log_details);
    }
    close(fd);
}

void remove_treasure(const char *hunt_id, int treasure_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "hunt/%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening treasure file");
        return;
    }

    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "hunt/%s/temp.dat", hunt_id);

    int temp_fd = open(temp_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd == -1) {
        perror("Error creating temporary file");
        close(fd);
        return;
    }

    Treasure treasure;
    int found = 0;
    while (read(fd, &treasure, sizeof(Treasure)) > 0) {
        if (treasure.id == treasure_id) {
            found = 1;
        } else {
            write(temp_fd, &treasure, sizeof(Treasure));
        }
    }

    close(fd);
    close(temp_fd);

    if (found) {
        if (rename(temp_file_path, file_path) == -1) {
            perror("Error renaming temporary file");
        } else {
            printf("Treasure entry deleted.\n");
            log_operation(hunt_id, "remove_treasure", "Treasure removed");
        }
    } else {
        printf("Treasure with ID %d not found.\n", treasure_id);
        unlink(temp_file_path);
    }
}

void remove_hunt(const char *hunt_id) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "hunt/%s", hunt_id);

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, TREASURE_FILE);

    if (unlink(file_path) == -1) {
        perror("Error deleting treasure file");
    }

    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", dir_path, LOG_FILE);

    if (unlink(log_path) == -1) {
        perror("Error deleting log file");
    }

    if (rmdir(dir_path) == -1) {
        perror("Error deleting hunt directory");
        return;
    }

    char symlink_path[256];
    snprintf(symlink_path, sizeof(symlink_path), "logs/logs_%s", hunt_id);
    remove(symlink_path);

    printf("Hunt removed successfully.\n");
    log_operation(hunt_id, "remove_hunt", "Hunt removed");
}

void log_operation(const char *hunt_id, const char *operation, const char *details) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "hunt/%s/hunt_activity.log", hunt_id);

    FILE *log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';

    fprintf(log_file, "[%s] %s: %s\n", timestamp, operation, details);
    fclose(log_file);
}
