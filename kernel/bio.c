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
#include "proc.h"

#define TABLE_SIZE 13

struct {
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf *table[TABLE_SIZE];
    struct spinlock table_lock[TABLE_SIZE];

} bcache;

void binit(void) {

    // Create linked list of buffers
    for (int i = 0; i < TABLE_SIZE; i++) {
        initlock(&bcache.table_lock[i], "bcache");
    }

    for (int i = 0; i < NBUF; i++) {
        initsleeplock(&bcache.buf[i].lock, "bcache");
    }
    acquire(&bcache.table_lock[1]);
    bcache.buf[0].next = 0;
    bcache.table[1] = &bcache.buf[0];
    bcache.table[1]->intable = 1;
    for (int i = 0; i < NBUF - 1; i++) {
        bcache.buf[i + 1].next = bcache.table[1];
        bcache.table[1] = &bcache.buf[i + 1];
        bcache.table[1]->intable = 1;
    }
    release(&bcache.table_lock[1]);
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {

    // for (int i = 0; i < NBUF; i++) {
    //     printf("buf %d refers to block %d and it is in %d buckect\n", i,
    //            bcache.buf[i].blockno, bcache.buf[i].intable);
    // }
    // printf("proc %d is looking up for block %d\n", myproc()->pid, blockno);
    struct buf *b = 0;

    // Is the block already cached?

    uint table_index = blockno % TABLE_SIZE;

    acquire(&bcache.table_lock[table_index]);
    for (b = bcache.table[table_index]; b != 0; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.table_lock[table_index]);
            acquiresleep(&b->lock);
            // printf("proc %d found block %d cached in bucket %d\n",
            // myproc()->pid, blockno, table_index);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    for (int i = 1; i < TABLE_SIZE; i++) {
        int next_table_index = (i + table_index) % TABLE_SIZE;
        acquire(&bcache.table_lock[next_table_index]);
        struct buf *pre = bcache.table[next_table_index];
        struct buf *cur = bcache.table[next_table_index];
        for (; cur != 0; cur = cur->next) {
            if (cur && cur->refcnt == 0) {
                b = cur;
                if (cur == pre && cur->next == 0) {
                    bcache.table[next_table_index] = 0;
                } else if (cur == pre && cur->next != 0) {
                    bcache.table[next_table_index] = cur->next;
                } else {
                    pre->next = cur->next;
                }
                b->next = bcache.table[table_index];
                bcache.table[table_index] = b;
                b->dev = dev;
                b->blockno = blockno;
                b->valid = 0;
                b->refcnt = 1;
                b->intable = table_index;

                release(&bcache.table_lock[table_index]);
                release(&bcache.table_lock[next_table_index]);
                acquiresleep(&b->lock);
                // printf("proc %d found block %d not cached in bucket %d\n",
                // myproc()->pid, blockno, next_table_index);
                return b;
            } else {
                pre = cur;
            }
        }
        // printf("bucket %d has %d buf\n", next_table_index, count);
        release(&bcache.table_lock[next_table_index]);
    }

    for (b = bcache.table[table_index]; b != 0; b = b->next) {
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            b->intable = table_index;
            release(&bcache.table_lock[table_index]);
            acquiresleep(&b->lock);
            // printf("proc %d found block %d not cached in bucket %d\n",
            // myproc()->pid, blockno, table_index);
            return b;
        }
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
        // Do nothing first.
        // printf("Found, and the next pointer is %p\n", b->next);
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
