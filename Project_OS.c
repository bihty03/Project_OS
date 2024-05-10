#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

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

int compareSnapshots(const char *old_snapshot_path, const char *new_snapshot_path) {
    FILE *old_snapshot_file = fopen(old_snapshot_path, "r");
    if (old_snapshot_file == NULL) {
        perror("Error opening old snapshot file");
        return -1;
    }

    FILE *new_snapshot_file = fopen(new_snapshot_path, "r");
    if (new_snapshot_file == NULL) {
        perror("Error opening new snapshot file");
        fclose(old_snapshot_file);
        return -1;
    }

    char old_line[MAX_METADATA_LENGTH];
    char new_line[MAX_METADATA_LENGTH];

    // Read lines from both snapshot files and compare
    while (fgets(old_line, sizeof(old_line), old_snapshot_file) != NULL &&
           fgets(new_line, sizeof(new_line), new_snapshot_file) != NULL) {
        // Ignore the first line which contains the header
        if (strcmp(old_line, new_line) != 0) {
            // Lines are different
            fclose(old_snapshot_file);
            fclose(new_snapshot_file);
            return 1; // Snapshots are different
        }
    }

    // Check if both files have ended
    if (feof(old_snapshot_file) && feof(new_snapshot_file)) {
        fclose(old_snapshot_file);
        fclose(new_snapshot_file);
        return 0; // Snapshots are identical
    } else {
        // Snapshots have different number of lines
        fclose(old_snapshot_file);
        fclose(new_snapshot_file);
        return 1;
    }
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

void analyzeFile(const char *file_path, const char *output_dir) {
    // Check if the file has all permissions missing
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Unable to get file status");
        return;
    }

    if ((file_stat.st_mode & S_IRWXU) == 0 && (file_stat.st_mode & S_IRWXG) == 0 && (file_stat.st_mode & S_IRWXO) == 0) {
        // Create a dedicated process to perform a syntactic analysis
        pid_t pid = fork();

        if (pid == -1) {
            perror("Error forking process");
            return;
        } else if (pid == 0) {
            // Child process
            char script_path[MAX_PATH_LENGTH];
            snprintf(script_path, sizeof(script_path), "%s/verify_for_malicious.sh", output_dir);
            execl(script_path, script_path, file_path, NULL);
            perror("Error executing script");
            exit(1);
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                // Move the file to the isolated directory
                char isolated_file_path[MAX_PATH_LENGTH];
                snprintf(isolated_file_path, sizeof(isolated_file_path), "%s/%s", output_dir, basename(file_path));
                if (rename(file_path, isolated_file_path) == -1) {
                    perror("Error moving file to isolated directory");
                } else {
                    printf("File %s moved to isolated directory.\n", file_path);
                }
            } else {
                printf("File %s is considered safe.\n", file_path);
            }
        }
    } else {
        printf("File %s has sufficient permissions and is not analyzed.\n", file_path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > MAX_DIRS + 4) {
        fprintf(stderr, "Usage: %s -o output_dir [-s safe_dir] dir1 [dir2 ... dirN]\n", argv[0]);
        return 1;
    }

    char *output_dir = NULL;
    char *safe_dir = NULL;
    int start_index = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_dir = argv[i + 1];
                i++; // Skip the next argument
            } else {
                fprintf(stderr, "Error: Missing argument for output directory\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                safe_dir = argv[i + 1];
                i++; // Skip the next argument
            } else {
                fprintf(stderr, "Error: Missing argument for safe directory\n");
                return 1;
            }
        } else {
            start_index = i;
            break;
        }
    }

    if (output_dir == NULL) {
        fprintf(stderr, "Error: Missing output directory\n");
        return 1;
    }

    for (int i = start_index; i < argc; i++) {
        const char *dir_path = argv[i];
        pid_t pid = fork();

        if (pid == -1) {
            perror("Error forking process");
            exit(1);
        } else if (pid == 0) {
            // Child process
            handleSnapshot(dir_path);
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            printf("Child Process %d terminated with PID %d and exit code %d.\n", i - start_index + 1, pid, WEXITSTATUS(status));
        }
    }

    // Analyze files in the output directory
    if (safe_dir != NULL) {
        for (int i = start_index; i < argc; i++) {
            const char *dir_path = argv[i];
            DIR *dir = opendir(dir_path);
            if (dir == NULL) {
                perror("Unable to open directory");
                continue;
            }

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    char full_path[MAX_PATH_LENGTH];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
                    analyzeFile(full_path, safe_dir);
                }
            }

            closedir(dir);
        }
    }

    return 0;
}
