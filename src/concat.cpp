#include "concat.h"

#include <fuse/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include "json.hpp"
#include "base64.h"

using namespace std;
using json = nlohmann::json;

concat::concat()
{
    // init
    buffer = 0;
    buffer_offset = 0;
    file_size = 0;
    merged_size = 0;
    file_path = "";
    file_descriptor = -1;
    //ctor
}

concat::~concat()
{
    //dtor
    removeAll();
}

void concat::setFile(int fd, const char *path)
{
    removeAll();

    file_path = path;
    file_descriptor = fd;
    struct stat stbuf;
    if (stat(file_path.c_str(), &stbuf) != 0)
    {
        debug_print("set error file %s\n", path);
        return;
    }

    file_size = stbuf.st_size;
    debug_print("set file %s: %d\n", path, file_size);
}

long long concat::read(void * buf, off_t offset, size_t count)
{
    list<chunk>::iterator it = chunks.begin();
    ssize_t bytes_read = 0;
    off_t off = offset;
    void * _b = buf;

    // set offset to matched file's offset
    for(;it != chunks.end() && offset > it->file_size; ++it)
        offset -= it->file_size;

    // cross files
    for(;it != chunks.end() && count > it->file_size - offset; ++it)
    {
        ssize_t rv = pread(it->file_descriptor, buf, it->file_size - offset, offset);
        if (rv == it->file_size - offset) {
			buf += rv;
			offset = 0;
			count -= rv;
			bytes_read += rv;
		} else if (rv > 0) {
			bytes_read += rv;
			replace(_b, off, bytes_read);
			return bytes_read;
		} else {
			return -errno;
		}
    }

    if (it != chunks.end() && count > 0) {
		ssize_t rv = pread(it->file_descriptor, buf, count, offset);

		if (rv < 0) {
			return -errno;
		}

		bytes_read += rv;
	}
    replace(_b, off, bytes_read);
	return bytes_read;
}

void concat::replace(void * buf, off_t offset, size_t count)
{
    off_t s = offset;
    off_t e = offset + count;


    for(list<replaced>::iterator it = replaces.begin(); it != replaces.end(); ++it)
    {
        off_t _s = it->offset; // s'
        off_t _e = it->offset + it->length; // e'
        /*
        http://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
        ---------------------s|-------A-------|e---------------------
            |----B1----|
                   |----B2----|e'
                     s'|----B3----|e'
                     s'|----------B4----------|e'
                     s'|----------------B5----------------|e'
                            s'|----B6----|e'
        ---------------------s|-------A-------|e---------------------
                            s'|------B7-------|e'
                            s'|----------B8-----------|e'
                               s'|----B9----|e'
                               s'|----B10-----|e'
                               s'|--------B11--------|e'
                                            s'|----B12----|
                                                 |----B13----|
        ---------------------s|-------A-------|e---------------------
        */
        //Overlap
        off_t len = std::max(0l, std::min(e, _e) - std::max(s, _s));
        if (len > 0) // s < _e && e > _s
        {
            off_t ws = std::max(0l, _s - s); //write start B9 B10 B11
            off_t rs = std::max(0l, s - _s); //read start B3 B4 B5
            memcpy(buf + ws, it->content + rs, len);
        }
    }

}


int concat::parsing()
{
    if (file_path.empty() || file_size < 2) {
        return throw_exception("File content invalid");
    }

    FILE * fp;

//	if (file_descriptor >= 0) {
//		fp = fdopen(dup(file_descriptor), "r");
//	} else {
		fp = fopen(file_path.c_str(), "r");
//	}

    buffer = new char[file_size];
    fread(buffer, sizeof(char), file_size, fp);
	fclose(fp);
    buffer_offset = buffer;

    int ret = 0;
    try{
        if (buffer[0] == '[' && buffer[file_size - 1] == ']') // json
            ret = parseJson();
        else
            ret = parseBinary();
    } catch(exception &e){
        throw_exception(e.what());
    }

    delete []buffer;
    buffer = 0;
    buffer_offset = 0;

    return ret ?: true;
}

/**
 * BINARY STRUCT
 * 2B   string  MG
 * 2B   uint    version
 * 2B	uint	merged files count
 * [
 * 	2B	uint	path length
 * 	*	string	path string
 * 	2B	uint	offsets count
 * 	[
 * 		8B	uint	string offset
 * 		2B	uint	replaced content length
 * 		*	binary	replaced
 * 	]
 * ]
 *
 *
 */
int concat::parseBinary()
{
    debug_print("parsing binary: %s\n", file_path);
    unsigned short path_length = 0;
    unsigned short chunks_count = 0;
    unsigned short replaced_count = 0;
    unsigned short content_length = 0;
    unsigned short version = 0;
    long long replaced_offset = 0;
    off_t off = 0;
    char fpath[PATH_MAX + 1], tpath[PATH_MAX + 1], content[1024], path[PATH_MAX + 1];
    struct stat stbuf;

    file_path.copy(path, sizeof(path) - 1);path[PATH_MAX] = '\0';
    char * base_dir = dirname(path);

    read_buffer(&content, 2);
    if (content[0] != 'M' && content[1] != 'G')
        return throw_exception("file format invalid");
    //read version
    read_buffer(&version, 2);
    //read chunks count
    read_buffer(&chunks_count, 2);

    if (chunks_count > 50)
        return throw_exception("the chunks is too much: > 50");

    for(int i = 0; i < chunks_count; i++) {

        chunk c;
        read_buffer(&path_length, 2);

        if (path_length > PATH_MAX)
            return throw_exception("file path is too long: > 255B");

        read_buffer(&fpath, path_length);
        fpath[path_length] = '\0';

		if (fpath[0] == '/') {
			strncpy(tpath, fpath, sizeof(tpath));
		} else {
			snprintf(tpath, sizeof(tpath), "%s/%s", base_dir, fpath);
		}
        c.file_path = tpath;
        c.file_descriptor = open(tpath, O_RDONLY);

        if (stat(tpath, &stbuf) == 0) {
            merged_size += stbuf.st_size;
            c.file_size = stbuf.st_size;

            read_buffer(&replaced_count, 2);

            if (replaced_count > 100)
                return throw_exception("replaces count is too much: > 100");

            //read replaces
            for(int j = 0; j < replaced_count; ++j)
            {

                read_buffer(&replaced_offset, 8);
                read_buffer(&content_length, 2);

                if (content_length > 1024)
                    return throw_exception("replaced content is too long: > 1024B");
                else if (content_length == 0)
                    continue;

                read_buffer(&content, content_length);

                if (replaced_offset >= c.file_size || replaced_offset < -c.file_size)
                    continue;
                    //return throw_exception("replaced offset is overflow.");

                if (replaced_offset < 0)
                    replaced_offset = c.file_size + replaced_offset;
                //if content_length out of file;
                if (replaced_offset + content_length > c.file_size)
                    content_length = c.file_size - replaced_offset;

                char * str = new char[content_length];
                memcpy(str, content, content_length);

                replaced r;
                r.offset = off + replaced_offset;
                r.length = content_length;
                r.content = str;

                replaces.push_back(r);
            }

            off += stbuf.st_size;
        } else {
            return throw_exception("merged file not exists");
        }

        chunks.push_back(c);
    }
    return true;
}

/**
 * JSON format
 * [
 *    {
 *      "path": "1.txt"
 *    },
 *    {
 *      "path": "2.txt",
 *      "replaces": [
 *        {
 *          "offset": 12345,
 *          "length": 12,
 *          "content": "base64"
 *        }
 *      ]
 *    }
 * ]
 */
int concat::parseJson()
{
    debug_print("parsing json: %s\n", file_path.c_str());
    int content_length = 0;
    long long replaced_offset = 0;
    off_t off = 0;
    char path[PATH_MAX + 1];
    string content;
    struct stat stbuf;

    file_path.copy(path, sizeof(path) - 1);path[PATH_MAX] = '\0';

    char * base_dir = dirname(path);

    json j;
    //try {
        j = json::parse(buffer);
        if (!j.is_array())
            return throw_exception("json format error.");
    //} catch(exception& e) {
    //   return throw_exception(e.what());
    //}
    debug_print("%d merged files\n", j.size());
    for (json::iterator ct = j.begin(); ct != j.end(); ++ct) {
        chunk c;

        auto cv = ct.value();
        if (!cv.is_object())
            return throw_exception("json format error.");
        else if (cv.find("path") == cv.end())
            return throw_exception("[json]data needs key: path");

        string path = cv.at("path");

		if (path[0] != '/') {
            path.insert(0, "/");
			path.insert(0, base_dir);
		}
        c.file_path = path;
        c.file_descriptor = open(path.c_str(), O_RDONLY);

        if (stat(path.c_str(), &stbuf) == 0) {
            merged_size += stbuf.st_size;
            c.file_size = stbuf.st_size;

            auto prs = cv.find("replaces");
            if (prs != cv.end())
            {
                auto rs = *prs;
                if (!rs.is_array())
                    return throw_exception("json format error");
                for (json::iterator rt = rs.begin(); rt != rs.end(); ++rt) {
                    auto rv = rt.value();

                    if (rv.find("offset") == rv.end() || rv.find("length") == rv.end() || rv.find("content") == rv.end())
                        return throw_exception("[json]replace needs keys: offset, length, content");
                    replaced_offset = rv.at("offset");
                    content_length = rv.at("length");

                    if (content_length > 1024)
                        return throw_exception("replaced content is too long: > 1024B");
                    else if (content_length <= 0)
                        continue;

                    if (replaced_offset >= c.file_size || replaced_offset < -c.file_size)
                        continue;
                        //return throw_exception("replaced offset is overflow.");

                    if (replaced_offset < 0)
                        replaced_offset = c.file_size + replaced_offset;
                    //if content_length out of file;
                    if (replaced_offset + content_length > c.file_size)
                        content_length = c.file_size - replaced_offset;

                    content = rv.at("content");
                    std::vector<BYTE> decode = base64_decode(content);
                    debug_print("base64:%s\n", content.c_str());

                    if (decode.size() < content_length)
                        return throw_exception("replaced content base64decode error.");

                    char * str = new char[content_length + 1];
                    memcpy(str, decode.data(), content_length);str[content_length] = '\0';

                    replaced r;
                    r.offset = off + replaced_offset;
                    r.length = content_length;
                    r.content = str;

                    replaces.push_back(r);
                    debug_print("replaced %d, %d\n", replaced_offset, content_length);
                }
            }

            off += stbuf.st_size;
        } else {
            return throw_exception("merged file not exists");
        }

        chunks.push_back(c);
    }
    return true;
}

void concat::removeAll()
{
    file_path.clear();
    file_size = 0;
    merged_size = 0;
    file_descriptor = -1;
    if (0 != buffer) delete []buffer;
    buffer = 0;
    buffer_offset = 0;
    for(list<replaced>::iterator it = replaces.begin(); it != replaces.end(); ++it)
        if (0 != it->content) delete[] it->content;
    replaces.clear();
    for(list<chunk>::iterator it = chunks.begin(); it != chunks.end(); ++it)
        if (0 != it->file_descriptor) close(it->file_descriptor);
    chunks.clear();

}

void concat::read_buffer(void * target, int len)
{
    if (buffer_offset + len > buffer + file_size) {
        throw_exception("file format invalid.");
        return ;
    }
    memcpy(target, buffer_offset, len);
}

template<typename... Args> bool concat::throw_exception(const char * fmt, Args... args)
{
    removeAll();
    char str[2048];
    snprintf(str, 2048, fmt, args...);
    throw std::runtime_error(str);
    //debug_print("%s\n", str);
    return false;
}
