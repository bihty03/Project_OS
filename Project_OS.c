#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_DIRS 10
#define MAX_PATH_LENGTH 1024
#define MAX_METADATA_LENGTH 512

struct EntryMetadata {
    char name[MAX_PATH_LENGTH];
    char type;
    time_t last_modified;
    int size;
};

void captureMetadata(const char *dir_path, int snapshot_fd) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char full_path[MAX_PATH_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

            struct stat file_stat;
            if (stat(full_path, &file_stat) == -1) {
                perror("Unable to get file status");
                continue;
            }

            struct EntryMetadata metadata;
            strcpy(metadata.name, entry->d_name);
            if (S_ISDIR(file_stat.st_mode))
                metadata.type = 'D';
            else if (S_ISREG(file_stat.st_mode))
                metadata.type = 'F';
            else
                continue;

            metadata.last_modified = file_stat.st_mtime;
            metadata.size = file_stat.st_size;

            dprintf(snapshot_fd, "%s\t%c\t%ld\t%lld\n", metadata.name, metadata.type,
                    (long)metadata.last_modified, (long long)metadata.size);

            if (metadata.type == 'D') {
                captureMetadata(full_path, snapshot_fd);
            }
        }
    }

    closedir(dir);
}

void createSnapshot(const char *dir_path) {
    char snapshot_path[MAX_PATH_LENGTH];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/Snapshot.txt", dir_path);
    FILE *snapshot_file = fopen(snapshot_path, "w");
    if (snapshot_file == NULL) {
        perror("Error creating snapshot file");
        return;
    }

    fprintf(snapshot_file, "The order is: Name, Type, Last Modified, Size\n");
    captureMetadata(dir_path, fileno(snapshot_file));

    fclose(snapshot_file);
    printf("Snapshot for Directory %s created successfully.\n", dir_path);
}

void handleSnapshot(const char *dir_path) {
    createSnapshot(dir_path);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > MAX_DIRS + 1) {
        fprintf(stderr, "Usage: %s <dir1> [<dir2> ... <dirN>]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        for (int j = i + 1; j < argc; j++) {
            if (strcmp(argv[i], argv[j]) == 0) {
                fprintf(stderr, "Error: Duplicate directory arguments\n");
                return 1;
            }
        }
    }

    for (int i = 1; i < argc; i++) {
        const char *dir_path = argv[i];
        pid_t pid = fork();

        if (pid == -1) {
            perror("Error forking process");
            exit(1);
        } else if (pid == 0) {
            handleSnapshot(dir_path);
        } else {
            int status;
            waitpid(pid, &status, 0);
            printf("Child Process %d terminated with PID %d and exit code %d.\n", i, pid, WEXITSTATUS(status));
        }
    }

    return 0;
}
