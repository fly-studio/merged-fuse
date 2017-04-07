#include "common.h"


#include <fuse/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <unordered_map>
#include "concat.h"


using namespace std;

static struct fuse_operations m_opers/* = {
	.getattr	= m_getattr,
	.readlink       = m_readlink,
	.mknod          = m_mknod,
	.mkdir          = m_mkdir,
	.unlink         = m_unlink,
	.rmdir          = m_rmdir,
	.symlink        = m_symlink,
	.rename         = m_rename,
	.link           = m_link,
	.chmod          = m_chmod,
	.chown          = m_chown,
	.truncate       = m_truncate,
	.utime          = m_utime,
	.open		= m_open,
	.read		= m_read,
	.write          = m_write,
	.release        = m_release,
	.readdir	= m_readdir,
	.access         = m_access,
	.create         = m_create,
}*/;

static char src_dir[PATH_MAX];
static pthread_mutex_t the_lock;
typedef unordered_map<int, concat*> concatT;
static concatT open_files;

// sync lock
static void lock()
{
	pthread_mutex_lock(&the_lock);
}

// sync unlock
static void unlock()
{
	pthread_mutex_unlock(&the_lock);
}

static int is_merged_file(const char * path)
{
	char fpath[PATH_MAX];
	strncpy(fpath, path, sizeof(fpath));

	return (strstr(basename(fpath), "-merged-") != 0);
}

static concat* create_concat(int fd, const char* path, bool strict = true)
{
    lock();
    concat *c = new concat();
    try
    {
        c->setFile(fd, path);
        c->parsing(strict);
    } catch(exception &e)
    {
        debug_print("parsing error: %s", e.what());
        errno = EINVAL;
    }
    unlock();
    return c;
}

static concat* insert_concat(int fd, concat* c)
{
    lock();
    if (open_files.find(fd) != open_files.end())
    {
        unlock();
        errno = EBADF;
        return nullptr;
    }
    open_files.insert(concatT::value_type(fd, c));
    unlock();
    return c;
}

static int remove_concat(int fd)
{
    lock();
    auto it = open_files.find(fd);
    if (it != open_files.end())
    {
        delete it->second;
        open_files.erase(it);
    }
    unlock();
    return true;
}

static concat* get_concat(int fd)
{
    lock();
    auto it = open_files.find(fd);
    if (it != open_files.end())
    {
        unlock();
        return it->second;
    }

    errno = EBADF;
    unlock();
    return nullptr;
}

static int read_concat(int fd, void *buf, off_t offset, size_t count)
{
    concat * c = get_concat(fd);
    if (nullptr == c)
        return -errno;

    return c->valid() ? c->read(buf, offset, count) : -EINVAL;
}

static int m_open(const char *path, struct fuse_file_info *fi)
{
	int fd;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	fd = open(fpath, fi->flags);

	if (fd < 0) {
		return -errno;
	}

	fi->fh = fd;

	if (is_merged_file(path)) {
        debug_print("open file: %s\n", fpath);
        concat* c = create_concat(fd, fpath);
        if (!c->valid()) //normal file
            return 0;
        if (!insert_concat(fd, c))
        {
            delete c;
            return -errno;
        }
	}
	return 0;
}

static int m_write(
	const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	int rv = 0;

	if (is_merged_file(path)) {
        debug_print("write file: [%ld-%ld] %s \n", offset, size, path);
        concat * c = get_concat(fi->fh);
        if (c && c->valid())
            return -EINVAL;
	}
    rv = pwrite(fi->fh, buf, size, offset);
    if (rv < 0) {
        return -errno;
    }
	return rv;
}

static int m_read(const char *path, char *buf, size_t size, off_t offset,
			 struct fuse_file_info *fi)
{
	int rv = 0;

	if (is_merged_file(path)) {
        debug_print("read file: [%ld-%ld] %s \n", offset, size, path);
        concat * c = get_concat(fi->fh);
        if (c && c->valid())
            return read_concat(fi->fh, buf, offset, size);
	}

    rv = pread(fi->fh, buf, size, offset);
    if (rv < 0) {
        return -errno;
    }
	return rv;
}

static int m_release(const char * path, struct fuse_file_info * fi)
{
	if (is_merged_file(path)) {
        debug_print("close file: %s \n", path);
		remove_concat(fi->fh);
	}
    close(fi->fh);

	return 0;
}

static int m_getattr(const char *path, struct stat *stbuf)
{
	char fpath[PATH_MAX];
	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);
	memset(stbuf, 0, sizeof(struct stat));

	if (lstat(fpath, stbuf) != 0)
		return -errno;

	if (is_merged_file(path)) {
        debug_print("get file attr: %s \n", fpath);
        concat * c = create_concat(0, fpath, false);
        if (c->valid())
            stbuf->st_size = c->getMergedSize();
		delete c;
	}

	return 0;
}

static int m_mknod(const char *path, mode_t mode, dev_t dev)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = mknod(fpath, mode, dev);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_mkdir(const char *path, mode_t mode)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = mkdir(fpath, mode);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_unlink(const char *path)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = unlink(fpath);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_rmdir(const char *path)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = rmdir(fpath);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_symlink(const char *path, const char * link)
{
	int rv = 0;
	char flink[PATH_MAX];

	snprintf(flink, sizeof(flink), "%s/%s", src_dir, path);
#ifndef __CYGWIN__
	rv = symlink(path, flink);
#endif // __CYGWIN__
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_rename(const char *path, const char *topath)
{
	int rv;
	char fpath[PATH_MAX];
	char ftopath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);
	snprintf(ftopath, sizeof(ftopath), "%s/%s", src_dir, topath);

	rv = rename(fpath, ftopath);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_link(const char *path, const char *topath)
{
	int rv;
	char fpath[PATH_MAX];
	char ftopath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);
	snprintf(ftopath, sizeof(ftopath), "%s/%s", src_dir, topath);

	rv = link(fpath, ftopath);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_chmod(const char *path, mode_t mode)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = chmod(fpath, mode);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_chown(const char *path, uid_t uid, gid_t gid)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = chown(fpath, uid, gid);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_truncate(const char *path, off_t nsize)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = truncate(fpath, nsize);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_utime(const char *path, struct utimbuf * buf)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = utime(fpath, buf);
	if (rv < 0) {
		return -errno;
	}
	return rv;
}

static int m_access(const char *path, int mask)
{
	int rv;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	rv = access(fpath, mask);

	if (rv < 0) {
		return -errno;
	}

	return rv;
}

static int m_create(
	const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int fd = 0;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	fd = creat(fpath, mode);

	if (fd < 0) {
		return -errno;
	}

	fi->fh = fd;

	return 0;
}

static int m_readlink(const char *path, char *link, size_t size)
{
	int rv = 0;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);
#ifndef __CYGWIN__
	rv = readlink(fpath, link, size - 1);
#endif // __CYGWIN__
	if (rv < 0) {
		rv = -errno;
	} else {
		link[rv] = '\0';
		rv = 0;
	}

	return rv;
}

static int m_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			    off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;
	DIR *dp;
	struct dirent *de;
	char fpath[PATH_MAX];

	snprintf(fpath, sizeof(fpath), "%s/%s", src_dir, path);

	dp = opendir(fpath);

	if (!dp) {
		return -errno;
	}

	de = readdir(dp);
	if (de == 0) {
		closedir(dp);
		return -errno;
	}

	do {
		if (filler(buf, de->d_name, NULL, 0) != 0) {
			closedir(dp);
			return -ENOMEM;
		}
	} while ((de = readdir(dp)) != NULL);

	closedir(dp);

	return retstat;
}

static void usage()
{
	fprintf(stderr, "Usage: merged-fuse src-dir mounted-dir [-d] [-f] [-o fuse-options...]\n \
	-d: \n \
	\tEnable debugging output (implies -f).\n \
	-f: \n \
	\tRun in foreground; this is useful if you're running under a debugger.\n \
	\tWARNING: When -f is given, Fuse's working directory is the directory you were in when you started it. \n\
	\tWithout -f, Fuse changes directories to \"/\". This will screw you up if you use relative pathnames.\n \
	-o:\n \
	\tfuse-options, see http://man7.org/linux/man-pages/man8/mount.fuse.8.html#OPTIONS");
	exit(-1);
}

int main(int argc, char **argv) {

    if (argc < 3)
		usage();

	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr,
			"WARNING! merged-fuse does *no* file access checking "
			"right now and therefore is *dangerous* to use "
			"as root!");
	}

	if (argv[1][0] == '/') {
		strncpy(src_dir, argv[1], sizeof(src_dir));
	} else {
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		snprintf(src_dir, sizeof(src_dir), "%s/%s",
			 cwd, argv[1]);
	}

	debug_print("mounting src_dir: %s\n", src_dir);

	pthread_mutex_init(&the_lock, NULL);

	char ** argv_ = (char**) calloc(argc, sizeof(char*));
	argv_[0] = argv[0];
	memcpy(argv_ + 1, argv + 2, (argc - 2) * sizeof(char*));



	m_opers.getattr     = m_getattr;
	m_opers.readlink   = m_readlink;
	m_opers.mknod      = m_mknod;
	m_opers.mkdir      = m_mkdir;
	m_opers.unlink     = m_unlink;
	m_opers.rmdir      = m_rmdir;
	m_opers.symlink    = m_symlink;
	m_opers.rename     = m_rename;
	m_opers.link       = m_link;
	m_opers.chmod      = m_chmod;
	m_opers.chown      = m_chown;
	m_opers.truncate   = m_truncate;
	m_opers.utime      = m_utime;
	m_opers.open		= m_open;
	m_opers.read		= m_read;
	m_opers.write      = m_write;
	m_opers.release    = m_release;
	m_opers.readdir	= m_readdir;
	m_opers.access     = m_access;
	m_opers.create     = m_create;

	return fuse_main(argc - 1, argv_, &m_opers, NULL);
}
