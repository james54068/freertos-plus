#include "osdebug.h"
#include "filesystem.h"
#include "fio.h"

#include <stdint.h>
#include <string.h>
#include <hash-djb2.h>

#define MAX_FS 16

struct fs_t {
    uint32_t hash;
    fs_open_t cb;
    fs_list_t list_cb;
    void * opaque;
    const char * fs_name;
};

static struct fs_t fss[MAX_FS];

__attribute__((constructor)) void fs_init() {
    memset(fss, 0, sizeof(fss));
}

int register_fs(const char * mountpoint, fs_open_t callback, fs_list_t list,void * opaque) {
    int i;    /*mountpoint = "romfs" romfs_open = callback   &_sromfs = opaque*/
    DBGOUT("register_fs(\"%s\", %p, %p)\r\n", mountpoint, callback, opaque);
    
    for (i = 0; i < MAX_FS; i++) {/*0-15*/
        if (!fss[i].cb) {
            fss[i].hash = hash_djb2((const uint8_t *) mountpoint, -1);/*194595040*/
            fss[i].cb = callback;
            fss[i].list_cb = list;
            fss[i].opaque = opaque;
            fss[i].fs_name = mountpoint;
            return 0;
        }
    }
    
    return -1;
}
/*check which file system is open and call corresponding callback functions*/
int fs_open(const char * path, int flags, int mode) {
    const char * slash;
    uint32_t hash;
    int i;
    
    while (path[0] == '/')
        path++;
    
    slash = strchr(path, '/');/*no '/' will return 0*/
    
    if (!slash)/*no '/' will return -2*/
        return -2;

    hash = hash_djb2((const uint8_t *) path, slash - path);
    path = slash + 1;

    for (i = 0; i < MAX_FS; i++) {
        if (fss[i].hash == hash)/*check hash(/romfs/test.txt)*/
            return fss[i].cb(fss[i].opaque, path, flags, mode); /*with '/' but wrong return -1 */
    }
    
    return -2;
}

void fs_list(char * buf){
    int i;
    *buf = (char)0x00;
    for (i = 0; i < MAX_FS; i++) {
        if (!fss[i].cb) {
        return;
        }   
        strcat((char *)buf, (const char *)"\r\n");
        strcat((char *)buf, fss[i].fs_name);
    }
}

void list_fs_path(char * buf, const char * path) {

    int i;
    *buf = (char)0x00;
    if (strcmp((char *)path, (const char *)"/") == 0) {
        for (i = 0; i < MAX_FS; i++) {
            if (!fss[i].cb) {
                return;
            }
        strcat((char *)buf, (const char *)"\r\n");
        strcat((char *)buf, fss[i].fs_name);
        }
    }

    const char * slash;
    uint32_t hash;

    while (path[0] == '/')
    path++;

    slash = strchr(path, '/');

    if (!slash)
    return;

    hash = hash_djb2((const uint8_t *) path, slash - path);
    path = slash + 1;

    for (i = 0; i < MAX_FS; i++) {
    if (fss[i].hash == hash)
        fss[i].list_cb(fss[i].opaque, buf);
    }
}


