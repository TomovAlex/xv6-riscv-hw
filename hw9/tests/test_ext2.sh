#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/test-out"
IMG="$OUT_DIR/ext2.img"
TMP="$OUT_DIR/temp"

INODE_INFO="$ROOT_DIR/inode_info"
INODE_CAT="$ROOT_DIR/inode_cat"
DIR_PARSE="$ROOT_DIR/dir_parse"

LOOP_DEV=""

cleanup() {
    if mountpoint -q "$TMP"; then
        sudo umount "$TMP"
    fi

    if [ -n "$LOOP_DEV" ]; then
        sudo losetup -d "$LOOP_DEV" || true
    fi
}

trap cleanup EXIT

mkdir -p "$OUT_DIR" "$TMP"

echo "== create image =="
truncate --size 256M "$IMG"
mkfs.ext2 -F -b 2048 "$IMG"

echo "== mount =="
sudo mount -t ext2 -o loop "$IMG" "$TMP"
sudo chown "$USER:$USER" "$TMP"

echo "== create files =="
echo "hello ext2" > "$TMP/small.txt"
dd if=/dev/urandom of="$TMP/big.bin" bs=2048 count=100 status=none

mkdir "$TMP/dir1" "$TMP/dir2" "$TMP/dir3"
echo "nested file" > "$TMP/dir1/nested.txt"

truncate -s 5G "$TMP/sparse_5g.bin"
printf 'BEGIN' > "$TMP/sparse_5g.bin"
printf 'END' | dd of="$TMP/sparse_5g.bin" bs=1 seek=$((5 * 1024 * 1024 * 1024 - 3)) conv=notrunc status=none

echo "== save inode numbers =="
ROOT_INO=2
SMALL_INO="$(stat -c '%i' "$TMP/small.txt")"
BIG_INO="$(stat -c '%i' "$TMP/big.bin")"
SPARSE_INO="$(stat -c '%i' "$TMP/sparse_5g.bin")"
DIR1_INO="$(stat -c '%i' "$TMP/dir1")"
DIR2_INO="$(stat -c '%i' "$TMP/dir2")"
DIR3_INO="$(stat -c '%i' "$TMP/dir3")"

cat > "$OUT_DIR/inodes.txt" <<EOF
ROOT_INO=$ROOT_INO
SMALL_INO=$SMALL_INO
BIG_INO=$BIG_INO
SPARSE_INO=$SPARSE_INO
DIR1_INO=$DIR1_INO
DIR2_INO=$DIR2_INO
DIR3_INO=$DIR3_INO
EOF

sha512sum "$TMP/small.txt" "$TMP/big.bin" > "$OUT_DIR/reference_sha.txt"
sha512sum "$TMP/sparse_5g.bin" >> "$OUT_DIR/reference_sha.txt"

sudo umount "$TMP"

echo "== inode_info =="
"$INODE_INFO" "$IMG" "$ROOT_INO" > "$OUT_DIR/inode_root.txt"
"$INODE_INFO" "$IMG" "$BIG_INO" > "$OUT_DIR/inode_big.txt"
"$INODE_INFO" "$IMG" "$SPARSE_INO" > "$OUT_DIR/inode_sparse.txt"

grep -q "type: directory" "$OUT_DIR/inode_root.txt"
grep -Eq "single_indirect: [1-9][0-9]*" "$OUT_DIR/inode_big.txt"
grep -q "HOLE" "$OUT_DIR/inode_sparse.txt"

echo "== inode_cat =="
"$INODE_CAT" "$IMG" "$SMALL_INO" | sha512sum > "$OUT_DIR/small_actual_sha.txt"
"$INODE_CAT" "$IMG" "$BIG_INO" | sha512sum > "$OUT_DIR/big_actual_sha.txt"
"$INODE_CAT" "$IMG" "$SPARSE_INO" | sha512sum > "$OUT_DIR/sparse_actual_sha.txt"

grep "$(cut -d ' ' -f1 "$OUT_DIR/small_actual_sha.txt")" "$OUT_DIR/reference_sha.txt"
grep "$(cut -d ' ' -f1 "$OUT_DIR/big_actual_sha.txt")" "$OUT_DIR/reference_sha.txt"
grep "$(cut -d ' ' -f1 "$OUT_DIR/sparse_actual_sha.txt")" "$OUT_DIR/reference_sha.txt"

echo "== dir_parse =="
"$INODE_CAT" "$IMG" "$ROOT_INO" | "$DIR_PARSE" | tee "$OUT_DIR/root_dir.txt"
"$INODE_CAT" "$IMG" "$DIR1_INO" | "$DIR_PARSE" | tee "$OUT_DIR/dir1_dir.txt"

grep -q "small.txt" "$OUT_DIR/root_dir.txt"
grep -q "big.bin" "$OUT_DIR/root_dir.txt"
grep -q "sparse_5g.bin" "$OUT_DIR/root_dir.txt"
grep -q "dir1" "$OUT_DIR/root_dir.txt"
grep -q "nested.txt" "$OUT_DIR/dir1_dir.txt"

grep -q "^$SMALL_INO .* small.txt" "$OUT_DIR/root_dir.txt"
grep -q "^$BIG_INO .* big.bin" "$OUT_DIR/root_dir.txt"
grep -q "^$SPARSE_INO .* sparse_5g.bin" "$OUT_DIR/root_dir.txt"
grep -q "^$DIR1_INO .* dir1" "$OUT_DIR/root_dir.txt"

echo "== loop device =="
LOOP_DEV="$(sudo losetup -f)"
sudo losetup "$LOOP_DEV" "$IMG"
sudo chmod a+r "$LOOP_DEV"

losetup -a > "$OUT_DIR/losetup-a.txt"
lsblk > "$OUT_DIR/lsblk.txt"
lsblk -o name,size,fstype > "$OUT_DIR/lsblk-fstype.txt"

"$INODE_INFO" "$LOOP_DEV" "$BIG_INO" > "$OUT_DIR/loop_inode_big.txt"
"$INODE_CAT" "$LOOP_DEV" "$BIG_INO" | sha512sum > "$OUT_DIR/loop_big_actual_sha.txt"

grep "$(cut -d ' ' -f1 "$OUT_DIR/loop_big_actual_sha.txt")" "$OUT_DIR/reference_sha.txt"

sudo losetup -d "$LOOP_DEV"
LOOP_DEV=""

echo "OK"
echo "test output saved in: $OUT_DIR"