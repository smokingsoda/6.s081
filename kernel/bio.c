// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define TABLE_SIZE 13

struct {
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf *table[TABLE_SIZE];
    struct spinlock table_lock[TABLE_SIZE];
    struct buf *unused_buf;
    struct spinlock unused_buf_lock;

} bcache;

void binit(void) {

    initlock(&bcache.unused_buf_lock, "bcache");

    // Create linked list of buffers
    for (int i = 0; i < TABLE_SIZE; i++) {
        initlock(&bcache.table_lock[i], "bcache");
    }

    for (int i = 0; i < NBUF; i++) {
        initsleeplock(&bcache.buf[i].lock, "bcache");
    }
    acquire(&bcache.unused_buf_lock);
    bcache.unused_buf = &bcache.buf[0];
    bcache.unused_buf->next = 0;
    for (int i = 0; i < NBUF - 1; i++) {
        bcache.buf[i + 1].next = bcache.unused_buf;
        bcache.unused_buf = &bcache.buf[i + 1];
    }
    release(&bcache.unused_buf_lock);
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
    struct buf *b = 0;

    // Is the block already cached?

    uint table_index = blockno % TABLE_SIZE;

    acquire(&bcache.table_lock[table_index]);
    for (b = bcache.table[table_index]; b != 0; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.table_lock[table_index]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    acquire(&bcache.unused_buf_lock);
    if (bcache.unused_buf && bcache.unused_buf->refcnt == 0) {
        b = bcache.unused_buf;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        bcache.unused_buf = b->next;
        struct buf *cur = bcache.table[table_index];
        b->next = cur;
        bcache.table[table_index] = b;
        release(&bcache.unused_buf_lock);
        release(&bcache.table_lock[table_index]);
        acquiresleep(&b->lock);
        return b;
    }
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    uint table_index = b->blockno % TABLE_SIZE;

    acquire(&bcache.table_lock[table_index]);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        acquire(&bcache.unused_buf_lock);
        struct buf *pre = bcache.table[table_index];
        struct buf *cur = bcache.table[table_index];
        if (b == cur) {
            bcache.table[table_index] = cur->next;
        } else {
            for (cur = cur->next; cur != 0;
                 cur = cur->next) {
                if (cur == b) {
                    pre->next = cur->next;
                    break;
                } else {
                    pre = cur;
                }
            }
        }
        b->next = bcache.unused_buf;
        bcache.unused_buf = b;
        release(&bcache.unused_buf_lock);
    }
    release(&bcache.table_lock[table_index]);
}

void bpin(struct buf *b) {
    uint table_index = b->blockno % TABLE_SIZE;
    acquire(&bcache.table_lock[table_index]);
    b->refcnt++;
    release(&bcache.table_lock[table_index]);
}

void bunpin(struct buf *b) {
    uint table_index = b->blockno % TABLE_SIZE;
    acquire(&bcache.table_lock[table_index]);
    b->refcnt--;
    release(&bcache.table_lock[table_index]);
}

