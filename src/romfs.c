#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs.h"
#include "osdebug.h"
#include "hash-djb2.h"
#include "clib.h"
#include "shell.h"

struct romfs_fds_t {
    const uint8_t * file;
    uint32_t cursor;
};

static struct romfs_fds_t romfs_fds[MAX_FDS];
static uint32_t get_unaligned(const uint8_t * d) {
    return ((uint32_t) d[0]) | ((uint32_t) (d[1] << 8)) | ((uint32_t) (d[2] << 16)) | ((uint32_t) (d[3] << 24));
}

/*ssize_t = signed long int */
static ssize_t romfs_read(void * opaque, void * buf, size_t count) {
    struct romfs_fds_t * f = (struct romfs_fds_t *) opaque;
    const uint8_t * size_p = f->file - 4;
    uint32_t size = get_unaligned(size_p);
    
    if ((f->cursor + count) > size)
        count = size - f->cursor;

    memcpy(buf, f->file + f->cursor, count);
    f->cursor += count;

    return count;
}

static off_t romfs_seek(void * opaque, off_t offset, int whence) {
    struct romfs_fds_t * f = (struct romfs_fds_t *) opaque;
    const uint8_t * size_p = f->file - 4;
    uint32_t size = get_unaligned(size_p);
    uint32_t origin;
    
    switch (whence) {
    case SEEK_SET:
        origin = 0;
        break;
    case SEEK_CUR:
        origin = f->cursor;
        break;
    case SEEK_END:
        origin = size;
        break;
    default:
        return -1;
    }

    offset = origin + offset;

    if (offset < 0)
        return -1;
    if (offset > size)
        offset = size;

    f->cursor = offset;

    return offset;
}

const uint8_t * romfs_get_file_by_hash(const uint8_t * romfs, uint32_t h, uint32_t *len) {
    const uint8_t * meta;              /*romfs=0xdba4 <_sromfs> , h=3764664357, len=0x0 */
    for (meta = romfs; get_unaligned(meta) && get_unaligned(meta + 8 + get_unaligned(meta+4)); meta += get_unaligned(meta + 8 + get_unaligned(meta+4)) + get_unaligned(meta+4) +12) {
        if (get_unaligned(meta) == h) {/*check hash(test.txt)*/
 
            if (filedump_flag == 1)
                return meta + get_unaligned(meta+4) + 12;
            else if(filedump_flag == 0)
                return meta + 8;
        }
    }

    return NULL;
}


static int romfs_open(void * opaque, const char * path, int flags, int mode) {
    uint32_t h = hash_djb2((const uint8_t *) path, -1); /*hash(test.txt)*/
    const uint8_t * romfs = (const uint8_t *) opaque;
    const uint8_t * file;
    int r = -1;

//    if(filedump_flag == 1)
    file = romfs_get_file_by_hash(romfs, h, NULL);/*check hash(test.txt)*/
/*    else if(filedump_flag == 0)
    file = romfs_get_file_by_hash(romfs, h, 0);
*/

    if (file) {
        r = fio_open(romfs_read, NULL, romfs_seek, NULL, NULL);
        if (r > 0) {
            romfs_fds[r].file = file;
            romfs_fds[r].cursor = 0;
            fio_set_opaque(r, romfs_fds + r);
        }
    }
    return r;/*romfs_fds[3], r = 3 */
}
   
static void romfs_list(void * opaque, char * filename) {
    const uint8_t * meta;
    //*filename = (char)0x00;
    for (meta = opaque; get_unaligned(meta) && get_unaligned(meta + 4); meta += get_unaligned(meta + 4) + get_unaligned(meta + 8) + 12) {
        strcat((char *)filename, (const char *)"\r\n");
        strncat((char *)filename, (char *)(meta + 12), get_unaligned(meta + 8));
    }
}

void register_romfs(const char * mountpoint, const uint8_t * romfs) {
//    DBGOUT("Registering romfs `%s' @ %p\r\n", mountpoint, romfs);
    /*mountpoint="romfs", * romfs = &_sromfs*/
    register_fs(mountpoint, romfs_open, romfs_list, (void *) romfs);
}
