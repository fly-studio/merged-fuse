#include "include/common.h"


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
#include "include/concat.h"

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

static concat* create_concat(int fd, const char* path)
{
    lock();
    concat *c = new concat();
    try
    {
        c->setFile(fd, path);
        c->parsing();
    } catch(exception &e)
    {
        delete c;
        fprintf(stderr, e.what());
        errno = EINVAL;
        unlock();
        return nullptr;
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

    return c->read(buf, offset, count);
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
        concat* c = create_concat(fd, fpath);
        if (nullptr == c)
            return -errno;
        if (!insert_concat(fd, c))
            return -errno;
	}
	return 0;
}

static int m_write(
	const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	int rv = 0;

	if (is_merged_file(path)) {
		return -EINVAL;
	} else {
		rv = pwrite(fi->fh, buf, size, offset);
		if (rv < 0) {
			return -errno;
		}
	}
	return rv;
}

static int m_read(const char *path, char *buf, size_t size, off_t offset,
			 struct fuse_file_info *fi)
{
	int rv = 0;

	if (is_merged_file(path)) {
		return read_concat(fi->fh, buf, size, offset);
	} else {
		rv = pread(fi->fh, buf, size, offset);
		if (rv < 0) {
			return -errno;
		}
	}
	return rv;
}

static int m_release(const char * path, struct fuse_file_info * fi)
{
	if (is_merged_file(path)) {
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
        concat * c = create_concat(0, path);
        if (nullptr == c)
            return -errno;
		stbuf->st_size = c->getMergedSize();
		delete c;
	}

	return 0;
}


static void usage()
{
	fprintf(stderr, "Usage: merged-fuse src-dir mounted-dir [-o] fuse-options...\n");
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

	pthread_mutex_init(&the_lock, NULL);

	char ** argv_ = (char**) calloc(argc, sizeof(char*));
	argv_[0] = argv[0];
	memcpy(argv_ + 1, argv + 2, (argc - 2) * sizeof(char*));



	m_opers.getattr     = m_getattr;
	//m_opers.readlink   = m_readlink;
	//m_opers.mknod      = m_mknod;
	//m_opers.mkdir      = m_mkdir;
	//m_opers.unlink     = m_unlink;
	//m_opers.rmdir      = m_rmdir;
	//m_opers.symlink    = m_symlink;
	//m_opers.rename     = m_rename;
	//m_opers.link       = m_link;
	//m_opers.chmod      = m_chmod;
	//m_opers.chown      = m_chown;
	//m_opers.truncate   = m_truncate;
	//m_opers.utime      = m_utime;
	m_opers.open		= m_open;
	m_opers.read		= m_read;
	m_opers.write      = m_write;
	m_opers.release    = m_release;
	//m_opers.readdir	= m_readdir;
	//m_opers.access     = m_access;
	//m_opers.create     = m_create;

	return fuse_main(argc - 1, argv_, &m_opers, NULL);
}

