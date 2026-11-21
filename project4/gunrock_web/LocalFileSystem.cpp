#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
  int superBlock[UFS_BLOCK_SIZE / sizeof(int)];
  disk->readBlock(0, superBlock);

  super->inode_bitmap_addr = superBlock[0];
  super->inode_bitmap_len = superBlock[1];
  super->data_bitmap_addr = superBlock[2];
  super->data_bitmap_len = superBlock[3];
  super->inode_region_addr = superBlock[4];
  super->inode_region_len = superBlock[5];
  super->data_region_addr = superBlock[6];
  super->data_region_len = superBlock[7];
  super->num_inodes = superBlock[8];
  super->num_data = superBlock[9];
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  for (int i = 0; i < super->inode_bitmap_len; i++) {
    disk->readBlock(super->inode_bitmap_addr + i, inodeBitmap + (UFS_BLOCK_SIZE * i));
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  for (int i = 0; i < super->inode_bitmap_len; i++) {
    disk->writeBlock(super->inode_bitmap_addr + i, inodeBitmap + (UFS_BLOCK_SIZE * i));
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; i++) {
    disk->readBlock(super->data_bitmap_addr + i, dataBitmap + (UFS_BLOCK_SIZE * i));
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; i++) {
    disk->writeBlock(super->data_bitmap_addr + i, dataBitmap + (UFS_BLOCK_SIZE * i));
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; i++) {
    disk->readBlock(super->inode_region_addr + i, inodes + ((UFS_BLOCK_SIZE / sizeof(inode_t)) * i));
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; i++) {
    disk->writeBlock(super->inode_region_addr + i, inodes + ((UFS_BLOCK_SIZE / sizeof(inode_t)) * i));
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  inode_t parentInode;
  if (stat(parentInodeNumber, &parentInode) != 0) return EINVALIDINODE;
  if (parentInode.type != UFS_DIRECTORY) return EINVALIDINODE;

  int num_entries = parentInode.size / sizeof(dir_ent_t);
  int num_entries_searched = 0;
  for (int dataBlockNum : parentInode.direct) {
    dir_ent_t dir_entries[UFS_BLOCK_SIZE / sizeof(dir_ent_t)];
    disk->readBlock(dataBlockNum, dir_entries);

    for (dir_ent_t dir_entry : dir_entries) {
      if (num_entries_searched == num_entries) return ENOTFOUND; // prevent us from interpreting garbage as a dir entry
      if (dir_entry.name == name) return dir_entry.inum;
      num_entries_searched++;
    }
  }
  return ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  if (inodeNumber < 1) return EINVALIDINODE;

  super_t super;
  readSuperBlock(&super);
  if (inodeNumber > super.num_inodes) return EINVALIDINODE;

  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  *inode = inodes[inodeNumber - 1]; // inodes start from 1 but array is 0-indexed
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  if (size <= 0) return EINVALIDSIZE;

  inode_t inode;
  if (stat(inodeNumber, &inode) != 0) return EINVALIDINODE;

  int bytesRead = 0;
  int directIdx = 0;
  while (size > 0) {
    disk->readBlock(inode.direct[directIdx], (char*)(buffer) + (UFS_BLOCK_SIZE * directIdx));
    bytesRead += size > UFS_BLOCK_SIZE ? UFS_BLOCK_SIZE : size;
    if (bytesRead >= inode.size) return inode.size;

    size -= UFS_BLOCK_SIZE;
    directIdx++;
  }
  return bytesRead;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  if (name.size() > 28) return EINVALIDNAME;

  inode_t parentInode;
  if (stat(parentInodeNumber, &parentInode) != 0) return EINVALIDINODE;
  if (parentInode.type != UFS_DIRECTORY) return EINVALIDINODE;

  int sameNameINode = lookup(parentInodeNumber, name);
  if (sameNameINode != ENOTFOUND) {
    inode_t inode;
    stat(sameNameINode, &inode);

    if (inode.type == type) return sameNameINode;
    else return EINVALIDTYPE;
  }

  super_t super;
  readSuperBlock(&super);

  unsigned char inodeBitmap[UFS_BLOCK_SIZE * super.inode_bitmap_len];
  readInodeBitmap(&super, inodeBitmap);
  int inode;
  for (inode = 3; inode < super.num_inodes; inode++) { // inodes 1 and 2 are reserved
    int idx = (inode - 1) / 8;
    unsigned char bitmapByte = inodeBitmap[idx];
  }
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

