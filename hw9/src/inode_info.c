#include "ext2.h"

#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static void print_time(const char *name, uint32_t raw) {
    time_t t = (time_t)raw;
    struct tm tm_buf;
    char buf[64];

    if (raw == 0) {
        printf("%s: 0\n", name);
        return;
    }

    if (localtime_r(&t, &tm_buf) == NULL) {
        printf("%s: %u\n", name, (unsigned)raw);
        return;
    }

    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &tm_buf) == 0) {
        printf("%s: %u\n", name, (unsigned)raw);
        return;
    }

    printf("%s: %u (%s)\n", name, (unsigned)raw, buf);
}

static void print_fs_info(const struct ext2_fs *fs) {
    printf("filesystem:\n");
    printf("  block_size: %u\n", (unsigned)fs->block_size);
    printf("  blocks_count: %u\n", (unsigned)fs->blocks_count);
    printf("  first_data_block: %u\n", (unsigned)fs->first_data_block);
    printf("  inodes_count: %u\n", (unsigned)fs->inodes_count);
    printf("  blocks_per_group: %u\n", (unsigned)fs->blocks_per_group);
    printf("  inodes_per_group: %u\n", (unsigned)fs->inodes_per_group);
    printf("  inode_size: %u\n", (unsigned)fs->inode_size);
    printf("  groups_count: %u\n", (unsigned)fs->groups_count);
    printf("  gd_table_offset: %llu\n", (unsigned long long)fs->gd_table_offset);
    printf("  feature_compat: 0x%08x\n", (unsigned)le32toh(fs->sb.s_feature_compat));
    printf("  feature_incompat: 0x%08x\n", (unsigned)le32toh(fs->sb.s_feature_incompat));
    printf("  feature_ro_compat: 0x%08x\n", (unsigned)le32toh(fs->sb.s_feature_ro_compat));
    printf("\n");
}

static void print_block_map(const struct ext2_fs *fs, const struct inode *ino) {
    uint64_t size;
    uint64_t logical_blocks;

    size = inode_size(ino);

    if (size == 0) {
        printf("\nblock map: empty file\n");
        return;
    }

    logical_blocks = (size + fs->block_size - 1) / fs->block_size;

    printf("\nblock map:\n");

    for (uint64_t i = 0; i < logical_blocks; i++) {
        uint32_t physical;

        if (get_data_block(fs, ino, i, &physical) < 0) {
            printf("  logical %llu: error\n", (unsigned long long)i);
            return;
        }

        if (physical == 0)
            printf("  logical %llu -> HOLE\n", (unsigned long long)i);
        else
            printf("  logical %llu -> physical %u\n",(unsigned long long)i, (unsigned)physical);
    }
}

static void print_inode_info(const struct ext2_fs *fs, uint32_t inode_no, const struct inode *ino) {
    uint16_t mode = le16toh(ino->i_mode);
    uint16_t perms = mode & 07777;

    printf("inode: %u\n", (unsigned)inode_no);
    printf("type: %s\n", inode_type_name(ino));
    printf("mode: 0%o\n", (unsigned)perms);

    printf("uid: %hu\n", (unsigned short)le16toh(ino->i_uid));
    printf("gid: %hu\n", (unsigned short)le16toh(ino->i_gid));

    printf("size: %llu\n", (unsigned long long)inode_size(ino));
    printf("links_count: %hu\n", (unsigned short)le16toh(ino->i_links_count));
    printf("blocks_512: %u\n", (unsigned)le32toh(ino->i_blocks));
    printf("flags: 0x%08x\n", (unsigned)le32toh(ino->i_flags));

    print_time("atime", le32toh(ino->i_atime));
    print_time("ctime", le32toh(ino->i_ctime));
    print_time("mtime", le32toh(ino->i_mtime));
    print_time("dtime", le32toh(ino->i_dtime));

    printf("\nblock pointers:\n");

    for (int i = 0; i < NDIR_BLOCKS; i++)
        printf("  direct[%d]: %u\n", i, (unsigned)le32toh(ino->i_block[i]));

    printf("  single_indirect: %u\n", (unsigned)le32toh(ino->i_block[IND_BLOCK]));
    printf("  double_indirect: %u\n", (unsigned)le32toh(ino->i_block[DIND_BLOCK]));
    printf("  triple_indirect: %u\n", (unsigned)le32toh(ino->i_block[TIND_BLOCK]));

    print_block_map(fs, ino);
}

int main(int argc, char **argv) {
    struct ext2_fs fs;
    struct inode ino;
    uint32_t inode_no;

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

    print_fs_info(&fs);
    print_inode_info(&fs, inode_no, &ino);

    ext2_close(&fs);
    return 0;
}