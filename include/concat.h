#ifndef CONCAT_H
#define CONCAT_H

#include <list>
#include <string>
#include <sys/types.h>
#include "common.h"

using namespace std;

typedef struct {
    string file_path;
    off_t file_size;
    int file_descriptor;
} chunk;

typedef struct {
    unsigned long long offset;
    unsigned short length;
    char * content;
} replaced;

class concat
{
    public:
        concat();
        virtual ~concat();

        void setFile(int, const char *);
        int parsing();
        void removeAll();

        off_t getMergedSize() {return merged_size;};
        int getFileDescriptor() {return file_descriptor;};
        string getFilePath() {return file_path;};

        long long read(void * buf, off_t offset, size_t count);
    public:
        list<chunk> chunks;
        list<replaced> replaces;

    protected:

    private:
        template<typename... Args> bool throw_exception(const char *, Args... args);
        void read_buffer(void * target, int len);
        void replace(void * buf, off_t offset, size_t count);
        int parseBinary();
        int parseJson();

        string file_path;
        int file_descriptor;
        char * buffer;
        char * buffer_offset;
        off_t file_size;
        off_t merged_size;
};

#endif // CONCAT_H
