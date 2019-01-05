# hugefile

Library for storing, scanning and fast access to collections of hundreds of millions of files

## Motivation

Working with big data involves managing collections of hundreds of millions of files. For me, it was a surprise how modern file systems are poorly adapted for such tasks.
Such work usually involves a lot of special shamanic techniques - for example, use the `rsync` instead of `cp`, never run `ls` in folders, copy collections using `dd` of the entire volume.
On the other hand, such collections are almost always immutable, and therefore do not require journaling throughout their existence.
Scanning such datasets takes an enormous amount of time that is spent not on reading content from a disk, but on opening and closing cycles of each file.
The solution is obvious - to merge millions of files into one large file and create an index for search.
This is exactly what this library does.
Unexpectedly for me, this approach turned out to be applicable in one more case - the management of websites with a large amount of static or "almost static" content. 
A practical example of such a site is rendered tiles for map services.
Worked example of web-server (faster than NGINX try_file) may be found in `examples/` folder.

## Design

KISS and only KISS.
Since the data is immutable, I used an index based on a perfect hash, the cmph library.
Hugefile operated entities are databases and "filelists".
Database is a folder with a several index and content files. All samples are deduplicated, no compression performed. 
Every filename associated with file content and a list of properties (metainforamtion) - key/value pairs.
Properties may store MIME-type, encoding, sample weigths, geodesic coordinates or any other useful info. 
For MNIST dataset metainformation contain training digits, for ImageNet - WordNet synset number. Metainfo data controlled by user, except system-related data (file mode, file owners, modification time etc).
ER is following
```
filename (1,n) -> (1) file content
filename (1) -> (0,n) file properties

```

"Filelist" used for import and export operations.
It's a plain TSV file, where 1st field is a unique file name in database. In simplest case filelist may be result of `find ./ -type f >filelist` shell command.
In more complex cases it allow place metainformation data in format `key:value` for TSV field.
If key is empty string, value used as real file name in filesystem,
So one can use string
```
14-234-3333.png\t:/home/me/storage/14/234/3333.png\tzoom:14\tx:234\ty:3333
```
for import file from /home/me/storage/14/234/3333.png into database where it will be indexed as 14-233-3333.png and get attributes zoom,x,y.

## Installation

### prerequisites

* gcc
* make
* uthash-dev
* libcmph-dev
* uuid-dev
* libssl-dev (1.1 seems not worked now, will be fixed soon)

### Install

`make ; make install`

## CLI

`hugefile -h`

## Library

## Examples

### HTTP server

### Memcache server


## Limitations

Perhaps a controversial decision is to keep file names in memory, as volumes for hundreds of millions of lines can be significant.
For 100M short names like 18-23456-98765.png you can reserve 1.5G, for 1B you get 15G. For long file names memory consumption may be worse.
While this decision is not required for big data tasks, but for web services it make difficult DoS attacks by iterate over random names.

