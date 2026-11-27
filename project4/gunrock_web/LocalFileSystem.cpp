#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

// Returns number of blocks needed to store size bytes
int numBlocksNeeded(int size) {
  if (size == 0) return 1;

  int blocks = size / UFS_BLOCK_SIZE;
  if ((size % UFS_BLOCK_SIZE) != 0) blocks++;
  return blocks;
}

// Returns the lowest free inode in the bitmap and modifies the provided bitmap if found
// Returns -ENOTENOUGHSPACE if there are no free inodes
int findFreeInode(super_t *super, unsigned char *inodeBitmap) {
  int inodeNum;
  for (inodeNum = 3; inodeNum < super->num_inodes; inodeNum++) { // inodes 1 and 2 are reserved
    int idx = (inodeNum - 1) / 8;
    unsigned char bitmapByte = inodeBitmap[idx];
    if (bitmapByte == 255) inodeNum = ((idx + 1) * 8) + 1; // go to LSB of next byte if this one is full
    else {
      unsigned char shiftBy = (inodeNum - 1) % 8;
      if (!((bitmapByte >> shiftBy) & 0x01)) {
        bitmapByte |= 1 << shiftBy;
        inodeBitmap[idx] = bitmapByte;
        return inodeNum;
      }
    }
  }
  return -ENOTENOUGHSPACE;
}

// Returns the lowest free data block in the bitmap and modifies the provided bitmap if found
// Returns -ENOTENOUGHSPACE if there are no free data blocks
int findFreeDataBlock(super_t *super, unsigned char *dataBitmap) {
  int dataBlockNum;
  for (dataBlockNum = 0; dataBlockNum < super->num_data; dataBlockNum++) {
    int idx = dataBlockNum / 8;
    unsigned char bitmapByte = dataBitmap[idx];
    if (bitmapByte == 255) dataBlockNum = ((idx + 1) * 8) + 1; // go to LSB of next byte if this one is full
    else {
      unsigned char shiftBy = dataBlockNum % 8;
      if (!((bitmapByte >> shiftBy) & 0x01)) {
        bitmapByte |= 1 << shiftBy;
        dataBitmap[idx] = bitmapByte;
        return dataBlockNum;
      }
    }
  }
  return -ENOTENOUGHSPACE;
}

// Clears the bit associated with the inode in the bitmap
// Does NOT handle deallocating data blocks pointed to by the inode
void freeInode(int inodeNum, unsigned char *inodeBitmap) {
  if (inodeNum < 3) return; // inodes 1 and 2 are reserved and cannot be deallocated

  int idx = (inodeNum - 1) / 8;
  unsigned char shiftBy = (inodeNum - 1) % 8;
  inodeBitmap[idx] &= ~(1 << shiftBy);
}

// Clears the bit associated with the data block in the bitmap
void freeDataBlock(int dataBlockNum, unsigned char *dataBitmap) {
  int idx = dataBlockNum / 8;
  unsigned char shiftBy = dataBlockNum % 8;
  dataBitmap[idx] &= ~(1 << shiftBy);
}

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
  if (stat(parentInodeNumber, &parentInode) != 0) return -EINVALIDINODE;
  if (parentInode.type != UFS_DIRECTORY) return -EINVALIDINODE;

  int num_entries = parentInode.size / sizeof(dir_ent_t);
  int num_entries_searched = 0;
  for (int dataBlockNum : parentInode.direct) {
    dir_ent_t dir_entries[UFS_BLOCK_SIZE / sizeof(dir_ent_t)];
    disk->readBlock(dataBlockNum, dir_entries);

    for (dir_ent_t dir_entry : dir_entries) {
      if (num_entries_searched == num_entries) return -ENOTFOUND; // prevent us from interpreting garbage as a dir entry
      if (dir_entry.name == name) return dir_entry.inum;
      num_entries_searched++;
    }
  }
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  if (inodeNumber < 1) return -EINVALIDINODE;

  super_t super;
  readSuperBlock(&super);
  if (inodeNumber > super.num_inodes) return -EINVALIDINODE;

  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  *inode = inodes[inodeNumber - 1]; // inodes start from 1 but array is 0-indexed
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  if (size <= 0) return -EINVALIDSIZE;

  inode_t inode;
  if (stat(inodeNumber, &inode) != 0) return -EINVALIDINODE;

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
  if (name.size() > 28) return -EINVALIDNAME;

  inode_t parentInode;
  if (stat(parentInodeNumber, &parentInode) != 0) return -EINVALIDINODE;
  if (parentInode.type != UFS_DIRECTORY) return -EINVALIDINODE;

  int sameNameInodeNum = lookup(parentInodeNumber, name);
  if (sameNameInodeNum > 0) {
    inode_t inode;
    stat(sameNameInodeNum, &inode);

    return inode.type == type ? sameNameInodeNum : -EINVALIDTYPE;
  }

  super_t super;
  readSuperBlock(&super);

  unsigned char inodeBitmap[UFS_BLOCK_SIZE * super.inode_bitmap_len];
  readInodeBitmap(&super, inodeBitmap);
  int inodeNum = findFreeInode(&super, inodeBitmap);
  if (inodeNum < 0) return -ENOTENOUGHSPACE;

  unsigned char dataBitmap[UFS_BLOCK_SIZE * super.data_bitmap_len];
  readDataBitmap(&super, dataBitmap);
  int dataBlockNum = findFreeDataBlock(&super, dataBitmap);
  if (dataBlockNum < 0) return -ENOTENOUGHSPACE;

  int num_entries = parentInode.size / sizeof(dir_ent_t);
  int parentNumBlocksAllocated = numBlocksNeeded(parentInode.size);
  bool existing_free_entry_found = false;
  dir_ent_t dir_entries[parentNumBlocksAllocated * UFS_BLOCK_SIZE / sizeof(dir_ent_t)];
  read(parentInodeNumber, dir_entries, parentInode.size);
  for (int i = 2; i < num_entries; i++) { // first two entries are . and ..
    dir_ent_t dir_entry = dir_entries[i];
    if (dir_entry.inum == 0) {
      existing_free_entry_found = true;
      dir_entry.inum = inodeNum;
      memset(dir_entry.name, 0, 28); // remove old name before writing new name
      memcpy(dir_entry.name, name.c_str(), name.size());
      dir_entries[i] = dir_entry;

      for (int j = 0; j < parentNumBlocksAllocated; j++) {
        disk->writeBlock(parentInode.direct[j], dir_entries + ((UFS_BLOCK_SIZE / sizeof(dir_ent_t)) * j));
      }

      writeInodeBitmap(&super, inodeBitmap);
      writeDataBitmap(&super, dataBitmap);
      break;
    }
  }
  if (!existing_free_entry_found) {
    if (parentInode.size < (parentNumBlocksAllocated * UFS_BLOCK_SIZE)) {
      dir_ent_t new_dir_entry = dir_entries[num_entries];
      new_dir_entry.inum = inodeNum;
      memset(new_dir_entry.name, 0, 28); // remove garbage
      memcpy(new_dir_entry.name, name.c_str(), name.size());
      dir_entries[num_entries] = new_dir_entry;
      parentInode.size += sizeof(dir_ent_t);

      inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
      readInodeRegion(&super, inodes);
      inodes[parentInodeNumber - 1] = parentInode; // inodes start from 1 but array is 0-indexed
      writeInodeRegion(&super, inodes);

      int lastBlockIdx = parentNumBlocksAllocated - 1;
      disk->writeBlock(parentInode.direct[lastBlockIdx], dir_entries + ((UFS_BLOCK_SIZE / sizeof(dir_ent_t)) * lastBlockIdx));

      writeInodeBitmap(&super, inodeBitmap);
      writeDataBitmap(&super, dataBitmap);
    }
    else {
      if (parentNumBlocksAllocated == DIRECT_PTRS) return -ENOTENOUGHSPACE;

      int parentNewDataBlockNum = findFreeDataBlock(&super, dataBitmap);
      if (parentNewDataBlockNum < 0) return -ENOTENOUGHSPACE;

      dir_ent_t new_dir_entry_block[UFS_BLOCK_SIZE / sizeof(dir_ent_t)];
      dir_ent_t new_dir_entry = new_dir_entry_block[0];
      new_dir_entry.inum = inodeNum;
      memset(new_dir_entry.name, 0, 28); // remove garbage
      memcpy(new_dir_entry.name, name.c_str(), name.size());
      new_dir_entry_block[0] = new_dir_entry;
      parentInode.size += sizeof(dir_ent_t);
      parentInode.direct[parentNumBlocksAllocated] = parentNewDataBlockNum;

      writeInodeBitmap(&super, inodeBitmap);
      writeDataBitmap(&super, dataBitmap);

      inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
      readInodeRegion(&super, inodes);
      inodes[parentInodeNumber - 1] = parentInode; // inodes start from 1 but array is 0-indexed
      writeInodeRegion(&super, inodes);

      int lastBlockIdx = parentNumBlocksAllocated;
      disk->writeBlock(parentInode.direct[lastBlockIdx], dir_entries + ((UFS_BLOCK_SIZE / sizeof(dir_ent_t)) * lastBlockIdx));
    }
  }

  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inode_t newInode = inodes[inodeNum - 1]; // inodes start from 1 but array is 0-indexed
  newInode.type = type;
  newInode.size = type == UFS_REGULAR_FILE ? 0 : (2 * sizeof(dir_ent_t));
  newInode.direct[0] = dataBlockNum;
  inodes[inodeNum - 1] = newInode;
  writeInodeRegion(&super, inodes);

  if (type == UFS_DIRECTORY) {
    dir_ent_t new_dir_entries[UFS_BLOCK_SIZE / sizeof(dir_ent_t)];

    dir_ent_t this_dir_entry = new_dir_entries[0];
    this_dir_entry.inum = inodeNum;
    memset(this_dir_entry.name, 0, 28); // remove garbage
    this_dir_entry.name[0] = '.';
    new_dir_entries[0] = this_dir_entry;

    dir_ent_t parent_dir_entry = new_dir_entries[1];
    parent_dir_entry.inum = parentInodeNumber;
    memset(parent_dir_entry.name, 0, 28); // remove garbage
    parent_dir_entry.name[0] = '.';
    parent_dir_entry.name[1] = '.';
    new_dir_entries[1] = parent_dir_entry;

    disk->writeBlock(dataBlockNum, new_dir_entries);
  }

  return inodeNum;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  if (size <= 0) return -EINVALIDSIZE;

  inode_t inode;
  if (stat(inodeNumber, &inode) < 0) return -EINVALIDINODE;
  if (inode.type == UFS_DIRECTORY) return -EINVALIDTYPE;

  int numBlocksAllocated = numBlocksNeeded(inode.size);
  int requiredBlocks = numBlocksNeeded(size);

  super_t super;
  readSuperBlock(&super);

  if (requiredBlocks < numBlocksAllocated) {
    unsigned char dataBitmap[UFS_BLOCK_SIZE * super.data_bitmap_len];
    readDataBitmap(&super, dataBitmap);
    
    while (numBlocksAllocated > requiredBlocks) {
      freeDataBlock(inode.direct[numBlocksAllocated - 1], dataBitmap);
      numBlocksAllocated--;
    }
    writeDataBitmap(&super, dataBitmap);
  }
  else if (requiredBlocks > numBlocksAllocated) {
    unsigned char dataBitmap[UFS_BLOCK_SIZE * super.data_bitmap_len];
    readDataBitmap(&super, dataBitmap);
    bool bitmapModified = false;

    while (numBlocksAllocated < requiredBlocks) {
      if (numBlocksAllocated == DIRECT_PTRS) {
        size = MAX_FILE_SIZE;
        break;
      }

      int dataBlockNum = findFreeDataBlock(&super, dataBitmap);
      if (dataBlockNum < 0) {
        size = numBlocksAllocated * UFS_BLOCK_SIZE;
        break;
      }
      bitmapModified = true;
      inode.direct[numBlocksAllocated] = dataBlockNum;
      numBlocksAllocated++;
    }
    if (bitmapModified) writeDataBitmap(&super, dataBitmap);
  }

  inode.size = size;
  inode_t inodes[UFS_BLOCK_SIZE * super.inode_region_len];
  readInodeRegion(&super, inodes);
  inodes[inodeNumber - 1] = inode; // inodes start at 1 but array is 0-indexed
  writeInodeRegion(&super, inodes);

  for (int i = 0; i < numBlocksAllocated; i++) {
    disk->writeBlock(inode.direct[i], (char*)(buffer) + (UFS_BLOCK_SIZE * i));
  }
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

