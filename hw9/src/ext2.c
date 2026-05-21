#include "ext2.h"

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int read_full_at(int fd, void *buf, size_t size, uint64_t offset) {
    uint8_t *p = buf;
    size_t done = 0;

    while (done < size) {
        ssize_t r = pread(fd, p + done, size - done, offset + done);

        if (r < 0) {
            if (errno == EINTR) 
                continue;
            return -1;
        } else if (r == 0) {
            errno = EIO;
            return -1;
        }

        done += (size_t)r;
    }

    return 0;
}

static int read_superblock(struct ext2_fs *fs) {
    uint16_t magic;
    uint32_t log_block_size;
    uint32_t rev_level;
    uint32_t usable_blocks;

    if (read_full_at(fs->fd, &fs->sb, sizeof(fs->sb), SUPER_OFFSET) < 0) {
        perror("read superblock");
        return -1;
    }

    magic = le16toh(fs->sb.s_magic);

    if (magic != SUPER_MAGIC) {
        fprintf(stderr, "bad ext2 magic: 0x%04x\n", magic);
        return -1;
    }

    log_block_size = le32toh(fs->sb.s_log_block_size);

    if (log_block_size > 6) {
        fprintf(stderr, "unsupported block size shift: %u\n", (unsigned)log_block_size);
        return -1;
    }

    fs->block_size = 1024U << log_block_size;
    fs->blocks_count = le32toh(fs->sb.s_blocks_count);
    fs->first_data_block = le32toh(fs->sb.s_first_data_block);
    fs->inodes_count = le32toh(fs->sb.s_inodes_count);
    fs->blocks_per_group = le32toh(fs->sb.s_blocks_per_group);
    fs->inodes_per_group = le32toh(fs->sb.s_inodes_per_group);

    rev_level = le32toh(fs->sb.s_rev_level);

    if (rev_level == 0)
        fs->inode_size = 128;
    else 
        fs->inode_size = le16toh(fs->sb.s_inode_size);
    

    if (fs->block_size == 0 ||
        fs->blocks_count == 0 ||
        fs->inodes_count == 0 ||
        fs->blocks_per_group == 0 ||
        fs->inodes_per_group == 0 ||
        fs->inode_size < sizeof(struct inode) ||
        fs->blocks_count <= fs->first_data_block) {
        fprintf(stderr, "invalid ext2 superblock values\n");
        return -1;
    }

    usable_blocks = fs->blocks_count - fs->first_data_block;
    fs->groups_count = (usable_blocks + fs->blocks_per_group - 1) / fs->blocks_per_group;

    if (fs->block_size == 1024)
        fs->gd_table_offset = 2048;
    else
        fs->gd_table_offset = fs->block_size;
    return 0;
}

int ext2_open(struct ext2_fs *fs, const char *path) {
    memset(fs, 0, sizeof(*fs));
    fs->fd = -1;

    fs->fd = open(path, O_RDONLY);

    if (fs->fd < 0) {
        perror(path);
        return -1;
    }

    if (read_superblock(fs) < 0) {
        ext2_close(fs);
        return -1;
    }

    return 0;
}

void ext2_close(struct ext2_fs *fs) {
    if (fs == NULL)
        return;
    if (fs->fd >= 0) {
        close(fs->fd);
        fs->fd = -1;
    }
}

int read_block(const struct ext2_fs *fs, uint32_t block_no, void *buf) {
    uint64_t offset = (uint64_t)block_no * fs->block_size;
    if (read_full_at(fs->fd, buf, fs->block_size, offset) < 0) {
        perror("read block");
        return -1;
    }
    return 0;
}

static int read_indirect_pointer(const struct ext2_fs *fs, uint32_t block_no, uint64_t index, uint32_t *value) {
    uint8_t *buf;
    uint32_t *pointers;
    uint32_t pointers_per_block;

    if (block_no == 0) {
        *value = 0;
        return 0;
    }

    pointers_per_block = fs->block_size / 4;

    if (index >= pointers_per_block) {
        fprintf(stderr, "indirect index out of range\n");
        return -1;
    }

    buf = malloc(fs->block_size);

    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    if (read_block(fs, block_no, buf) < 0) {
        free(buf);
        return -1;
    }

    pointers = (uint32_t *)buf;
    *value = le32toh(pointers[index]);

    free(buf);
    return 0;
}

int get_data_block(const struct ext2_fs *fs, const struct inode *ino, uint64_t logical_block, uint32_t *physical_block) {
    uint64_t n;
    uint64_t first;
    uint64_t second;
    uint64_t third;
    uint64_t rest;

    uint32_t indirect_block;
    uint32_t double_block;
    uint32_t triple_block;

    n = fs->block_size / 4;

    if (logical_block < NDIR_BLOCKS) {
        *physical_block = le32toh(ino->i_block[logical_block]);
        return 0;
    }

    logical_block -= NDIR_BLOCKS;

    if (logical_block < n) {
        indirect_block = le32toh(ino->i_block[IND_BLOCK]);
        return read_indirect_pointer(fs, indirect_block, logical_block, physical_block);
    }

    logical_block -= n;

    if (logical_block < n * n) {
        double_block = le32toh(ino->i_block[DIND_BLOCK]);

        first = logical_block / n;
        second = logical_block % n;

        if (read_indirect_pointer(fs, double_block, first, &indirect_block) < 0)
            return -1;
        return read_indirect_pointer(fs, indirect_block, second, physical_block);
    }

    logical_block -= n * n;

    triple_block = le32toh(ino->i_block[TIND_BLOCK]);

    first = logical_block / (n * n);
    rest = logical_block % (n * n);
    second = rest / n;
    third = rest % n;

    if (read_indirect_pointer(fs, triple_block, first, &double_block) < 0)
        return -1;

    if (read_indirect_pointer(fs, double_block, second, &indirect_block) < 0)
        return -1;

    return read_indirect_pointer(fs, indirect_block, third, physical_block);
}


static int read_group_descriptor(const struct ext2_fs *fs, uint32_t group, struct group_descriptor *gd) {
    uint64_t offset;

    if (group >= fs->groups_count) {
        fprintf(stderr, "group out of range: %u\n", (unsigned)group);
        return -1;
    }

    offset = fs->gd_table_offset + (uint64_t)group * sizeof(struct group_descriptor);

    if (read_full_at(fs->fd, gd, sizeof(*gd), offset) < 0) {
        perror("read group descriptor");
        return -1;
    }

    return 0;
}

int read_inode(const struct ext2_fs *fs, uint32_t inode_no, struct inode *out) {
    uint32_t group;
    uint32_t index;
    uint32_t inode_table_block;
    uint64_t inode_offset;
    struct group_descriptor gd;

    if (inode_no == 0 || inode_no > fs->inodes_count) {
        fprintf(stderr, "bad inode number: %u\n", (unsigned)inode_no);
        return -1;
    }

    group = (inode_no - 1) / fs->inodes_per_group;
    index = (inode_no - 1) % fs->inodes_per_group;

    if (read_group_descriptor(fs, group, &gd) < 0) {
        return -1;
    }

    inode_table_block = le32toh(gd.bg_inode_table);

    inode_offset =
        (uint64_t)inode_table_block * fs->block_size +
        (uint64_t)index * fs->inode_size;

    if (read_full_at(fs->fd, out, sizeof(*out), inode_offset) < 0) {
        perror("read inode");
        return -1;
    }

    return 0;
}

uint64_t inode_size(const struct inode *ino) {
    uint16_t mode = le16toh(ino->i_mode);
    uint16_t type = mode & EXT2_S_IFMT;
    uint64_t size = le32toh(ino->i_size);

    if (type == EXT2_S_IFREG)
        size |= ((uint64_t)le32toh(ino->i_dir_acl)) << 32;

    return size;
}

const char *inode_type_name(const struct inode *ino) {
    uint16_t mode = le16toh(ino->i_mode);

    switch (mode & EXT2_S_IFMT) {
    case EXT2_S_IFREG:
        return "regular file";
    case EXT2_S_IFDIR:
        return "directory";
    case EXT2_S_IFLNK:
        return "symbolic link";
    case EXT2_S_IFCHR:
        return "character device";
    case EXT2_S_IFBLK:
        return "block device";
    case EXT2_S_IFIFO:
        return "fifo";
    case EXT2_S_IFSOCK:
        return "socket";
    default:
        return "unknown";
    }
}