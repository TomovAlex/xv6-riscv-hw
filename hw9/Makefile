CC := gcc

CFLAGS := -Wall -Wextra -g -std=c11 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64
CPPFLAGS := -Isrc

OBJ_DIR := obj

all: inode_info inode_cat dir_parse

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

inode_info: $(OBJ_DIR)/inode_info.o $(OBJ_DIR)/ext2.o
	$(CC) $(CFLAGS) $^ -o $@

inode_cat: $(OBJ_DIR)/inode_cat.o $(OBJ_DIR)/ext2.o
	$(CC) $(CFLAGS) $^ -o $@

dir_parse: $(OBJ_DIR)/dir_parse.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/ext2.o: src/ext2.c src/ext2.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/ext2.c -o $@

$(OBJ_DIR)/inode_info.o: src/inode_info.c src/ext2.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/inode_info.c -o $@

$(OBJ_DIR)/inode_cat.o: src/inode_cat.c src/ext2.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/inode_cat.c -o $@

$(OBJ_DIR)/dir_parse.o: src/dir_parse.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/dir_parse.c -o $@

test: all
	bash tests/test_ext2.sh

valgrind: all
	@command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found. Install it: sudo apt install valgrind"; exit 1; }
	bash tests/test_ext2.sh
	. "test-out/inodes.txt"; \
	valgrind --leak-check=full --error-exitcode=1 ./inode_info "test-out/ext2.img" "$$BIG_INO"; \
	valgrind --leak-check=full --error-exitcode=1 ./inode_cat "test-out/ext2.img" "$$BIG_INO" >/dev/null; \
	./inode_cat "test-out/ext2.img" "$$ROOT_INO" | valgrind --leak-check=full --error-exitcode=1 ./dir_parse >/dev/null

clean:
	rm -rf $(OBJ_DIR) test-out inode_info inode_cat dir_parse