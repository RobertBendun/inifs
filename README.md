# inifs

Educational filesystem allowing for FS traversal over INI file

| Filesystem | INI |
| :---: | :---: |
| Read file `filename` | Read value of a key `filename` |
| Write `value` to file `filename` | Make key `filename` with value `value` |
| Make directory `dir` | Make section `dir` |
| Read file `filename` in directory `dir` | Read value of a key `filename` in section `dir` |
| Write `value` to file `filename` in directory `dir` | Make key `filename` in section `dir` with value `value` |


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
