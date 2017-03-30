# Introduce
It's a fuse plugin, and it can virtual merges files, and virtual replaces  bytes.

when system read the file, it output the merged/replaced content.

**Example**

1.txt
```
12345678
```
2.txt
```
abcdefghi
```

Make a file named `1-merged-.txt`, 

`-merged-` is a special words in the file name

JSON format
```
[
    {
        "path": "1.txt"
    },
    {
        "path": "2.txt"
    }
]
```

When you read the `cat 1-merged-.txt`, System output:
```
12345678abcdefghi
```

So it virtual merges two files, and used a json text, then take few disk space.

So it can virtual merges so many Big Size files. eg: `mkv, mp4`


# Install
It needs GCC 4.9, [Install GCC 4.9 in CentOS 6/7](#Install_GCC_4.9_in_CentOS 6)
```
cd merged-fuse
make
mv bin/merged-fuse /usr/bin/
```

# Usage
## Directories
It's a FUSE plugin, so it uses two directories:
- source dir
- be mounted dir

```
mkdir /the/src/dir
mkdir /the/mounted/dir
```

You can **Write** your `-merged-` file in source dir

And **Read** your `-merged-` file in mounted dir after `merged-fuse`

## Run
```
merged-fuse /the/src/dir/ /the/mount/dir/
```
## Stop
```
umount /the/mount/dir/
```

# Methods

## MERGE

```
vim /the/src/dir/game.of.thrones-merge-.mp4
```

```
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
## REPLACE
```
vim /the/src/dir/1-merge-.zip
```

- offset [long long]: may ±, current file's offset,

    If **offset** is negative, it will seeking from the end of file

    If **offset** is out of file, return error.

- length [short]: max value is 1024

    If **length** is greater than the offset to the end of file,
    it's fine, the replaced content will be truncate, and it'll not overflow. 


- content [string]: BASE64, the repleaced content(supported Binary)

    If **BASE64 DECODED Content**'s size is less than **length**, return error.

```
[
    {
        "path": "1.zip"
        "replaces": [
            {
                "offset": 10, //0xa
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

# Install GCC 4.9 in CentOS 6/7
```
$ yum install centos-release-scl-rh
$ yum install devtoolset-3-binutils devtoolset-3-gcc devtoolset-3-gcc-c++


# Backup your old gcc
$ mv /usr/bin/gcc /usr/bin/gcc-v4.4.7
$ mv /usr/bin/g++ /usr/bin/g++-v4.4.7

$ ln -s /opt/rh/devtoolset-3/root/usr/bin/gcc /usr/bin/gcc
$ ln -s /opt/rh/devtoolset-3/root/usr/bin/gcc /usr/bin/cc
$ ln -s /opt/rh/devtoolset-3/root/usr/bin/g++ /usr/bin/g++
```