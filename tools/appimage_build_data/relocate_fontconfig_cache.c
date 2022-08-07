#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <openssl/md5.h>

void fail(const char* reason) {
    fprintf(stderr, "ERROR: Relocating fontconfig cache failed: %s\n", reason);
    exit(1);
}

void md5(const char* str, char out[32]) {
    unsigned char binOut[16];
    MD5((const unsigned char*)str, strlen(str), binOut);
    for(int i = 0; i < 16; ++i) {
        int high = (int)binOut[i] >> 4;
        int low = (int)binOut[i] & 15;
        out[2 * i] = high < 10 ? '0' + high : 'a' + (high - 10);
        out[2 * i + 1] = low < 10 ? '0' + low : 'a' + (low - 10);
    }
}

long findStr(const char* data, long dataLen, const char* needle) {
    for(long i = 0; i <= dataLen; ++i) {
        const char* pos = needle;
        long j = i;
        while(j < dataLen && *pos != '\0' && data[j] == *pos) {
            ++j;
            ++pos;
        }
        if(*pos == '\0') {
            return i;
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        fail("Invalid arguments");
    }

    const char* oldRoot = "/LVT3hhSkcNc5zRqVUNYsPhi04ynbyA6OsunvGAvCq8VDd2RFcLbbHjzU6IXzT47/19GtB0Wo248j5HohiIpAjFJGD6lVfPpPjmUZiyUzY3Xv90dz1n9qjrlXD2rR8EK/dEwsBnoucPaBCN2L3Lrmrf2FcIIeG4puJ28rizYQRX0mofs5CnYiqe8jFGuVJ76/bmC6XM33HnRR9S3QAtMB7iSrQlvT91CMhlTzdmrose41798QltC0TstZpRPIBOL/nhsRVVU6I9VcV0YRg3zz8gqfwe7ZJyaatzrAtjlXK30D07mNnQMD7a9DDcnPp7z/svKScD6FKcpn9fMm0k40BNr6dFqhRyXDU6dkCech3Pp9lqjeuQ4YzbvPqwzowmq/R5X4u2OMXpz6k08a6AcHv6z7TkzfnzsKcq0w303Z6Yz47zOUbZv7TCSuvKvT5LS/IDHXUis1UKrlOqPZkQy2gYVbjdfcfzXtZDm466vseM35dyatcsBcqIqvbpLbz9X/IZznU2HilLp36sEH9jLqdWkScpLOLekIPWSb7gMYP4uwATYhjeM02AXFgH23YkC/p5mAd5HE0Otgsh5gqcdDzzcG7A4umjgX17YqCiFlqTAHUprlCFQePrmE4iqfqmY/lQD5FJflFrWwFIDMRgjhU18yJvrPMvpdpypbt2XPF2sPb18YWUe5wWC6SUAngzO/9wNmfDXrqnlBAmuEuhPvz4d3bvw2BUjhQ56zRn5znvq887C5d8mMm3NrcwX16p9/dY9Kz64wsfnKpaYDN2Y2zvGp7TkHPUYaIsc12FpZc225OaPTyRaZThs9JHJLCln/pqYp5DmZPe68YCf94B8eL73nVXd1KCAgq34qplWtuAHeAQsIrbC7M7ZqGU3OYXH/npKMe4Uj3mORt69rKTOmddFUJBLw6JjYUAFFgnUDbb6OBNvjv2roucHFPACjdPS/PNO2C2YVB8pW5CMP7LgfKUbzXpehDzFjs4q93hS0yiSIzeT6sACnZoEyIGlKwbu";
    const char* newRoot = argv[1];
    const char* srcDirPath = argv[2];
    const char* destDirPath = argv[3];

    int oldRootLen = strlen(oldRoot);
    int newRootLen = strlen(newRoot);
    if(newRootLen > oldRootLen) {
        fail("Root path is too long");
    }

    DIR* dir = opendir(srcDirPath);
    if(dir == NULL) {
        fail("Opening source directory failed");
    }

    int srcDirPathLen = strlen(srcDirPath);
    int destDirPathLen = strlen(destDirPath);

    while(1) {
        errno = 0;
        struct dirent* ent = readdir(dir);
        if(ent == NULL) {
            if(errno) {
                fail("Listing source directory failed");
            }
            break;
        }

        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        int nameLen = strlen(ent->d_name);
        char* name = malloc(nameLen + 1);
        if(name == NULL) {
            fail("Allocating memory failed");
        }
        memcpy(name, ent->d_name, nameLen + 1);

        int isCacheFile = 1;

        if(nameLen <= 32) {
            isCacheFile = 0;
        }

        int isUUIDCacheFile = isCacheFile && name[8] == '-';

        if(isUUIDCacheFile) {
            if(nameLen <= 36) {
                isCacheFile = 0;
            }

            for(int i = 0; isCacheFile && i < 36; ++i) {
                char c = name[i];
                if(!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && c != '-') {
                    isCacheFile = 0;
                }
            }

            if(isCacheFile && name[36] != '-') {
                isCacheFile = 0;
            }
        } else {
            for(int i = 0; isCacheFile && i < 32; ++i) {
                char c = name[i];
                if(!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) {
                    isCacheFile = 0;
                }
            }

            if(isCacheFile && name[32] != '-') {
                isCacheFile = 0;
            }
        }

        if(isCacheFile && strstr(&name[32], ".cache") == NULL) {
            isCacheFile = 0;
        }

        int srcFilePathLen = srcDirPathLen + 1 + nameLen;
        int destFilePathLen = destDirPathLen + 1 + nameLen;
        char* srcFilePath = malloc(srcFilePathLen + 1);
        char* destFilePath = malloc(destFilePathLen + 1);
        if(srcFilePath == NULL || destFilePath == NULL) {
            fail("Allocating memory failed");
        }
        snprintf(srcFilePath, srcFilePathLen + 1, "%s/%s", srcDirPath, name);

        FILE* fp = fopen(srcFilePath, "rb");
        if(fp == NULL) {
            fail("Opening source file for reading failed");
        }

        if(fseek(fp, 0, SEEK_END)) {
            fail("Seeking to the end of a source file failed");
        }

        long fileSize = ftell(fp);
        if(fileSize < 0) {
            fail("Determining the size of a source file failed");
        }

        if(fseek(fp, 0, SEEK_SET)) {
            fail("Seeking to the beginning of a source file failed");
        }

        char* data = malloc(fileSize + 1);
        if(data == NULL) {
            fail("Allocating memory failed");
        }
        data[fileSize] = '\0';

        if(fileSize > 0 && (long)fread(data, 1, fileSize, fp) != fileSize) {
            fail("Reading the content of a source file failed");
        }

        if(fclose(fp)) {
            fail("Closing source file failed");
        }

        if(isCacheFile) {
            long fontDirPathPos = findStr(data, fileSize, oldRoot);
            if(fontDirPathPos < 0) {
                fail("Could not find old root path from cache file");
            }
            char* fontDirPath = &data[fontDirPathPos];

            char fontDirPathHash[32];
            md5(fontDirPath, fontDirPathHash);

            if(!isUUIDCacheFile) {
                for(int i = 0; i < 32; ++i) {
                    if(fontDirPathHash[i] != name[i]) {
                        fail("Unexpected name for cache file");
                    }
                }
            }

            long pos = 0;
            while(pos < fileSize) {
                long res = findStr(&data[pos], fileSize - pos, oldRoot);
                if(res < 0) {
                    break;
                }
                pos += res;

                char* path = &data[pos];
                int pathLen = strlen(path);

                int j = 0;
                for(int i = 0; i < newRootLen; ++i) {
                    path[j++] = newRoot[i];
                }
                for(int i = 0; i < pathLen - oldRootLen; ++i) {
                    path[j++] = path[oldRootLen + i];
                }
                while(j < pathLen) {
                    path[j++] = '\0';
                }

                ++pos;
            }

            if(!isUUIDCacheFile) {
                md5(fontDirPath, name);
            }

            struct stat fontDirStat;
            if(stat(fontDirPath, &fontDirStat)) {
                fail("Could not stat font directory");
            }

            int timePos;
            if(sizeof(struct timespec) == 16) {
                timePos = 0x30;
            } else if(sizeof(struct timespec) == 8) {
                timePos = 0x1c;
            } else {
                fail("Unsupported sizeof(struct timespec)");
            }

            if(fileSize < (long)(timePos + sizeof(struct timespec))) {
                fail("File is too short");
            }

            memcpy(&data[timePos], &fontDirStat.st_mtim, sizeof(struct timespec));
        }

        snprintf(destFilePath, destFilePathLen + 1, "%s/%s", destDirPath, name);

        fp = fopen(destFilePath, "wb");
        if(fp == NULL) {
            fail("Opening destination file for writing failed");
        }

        if(fileSize > 0 && (long)fwrite(data, 1, fileSize, fp) != fileSize) {
            fail("Writing data to a destination file failed");
        }

        if(fclose(fp)) {
            fail("Closing destination file failed");
        }

        free(name);
        free(srcFilePath);
        free(destFilePath);
        free(data);
    }

    if(closedir(dir)) {
        fail("Closing source directory failed");
    }

    return 0;
}
