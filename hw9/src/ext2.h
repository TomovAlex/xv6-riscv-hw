#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stddef.h>

#define SUPER_OFFSET 1024
#define SUPER_MAGIC 0xEF53

#define NDIR_BLOCKS 12
#define IND_BLOCK 12
#define DIND_BLOCK 13
#define TIND_BLOCK 14
#define N_BLOCKS 15

#define EXT2_S_IFMT 0xF000
#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000

struct super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint8_t unused_8_19[12];
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint8_t unused_28_31[4];
    uint32_t s_blocks_per_group;
    uint8_t unused_36_39[4];
    uint32_t s_inodes_per_group;
    uint8_t unused_44_55[12];
    uint16_t s_magic;
    uint8_t unused_58_75[18];
    uint32_t s_rev_level;
    uint8_t unused_80_87[8];
    uint16_t s_inode_size;
    uint8_t unused_90_91[2];
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
} __attribute__((packed));

struct group_descriptor {
    uint8_t unused_0_7[8];
    uint32_t bg_inode_table;
    uint8_t unused_12_31[20];
} __attribute__((packed));

struct inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint8_t unused_36_39[4];
    uint32_t i_block[N_BLOCKS];
    uint8_t unused_100_103[4];
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
} __attribute__((packed));

struct ext2_fs {
    int fd;
    struct super_block sb;

    uint32_t block_size;
    uint32_t blocks_count;
    uint32_t first_data_block;
    uint32_t inodes_count;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t groups_count;

    uint64_t gd_table_offset;
};

int ext2_open(struct ext2_fs *fs, const char *path);
void ext2_close(struct ext2_fs *fs);

int read_inode(const struct ext2_fs *fs, uint32_t inode_no, struct inode *out);
int read_block(const struct ext2_fs *fs, uint32_t block_no, void *buf);
int get_data_block(const struct ext2_fs *fs, const struct inode *ino, uint64_t logical_block, uint32_t *physical_block);

uint64_t inode_size(const struct inode *ino);
const char *inode_type_name(const struct inode *ino);


#endif