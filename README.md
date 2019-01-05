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
Perhaps a controversial decision is to keep file names in memory, as volumes for hundreds of millions of lines can be significant.
For 100M short names like 18-23456-98765.png, you can reserve 1.5G, for 1B you get 15G.
This solution is not required for big data tasks, however, for web services, it make difficult DoS attacks by iterate over random names.

## Installation

### prerequisites

* gcc
* make
* uthash-dev
* cmph
* uuid-dev


### Required libraries.

## CLI

## Library

## Examples

### HTTP server

### Memcache interface


## Limitations


