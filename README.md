# inifs

Educational filesystem allowing for FS traversal over INI file

## Example
For file `example.ini`:
```ini
[kowalski]
pos_x=10
pos_y=20
name=Jan Kowalski

[nowak]
pos_x=30
pos_y=40
name=Grzegorz Nowak
```

```console
$ mkdir test
$ inifs --ini=example.ini -f test &
$ tree test
test
├── kowalski
│   ├── name
│   ├── pos_x
│   └── pos_y
└── nowak
    ├── name
    ├── pos_x
    └── pos_y

2 directories, 6 files
```

## See also
- [Wikipedia on FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace)
- [libfuse](https://github.com/libfuse/libfuse)
