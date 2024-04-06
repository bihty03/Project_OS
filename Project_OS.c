#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

void listFilesRecursively(const char *basePath, FILE *snapshotFile, int depth) {
    struct dirent *dp;
    struct stat statbuf;

    DIR *dir = opendir(basePath);
    if (!dir) {
        fprintf(stderr, "Cant open ", basePath);
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            for (int i = 0; i < depth; i++) {
                fprintf(snapshotFile, "  -");
            }
            fprintf(snapshotFile, "%s\n", dp->d_name);

            char path[1000];
            snprintf(path, sizeof(path), "%s/%s", basePath, dp->d_name);
            if (stat(path, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) {
                listFilesRecursively(path, snapshotFile, depth + 1);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error", argv[0]);
        return 1;
    }

    FILE *snapshotFile = fopen("directory_snapshot.txt", "w");
    if (!snapshotFile) {
        fprintf(stderr, "Error ");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        fprintf(snapshotFile, "Directory: %s\n", argv[i]);
        listFilesRecursively(argv[i], snapshotFile, 1);
        fprintf(snapshotFile, "\n");
    }

    fclose(snapshotFile);
    printf("Snapshot saved in 'directory_snapshot.txt'\n");

    return 0;
}