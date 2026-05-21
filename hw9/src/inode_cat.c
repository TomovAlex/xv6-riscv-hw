#include "ext2.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int parse_inode_number(const char *s, uint32_t *out) {
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || value == 0 || value > UINT32_MAX)
        return -1;

    *out = (uint32_t)value;
    return 0;
}

static int write_full(int fd, const void *buf, size_t size) {
    const uint8_t *p = buf;
    size_t done = 0;

    while (done < size) {
        ssize_t w = write(fd, p + done, size - done);

        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        } else if (w == 0) {
            errno = EIO;
            return -1;
        }

        done += (size_t)w;
    }

    return 0;
}

int main(int argc, char **argv) {
    struct ext2_fs fs;
    struct inode ino;
    uint32_t inode_no;
    uint8_t *block_buf = NULL;
    uint8_t *zero_buf = NULL;
    uint64_t size;
    uint64_t logical_blocks;
    uint64_t written = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <ext2 image/device> <inode>\n", argv[0]);
        return 1;
    }

    if (parse_inode_number(argv[2], &inode_no) < 0) {
        fprintf(stderr, "invalid inode number: %s\n", argv[2]);
        return 1;
    }

    if (ext2_open(&fs, argv[1]) < 0)
        return 1;

    if (read_inode(&fs, inode_no, &ino) < 0) {
        ext2_close(&fs);
        return 1;
    }

    size = inode_size(&ino);
    logical_blocks = (size + fs.block_size - 1) / fs.block_size;

    block_buf = malloc(fs.block_size);
    zero_buf = calloc(1, fs.block_size);

    if (block_buf == NULL || zero_buf == NULL) {
        perror("malloc");
        free(block_buf);
        free(zero_buf);
        ext2_close(&fs);
        return 1;
    }

    for (uint64_t logical = 0; logical < logical_blocks; logical++) {
        uint32_t physical;
        size_t to_write;

        if (get_data_block(&fs, &ino, logical, &physical) < 0) {
            free(block_buf);
            free(zero_buf);
            ext2_close(&fs);
            return 1;
        }

        if (size - written < fs.block_size)
            to_write = (size_t)(size - written);
        else
            to_write = fs.block_size;

        if (physical == 0) {
            if (write_full(STDOUT_FILENO, zero_buf, to_write) < 0) {
                perror("write");
                free(block_buf);
                free(zero_buf);
                ext2_close(&fs);
                return 1;
            }
        } else {
            if (read_block(&fs, physical, block_buf) < 0) {
                free(block_buf);
                free(zero_buf);
                ext2_close(&fs);
                return 1;
            }

            if (write_full(STDOUT_FILENO, block_buf, to_write) < 0) {
                perror("write");
                free(block_buf);
                free(zero_buf);
                ext2_close(&fs);
                return 1;
            }
        }

        written += to_write;
    }

    free(block_buf);
    free(zero_buf);
    ext2_close(&fs);

    return 0;
}