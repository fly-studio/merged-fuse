# Introduce
It's a fuse plugin, and it can virtual merges files, and virtual replaces bytes.

when system read this file, it output the merged/replaced content of many really files that you set.

But it's not a really merged file, it only output the merged content when you use `fread pread, even md5...` in linux.

So, you can save so many disk space.

> the code is base by https://github.com/schlaile/concatfs, Thanks!

**Example**

`/the/src/dir/`1.txt

```text
12345678
```

`/the/src/dir/`2.txt

```text
abcdefghi
```

Make a file named `/the/mounted/dir/`1-merged-.txt, 

`-merged-` is a special words in the file name, and the content is

**JSON format**
```json
[
    {
        "path": "1.txt"
    },
    {
        "path": "2.txt"
    }
]
```

`cat /the/mounted/dir/1-merged-.txt`, output:

```text
12345678abcdefghi
```

So it virtual merges two files, with a json, and output the merged content.

> Of course, this plugin builded for **BIG SIZE** files. eg: `mkv, mp4`,
>
> Or so many **BIG SIZE** copies with a few difference. eg: program copies(with different biz channel ID) 

# Install
It needs GCC 4.9, [Install GCC 4.9 in CentOS 6/7](#install-gcc-49-in-centos-67)

> modern C++ 11

```bash
yum install fuse fuse-devel

cd merged-fuse
make
mv bin/merged-fuse /usr/bin/
```

# Usage
## Directories
It's a FUSE plugin, so it uses two directories:
- source dir
- be mounted dir

```bash
mkdir /the/src/dir
mkdir /the/mounted/dir
```

You can **Write** your `-merged-` file in source dir

And **Read** your `-merged-` file in mounted dir after `merged-fuse`

## Run in background
```bash
merged-fuse /the/src/dir/ /the/mount/dir/
```
## Run in debugging

> see https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201109/homework/fuse/fuse_doc.html#other-options

```
merged-fuse /the/src/dir/ /the/mount/dir/ -d
```

## Run in other user
make your config

> see http://manpages.ubuntu.com/manpages/precise/man8/mount.fuse.8.html

```
vim  /etc/fuse.conf 
```
```
user_allow_other
```
run with user 'apache' and allow other user read/write
```
sudo -u apache merged-fuse /the/src/dir/ /the/mount/dir/ -o allow_other
```

## Stop
```bash
umount /the/mounted/dir/
```

# Methods

## MERGE
Write
```bash
/the/src/dir/game.of.thrones-merge-.mp4
```

```json
[
    {
        "path": "1.mp4"
    },
    {
        "path": "2.mp4"
    },
    {
        "path": "3.mp4"
    }
]
```
Read

```bash
/the/mounted/dir/game.of.thrones-merge-.mp4
```

## REPLACE
Write
```bash
/the/src/dir/1-merge-.zip
```

- offset [long long]: may ±, current file's offset,

    If **offset** is negative, it will seeking from the end of file

    If **offset** is out of file, return error.

- length [short]: max value is 1024

    If **length** is greater than the offset to the end of file,
    it's fine, the replaced content will be truncate, and it'll not overflow. 


- content [string]: BASE64, the repleaced content(supported Binary)

    If **BASE64 DECODED Content**'s size is less than **length**, return error.

```json
[
    {
        "path": "1.zip",
        "replaces": [
            {
                "offset": 10,
                "length": 4,
                "content": "NDMyMQ=="
            },
            {
                "offset": -2,
                "length": 2,
                "content": "MjE="
            }
        ]
    },
    {
        "path": "2.text"
    }
]
```
Read

```bash
/the/mounted/dir/1-merge-.zip
```

# Install GCC 4.9 in CentOS 6/7
```bash
$ yum install centos-release-scl-rh
$ yum install devtoolset-3-binutils devtoolset-3-gcc devtoolset-3-gcc-c++


# Backup your old gcc
$ mv /usr/bin/gcc /usr/bin/gcc-v4.4.7
$ mv /usr/bin/g++ /usr/bin/g++-v4.4.7

$ ln -s /opt/rh/devtoolset-3/root/usr/bin/gcc /usr/bin/gcc
$ ln -s /opt/rh/devtoolset-3/root/usr/bin/gcc /usr/bin/cc
$ ln -s /opt/rh/devtoolset-3/root/usr/bin/g++ /usr/bin/g++
```