#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Treasure {
    int id;
    char username[50];
    float lat, lon;
    char clue[256];
    int value;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <treasure_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    struct Treasure t;
    struct {
        char username[50];
        int score;
    } scores[1000];

    int count = 0;
    while (fread(&t, sizeof(t), 1, f) == 1) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(scores[i].username, t.username) == 0) {
                scores[i].score += t.value;
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(scores[count].username, t.username);
            scores[count].score = t.value;
            count++;
        }
    }

    fclose(f);

    for (int i = 0; i < count; i++) {
        printf("%s: %d\n", scores[i].username, scores[i].score);
    }

    return 0;
}
