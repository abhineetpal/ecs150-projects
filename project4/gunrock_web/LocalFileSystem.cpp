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
    disk->readBlock(super->inode_region_addr + i, inodes + (UFS_BLOCK_SIZE * i));
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; i++) {
    disk->writeBlock(super->inode_region_addr + i, inodes + (UFS_BLOCK_SIZE * i));
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  return 0;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

