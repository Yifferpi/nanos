typedef struct filesystem *filesystem;

typedef u64 inode;

typedef closure_type(filesystem_complete, void, filesystem, status);

typedef struct fsfile *fsfile;

filesystem fsfile_get_fs(fsfile f);
pagecache_volume filesystem_get_pagecache_volume(filesystem fs);

u64 fsfile_get_length(fsfile f);
void fsfile_set_length(fsfile f, u64 length);
sg_io fsfile_get_reader(fsfile f);
sg_io fsfile_get_writer(fsfile f);
pagecache_node fsfile_get_cachenode(fsfile f);

extern io_status_handler ignore_io_status;

#define MIN_EXTENT_SIZE PAGESIZE
#define MAX_EXTENT_SIZE (1 * MB)

boolean filesystem_probe(u8 *first_sector, u8 *uuid, char *label);
const char *filesystem_get_label(filesystem fs);
void filesystem_get_uuid(filesystem fs, u8 *uuid);

void create_filesystem(heap h,
                       u64 blocksize,
                       u64 size,
                       block_io read,
                       block_io write,
                       block_flush flush,
                       const char *label,
                       filesystem_complete complete);
void destroy_filesystem(filesystem fs);

// turn these into method gets rather than call
void filesystem_read_sg(fsfile f, sg_list sg, range q, status_handler completion);

void filesystem_write_sg(fsfile f, sg_list sg, range q, status_handler completion);

/* deprecate these if we can */
void filesystem_read_linear(fsfile f, void *dest, range q, io_status_handler completion);
void filesystem_write_linear(fsfile f, void *src, range q, io_status_handler completion);

void filesystem_flush(filesystem fs, status_handler completion);

void filesystem_reserve(filesystem fs);
void filesystem_release(filesystem fs);

timestamp filesystem_get_atime(filesystem fs, tuple t);
timestamp filesystem_get_mtime(filesystem fs, tuple t);
void filesystem_set_atime(filesystem fs, tuple t, timestamp tim);
void filesystem_set_mtime(filesystem fs, tuple t, timestamp tim);

#define filesystem_update_atime(fs, t) \
    filesystem_set_atime(fs, t, now(CLOCK_ID_REALTIME))
#define filesystem_update_mtime(fs, t) \
    filesystem_set_mtime(fs, t, now(CLOCK_ID_REALTIME))

u64 fsfile_get_length(fsfile f);
void fsfile_set_length(fsfile f, u64);
u64 fsfile_get_blocks(fsfile f);    /* returns the number of allocated blocks */
fsfile fsfile_from_node(filesystem fs, tuple n);
fsfile file_lookup(filesystem fs, vector v);
void filesystem_read_entire(filesystem fs, tuple t, heap bufheap, buffer_handler c, status_handler s);
fsfile allocate_fsfile(filesystem fs, tuple md);
void fsfile_reserve(fsfile f);
void fsfile_release(fsfile f);
// XXX per-file flush

typedef enum {
    FS_STATUS_OK = 0,
    FS_STATUS_NOSPACE,
    FS_STATUS_IOERR,
    FS_STATUS_NOENT,
    FS_STATUS_EXIST,
    FS_STATUS_INVAL,
    FS_STATUS_NOTDIR,
    FS_STATUS_ISDIR,
    FS_STATUS_NOTEMPTY,
    FS_STATUS_NOMEM,
    FS_STATUS_LINKLOOP,
    FS_STATUS_NAMETOOLONG,
    FS_STATUS_XDEV,
    FS_STATUS_FAULT
} fs_status;

const char *string_from_fs_status(fs_status s);

fs_status filesystem_write_tuple(filesystem fs, tuple t);
fs_status filesystem_write_eav(filesystem fs, tuple t, symbol a, value v);

typedef closure_type(fs_status_handler, void, fsfile, fs_status);

void filesystem_alloc(fsfile f, long offset, long len,
        boolean keep_size, fs_status_handler completion);
void filesystem_dealloc(fsfile f, long offset, long len,
        fs_status_handler completion);
fs_status filesystem_truncate(filesystem fs, fsfile f, u64 len);

fs_status do_mkentry(filesystem fs, tuple parent, const char *name, tuple entry,
        boolean persistent);

fs_status filesystem_mkentry(filesystem fs, tuple cwd, const char *fp, tuple entry,
    boolean persistent, boolean recursive);
fs_status filesystem_mkdirpath(filesystem fs, tuple cwd, const char *fp,
        boolean persistent);
fs_status filesystem_mkdir(filesystem fs, inode cwd, const char *path);
fs_status filesystem_get_node(filesystem *fs, inode cwd, const char *path, boolean nofollow,
                              boolean create, boolean exclusive, tuple *n, fsfile *f);
void filesystem_put_node(filesystem fs, tuple n);
tuple filesystem_get_meta(filesystem fs, inode n);
void filesystem_put_meta(filesystem fs, tuple n);
fsfile filesystem_creat_unnamed(filesystem fs);
fs_status filesystem_symlink(filesystem fs, inode cwd, const char *path, const char *target);
fs_status filesystem_delete(filesystem fs, inode cwd, const char *path, boolean directory);
fs_status filesystem_rename(filesystem oldfs, inode oldwd, const char *oldpath,
                            filesystem newfs, inode newwd, const char *newpath,
                            boolean noreplace);
fs_status filesystem_exchange(filesystem fs1, inode wd1, const char *path1,
                              filesystem fs2, inode wd2, const char *path2);

fs_status filesystem_mk_socket(filesystem *fs, inode cwd, const char *path, void *s, inode *n);
fs_status filesystem_get_socket(filesystem *fs, inode cwd, const char *path, tuple *n, void **s);
fs_status filesystem_clear_socket(filesystem fs, inode n);

fs_status filesystem_mount(filesystem parent, inode mount_dir, filesystem child);
void filesystem_unmount(filesystem parent, inode mount_dir, filesystem child, thunk complete);

tuple filesystem_getroot(filesystem fs);

u64 fs_blocksize(filesystem fs);
u64 fs_totalblocks(filesystem fs);
u64 fs_usedblocks(filesystem fs);
u64 fs_freeblocks(filesystem fs);

extern const char *gitversion;

#define NAME_MAX 255
#define PATH_MAX 4096

static inline inode inode_from_tuple(tuple t) {
    return u64_from_pointer(t);
}

static inline buffer linktarget(tuple x)
{
    return get_string(x, sym(linktarget));
}

static inline boolean is_dir(tuple n)
{
    return children(n) ? true : false;
}

static inline boolean is_symlink(tuple n)
{
    return linktarget(n) ? true : false;
}

static inline boolean is_socket(tuple n)
{
    return get(n, sym(socket)) ? true : false;
}

static inline char *path_find_last_delim(const char *path, unsigned int len)
{
    return (char *)utf8_findn_r((u8 *)path, len, '/');
}

static inline const char *filename_from_path(const char *path)
{
    const char *filename = path_find_last_delim(path, PATH_MAX);
    if (!filename) {
        filename = path;
    } else {
        filename++;
    }
    return filename;
}

/* Expects an empty buffer, and never resizes the buffer. */
boolean dirname_from_path(buffer dest, const char *path);

void fs_set_path_helper(filesystem (*get_root_fs)(), inode (*get_mountpoint)(tuple, filesystem *));

int filesystem_resolve_cstring(filesystem *fs, tuple cwd, const char *f, tuple *entry,
                    tuple *parent);

/* Same as resolve_cstring(), except that if the entry is a symbolic link this
 * function follows the link (recursively). */
int filesystem_resolve_cstring_follow(filesystem *fs, tuple cwd, const char *f, tuple *entry,
        tuple *parent);

int filesystem_follow_links(filesystem *fs, tuple link, tuple parent,
                            tuple *target);

int file_get_path(filesystem fs, inode ino, char *buf, u64 len);

#ifdef KERNEL

/* Functions called by TFS code to notify filesystem operations */
void fs_notify_create(tuple t, tuple parent, symbol name);
void fs_notify_move(tuple t, tuple old_parent, symbol old_name, tuple new_parent, symbol new_name);
void fs_notify_delete(tuple t, tuple parent, symbol name);
void fs_notify_modify(tuple t);
void fs_notify_release(tuple t, boolean unmounted);

#else

#define fs_notify_create(t, p, n)
#define fs_notify_move(t, op, on, np, nn)
#define fs_notify_delete(t, p, n)
#define fs_notify_modify(t)
#define fs_notify_release(t, u)             (void)(t)

#endif
