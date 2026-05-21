#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIRENT_INODE 0
#define DIRENT_REC_LEN 4
#define DIRENT_NAME_LEN 6
#define DIRENT_FILE_TYPE 7
#define DIRENT_NAME 8

static uint16_t rd_le16(const void *p) {
    const uint8_t *b = p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t rd_le32(const void *p) {
    const uint8_t *b = p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static const char *file_type_name(uint8_t type) {
    switch (type) {
    case 0:
        return "unknown";
    case 1:
        return "file";
    case 2:
        return "dir";
    case 3:
        return "chrdev";
    case 4:
        return "blkdev";
    case 5:
        return "fifo";
    case 6:
        return "sock";
    case 7:
        return "symlink";
    default:
        return "bad";
    }
}

static int read_all_stdin(uint8_t **out_buf, size_t *out_size) {
    size_t cap = 4096;
    size_t size = 0;
    uint8_t *buf = malloc(cap);
    int done = 0;

    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    while (!done) {
        ssize_t r;

        if (size == cap) {
            size_t new_cap = cap * 2;
            uint8_t *new_buf = realloc(buf, new_cap);

            if (new_buf == NULL) {
                perror("realloc");
                free(buf);
                return -1;
            }

            buf = new_buf;
            cap = new_cap;
        }

        r = read(STDIN_FILENO, buf + size, cap - size);

        if (r < 0) {
            if (errno == EINTR)
                continue;

            perror("read");
            free(buf);
            return -1;
        } else if (r == 0)
            done = 1;
        else
            size += (size_t)r;
    }

    *out_buf = buf;
    *out_size = size;
    return 0;
}

int main(void) {
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t pos = 0;

    if (read_all_stdin(&buf, &size) < 0)
        return 1;

    printf("%-10s %-8s %-10s %s\n", "inode", "rec_len", "type", "name");

    while (pos + DIRENT_NAME <= size) {
        uint32_t inode;
        uint16_t rec_len;
        uint8_t name_len;
        uint8_t file_type;
        const char *name;

        inode = rd_le32(buf + pos + DIRENT_INODE);
        rec_len = rd_le16(buf + pos + DIRENT_REC_LEN);
        name_len = buf[pos + DIRENT_NAME_LEN];
        file_type = buf[pos + DIRENT_FILE_TYPE];
        name = (const char *)(buf + pos + DIRENT_NAME);

        if (rec_len == 0) {
            fprintf(stderr, "bad directory entry: rec_len is zero at offset %zu\n", pos);
            free(buf);
            return 1;
        }

        if (pos + rec_len > size) {
            fprintf(stderr, "bad directory entry: rec_len out of input at offset %zu\n", pos);
            free(buf);
            return 1;
        }

        if (rec_len < DIRENT_NAME || name_len > rec_len - DIRENT_NAME) {
            fprintf(stderr, "bad directory entry: bad name_len at offset %zu\n", pos);
            free(buf);
            return 1;
        }

        if (inode != 0)
            printf("%-10u %-8u %-10s %.*s\n", (unsigned)inode, (unsigned)rec_len, file_type_name(file_type), (int)name_len, name);
        pos += rec_len;
    }

    free(buf);
    return 0;
}