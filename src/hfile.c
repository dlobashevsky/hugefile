#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <utime.h>

#include <uuid/uuid.h>
#include <uthash.h>

#include "common.h"
#include "checksum.h"
#include "dict.h"
#include "utils.h"
#include "hfile.h"


#define HFILE_FLAG_DELETED	1
#define HFILE_FLAG_CORRUPTED	2

#if HFILE_USE_HUGEPAGES
#define HUGEPAGE	MAP_HUGETLB
#else
#define HUGEPAGE	0
#endif

//! dummy macros for point to on disk storage
#define PERSISTENT

//! common header for all files
PERSISTENT typedef struct hfile_header_t
{
  uint32_t magic;			//!< common magic
  uint32_t version;			//!< HFILE_VERSION
  uint64_t size;			//!< size of entire file include header
  uint32_t chunks;			//!< number of items
  uint64_t tm;				//!< creation/last modification time
  uint8_t uuid[UUID_SIZE];		//!< common uuid
  uint8_t checksum[CHECKSUM_SIZE];	//!< checksum of entire file without header
} __attribute__ ((packed)) hfile_header_t;


//! single file pointer, one item of index file
PERSISTENT typedef struct hfile_idx_item_t
{
  uint64_t content_offset;
  uint64_t name_offset;
//  uint32_t content_size;
//  uint32_t name_size;
} __attribute__ ((packed)) hfile_idx_item_t;


//! index file
typedef struct hfile_idx_t
{
  void* base;				//!< base mmaped ptr
  uint64_t mmapsize;			//!< size of memory mapped region
  int fd;				//!< index file descriptor
  hfile_header_t header;		//!< copy of header
  hfile_idx_item_t* data;		//!< pointer to hfile_idx_item_t
} hfile_idx_t;


//! chunk, header of single file in data file collection
PERSISTENT typedef struct hfile_chunk_t
{
  uint32_t magic2;
  uint32_t size;			//!< chunk total size include this header
  uint8_t flags;			//!< HFILE_FILE_FLAG_*
  uint8_t checksum[CHECKSUM_SIZE];	//!< hash of file content only
} __attribute__ ((packed)) hfile_chunk_t;


//! base data file
typedef struct hfile_content_t
{
  void* base;			//!< base mmaped ptr
  uint64_t mmapsize;		//!< size of memory mapped region
  int fd;			//!< file descriptor
  hfile_header_t header;
  hfile_chunk_t* files;
} hfile_content_t;


//! metainfo chunk
PERSISTENT typedef struct hfile_meta_t
{
//  uint16_t type;		//!< HFILE_META_TYPE_*
  uint16_t idx;			//!< index of metainfo name
  uint16_t size;		//!< chunk size
// uint8_t metadata[.size]
} __attribute__ ((packed)) hfile_meta_t;


//! individual file with name, attributes and metainfo
PERSISTENT typedef struct hfile_item_t
{
  uint32_t magic2;
  uint32_t size;			//!< chunk total size include this header
  uint8_t flags;			//!< HFILE_FILE_FLAG_*
  uint64_t content;			//!< pointer to content file
  uint32_t name_idx;			//!< name index in dict
  uint16_t meta_cnt;			//!< count of metainfo
//  uint32_t uid,gid,mode;
//  uint64_t atime,mtime;

/*
  hfile_meta_t[.meta_cnt];
*/
} __attribute__ ((packed)) hfile_item_t;


//! index file
typedef struct hfile_names_t
{
  void* base;				//!< base mmaped ptr
  uint64_t mmapsize;			//!< size of memory mapped region
  int fd;				//!< index file descriptor
  hfile_header_t header;		//!< copy of header
  hfile_item_t* items;			//!< pointer to first hfile_item
} hfile_names_t;


typedef struct hfile_t
{
  hfile_idx_t idx;
  hfile_names_t names;
  hfile_content_t content;
  dict_t* meta_dict;
  dict_t* names_dict;
} hfile_t;


//! fill system metainformation about file (name,mime,uid,gid,mode,atime,mtime)
static int fill_system_info(const char* name,const char* source,char** array);

//! update heared checksum and sizes
static int update_checksum(const char* file);

//! set file attributes based on metainfo/properties
static void export_attrs(const dict_t* meta_dict,const char* new_name,void* meta_ptr,size_t meta_cnt);

//! open and mmap file
static void* hfile_mmap_int(const char* name,uint64_t* size,int* pfd,hfile_header_t* header)
{
  struct stat st;

  if(stat(name,&st) || ! (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)))
  {
    log("invalid file type: %s",name);  return 0;
  }
  if(sizeof(hfile_header_t)>st.st_size)
  {
    log("file too short: %s",name);  return 0;
  }
  int fd=open(name,O_RDONLY);
  if(fd<0)
  {
    log("open file error: %s",name);
    return 0;
  }
  *size=st.st_size;

  void* rv=mmap(0,st.st_size,PROT_READ,MAP_PRIVATE | HFILE_USE_HUGEPAGES,fd,0);
  if(rv==(void*)-1) goto err;
  madvise(rv, st.st_size, MADV_DONTDUMP | MADV_RANDOM);

  memcpy(header,rv,sizeof(*header));

  if(header->magic!=MAGIC)
  {
    log("integrity of file %s is broken, bad magic",name);
    goto err;
  }

//fprintf(stderr,"%zu %zu %zu\n",header->size,*size,header->size+sizeof(hfile_header_t));

  if(header->size != *size)
  {
    log("integrity of file %s is broken, bad size",name);
    goto err;
  }

#if HFILE_CHECKSUM_ONOPEN
  {
    checksum_t* cs=checksum_init();
    uint8_t checksum[CHECKSUM_SIZE]; memset(checksum,0,CHECKSUM_SIZE);

    checksum_update(cs,rv+sizeof(*header),header->size-sizeof(*header));
    checksum_finalize(cs,checksum);

    if(memcmp(checksum,header->checksum,sizeof(header->checksum)))
    {
      log("integrity of file %s is broken, checksum mismatch",name);
      goto err;
    }
  }
#endif

  *pfd=fd;
  return rv;
err:
  close(fd);
  if(rv && rv!=(void*)-1) munmap(rv,st.st_size);
  *pfd=-1;
  *size=0;
  return 0;
}


typedef struct names_t
{
  char* mhash_name;
  char* nhash_name;
  char* idx_name;
  char* content_name;
  char* names_name;
} names_t;


static names_t* names_init(const char* folder)
{
  if(!folder)  return 0;
  names_t* rv=calloc(1,sizeof(*rv));

  asprintf(&rv->nhash_name,"%s/names.hash",folder);
  asprintf(&rv->mhash_name,"%s/meta.hash",folder);
  asprintf(&rv->idx_name,"%s/data.idx",folder);
  asprintf(&rv->content_name,"%s/data.content",folder);
  asprintf(&rv->names_name,"%s/names.content",folder);

  return rv;
}

static void names_free(struct names_t* n)
{
  if(!n)  return;
  free(n->mhash_name);
  free(n->nhash_name);
  free(n->idx_name);
  free(n->content_name);
  free(n->names_name);
  free(n);
}


hfile_t* hfile_open(const char* base)
{
  if(!base || !*base)  return 0;

  names_t* n=names_init(base);
  if(!n)  return 0;

  hfile_t* rv=0;
  dict_t* meta_dict=dict_load(n->mhash_name);
  dict_t* names_dict=dict_load(n->nhash_name);
  if(!meta_dict || !names_dict)
  {
    log("hash load error");
    names_free(n);
    goto leave;
  }

  rv=calloc(1,sizeof(*rv));
  rv->meta_dict=meta_dict;
  rv->names_dict=names_dict;

  rv->idx.base=hfile_mmap_int(n->idx_name,&rv->idx.mmapsize,&rv->idx.fd,&rv->idx.header);
  rv->names.base=hfile_mmap_int(n->names_name,&rv->names.mmapsize,&rv->names.fd,&rv->names.header);
  rv->content.base=hfile_mmap_int(n->content_name,&rv->content.mmapsize,&rv->content.fd,&rv->content.header);

  names_free(n);

  if(!rv->idx.base || !rv->names.base || !rv->content.base)
  {
    log("can not mmap data files, exiting");
    goto err;
  }
// simple consistency check here
  const char* muuid=dict_get_uuid(rv->meta_dict);
  const char* nuuid=dict_get_uuid(rv->names_dict);
  if(
      memcmp(muuid,nuuid,UUID_SIZE) ||
      memcmp(muuid,rv->names.header.uuid,UUID_SIZE) ||
      memcmp(muuid,rv->idx.header.uuid,UUID_SIZE) ||
      memcmp(muuid,rv->content.header.uuid,UUID_SIZE)
    )
  {
    log("uuids are differ");
    goto err;
  }
//  log("UUID is %s",rv->idx.header.uuid);

  rv->idx.data=rv->idx.base+sizeof(rv->idx.header);
  rv->names.items=rv->names.base+sizeof(rv->names.header);
  rv->content.files=rv->content.base+sizeof(rv->content.header);

leave:
  return rv;

err:
  hfile_free(rv);
  return 0;
}


void hfile_free(hfile_t* h)
{
  if(!h)  return;

  dict_free(h->meta_dict);
  dict_free(h->names_dict);

  if(h->content.base)  munmap(h->content.base,h->content.mmapsize);
  close(h->content.fd);

  if(h->idx.base) munmap(h->idx.base,h->idx.mmapsize);
  close(h->idx.fd);

  if(h->names.base) munmap(h->names.base,h->names.mmapsize);
  close(h->names.fd);

  free(h);
}



typedef struct hfile_int_entry_t
{
  char* name;
  UT_hash_handle hh;
} hfile_int_entry_t;

typedef struct hfile_int_entry2_t
{
  uint8_t checksum[CHECKSUM_SIZE];
  uint64_t off;
  uint32_t sz;

  UT_hash_handle hh;
} hfile_int_entry2_t;


static const char* meta_system[]=
{
// "_src"
  "_name",
//  "_mime",
  "_uid",
  "_gid",
  "_mode",
  "_atime",
  "_mtime"
};

#define meta_system_count	(sizeof(meta_system)/sizeof(*meta_system))

static dict_t* get_meta_dict(FILE *f,const char* uuid)
{
  size_t meta_count=0;
  size_t z=0;
  char *bf=0;

  hfile_int_entry_t *r,*root=0;

  while(getline(&bf,&z,f)>=0)
  {
    if(strchr("\t\n\r\f\b ",*bf))
      continue;
    utils_line_t* str=utils_line_parse(bf);
    if(!str)  continue;

    for(size_t i=0;i<str->metas;i++)
    {
      const char* meta=str->keys[i];
      size_t l=strlen(meta);
      r=0;
      HASH_FIND(hh,root,meta,l,r);
      if(r)  continue;
      r=calloc(1,sizeof(*r));
      r->name=strdup(meta);
      HASH_ADD_KEYPTR(hh,root,r->name,l,r);
      meta_count++;
    }
    utils_line_free(str);
  }

  free(bf);

  for(size_t i=0;i<sizeof(meta_system)/sizeof(*meta_system);i++)
  {
    size_t l=strlen(meta_system[i]);
    r=0;
    HASH_FIND(hh,root,(meta_system[i]),l,r);
    if(r)  continue;
    r=calloc(1,sizeof(*r));
    r->name=strdup(meta_system[i]);
    HASH_ADD_KEYPTR(hh,root,r->name,l,r);
    meta_count++;
  }

  char** list=calloc(meta_count,sizeof(char*));
  meta_count=0;
  for(r=root;r;r=r->hh.next)
    list[meta_count++]=r->name;

  dict_t* ret=dict_init_strings(uuid,list,meta_count);
  while(root)
  {
    r=root;
    HASH_DELETE(hh,root,r);
    free(r->name);
    free(r);
  }
  free(list);

  if(!ret)
    log("metainfo hash creation failed");

  return ret;
}


int hfile_build(const char* result,const char* input,uint32_t flags)
{
  if(!result || !input)  return -1;

  mkdir(result,0777);
  int ret=-1;
  time_t tm=time(0);

  names_t* n=names_init(result);
  if(!n) return -1;

  tic;tic;
  dict_t* names_dict=dict_init_tsv(0,input);

  if(!names_dict)
  {
    names_free(n);
    log("something wrong with <%s>",input);
    utils_time_pop();
    utils_time_pop();
    return ret;
  }

  if(dict_save(names_dict,n->nhash_name))
  {
    utils_time_pop();
    utils_time_pop();
    names_free(n);
    log("can not save hash in <%s>",n->nhash_name);
    dict_free(names_dict);
    return ret;
  }

  size_t total_items=dict_get_size(names_dict);
  log("names hash created <%s>, total items %zu, time taken %s",n->nhash_name,total_items,toc);

  FILE *f=fopen(input,"r");

  if(!f)
  {
    log("cant open file <%s>",input);
    utils_time_pop();
    return ret;
  }

  tic;
  dict_t* meta_dict=get_meta_dict(f,dict_get_uuid(names_dict));
  if(!meta_dict)
  {
    log("cant generate perfect hash for metainfo/properties");
    utils_time_pop();
    utils_time_pop();
    return ret;
  }

  dict_save(meta_dict,n->mhash_name);
  size_t meta_count=dict_get_size(meta_dict);
  log("props/meta names hash created <%s>, total items %zu, time taken %s",n->mhash_name,meta_count,toc);

  tic;
  rewind(f);

  size_t idx_size=sizeof(hfile_header_t)+total_items*sizeof(hfile_idx_item_t);
  void* idx_mem=MAP_FAILED;
  int idx_fd=open(n->idx_name,O_RDWR | O_CREAT,0644);

  if(idx_fd<0 || posix_fallocate(idx_fd,0,idx_size) || (idx_mem=mmap(0,idx_size,PROT_READ | PROT_WRITE,MAP_SHARED,idx_fd,0)) == MAP_FAILED)
  {
    log("file creation error %s: %s",n->idx_name,strerror(errno));
    close(idx_fd);
    goto err;
  }

  hfile_header_t header_content,header_names;
  memset(&header_content,0,sizeof(header_content));
  memset(&header_names,0,sizeof(header_names));
  hfile_header_t* idx_header=idx_mem;
  hfile_idx_item_t* idx=idx_mem+sizeof(hfile_header_t);

  memset(idx,0xff,total_items*sizeof(hfile_idx_item_t));

  header_content.magic=header_names.magic=idx_header->magic=MAGIC;
  header_content.version=header_names.version=idx_header->version=HFILE_VERSION;
  header_content.tm=header_names.tm=idx_header->tm=tm;
  memcpy(idx_header->uuid,dict_get_uuid(meta_dict),sizeof(idx_header->uuid));
  memcpy(header_names.uuid,dict_get_uuid(meta_dict),sizeof(header_names.uuid));
  memcpy(header_content.uuid,dict_get_uuid(meta_dict),sizeof(header_content.uuid));
  idx_header->size=idx_size;
  idx_header->chunks=total_items;

  FILE* fname=fopen(n->names_name,"w");
  FILE* fcontent=fopen(n->content_name,"w");
  if(!fname || !fcontent)
  {
    log("file creation error %s",strerror(errno));
    fclose(fname);
    fclose(fcontent);
    goto err2;
  }

  fwrite(&header_names,sizeof(header_names),1,fname);
  fwrite(&header_content,sizeof(header_content),1,fcontent);

  hfile_int_entry2_t file,*r2,*root2=0;
  size_t name_count=0;
  size_t content_count=0;

// calculate size and create memory mapped files

  size_t z=0;
  char *bf=0;

  while(getline(&bf,&z,f)>=0)
  {
    if(strchr("\t\n\r\f\b ",*bf) || !*bf)
      continue;
    utils_line_t* str=utils_line_parse(bf);
    if(!str)  continue;

    void* fptr=0;
    int mfd=-1;
    file.sz=(uint32_t)-1;

    {
      memset(&file,0,sizeof(file));
      struct stat st;
      if(!stat(str->source,&st))
        mfd=open(str->source,O_RDONLY);
      if(mfd<0)
      {
        log("skipping non existent file %s",str->source);
        utils_line_free(str);
        continue;
      }

      file.sz=st.st_size;
      file.off= -1LL;

      if(!st.st_size)
      {
        log("file <%s> have zero size",str->source);
        file.sz=0;
        fptr=0;
      }
      else if((fptr=mmap(0,st.st_size,PROT_READ,MAP_PRIVATE,mfd,0)) == MAP_FAILED)
      {
        log("can not mmap file <%s>",str->source);
        close(mfd);
        mfd=-1;
        file.sz=(uint32_t)-1;
        fptr=0;
      }
    }

    if(file.sz==(uint32_t)-1)
    {
      utils_line_free(str);
      continue;
    }

    {
      checksum_t* cs=checksum_init();
      checksum_update(cs,fptr,file.sz);
      checksum_finalize(cs,file.checksum);
    }

    uint64_t content_off=ftell(fcontent);
    uint64_t name_off=ftell(fname);
    uint32_t name_idx=dict_get_str(names_dict,str->name);
    if(name_idx==DICT_NOT_FOUND)  crash("something unusual");

    r2=0;
    HASH_FIND(hh,root2,file.checksum,sizeof(file.checksum),r2);
    if(!r2)			// add content
    {
      r2=md_new(r2);
      r2->off=content_off;
      r2->sz=file.sz;
      memcpy(r2->checksum,file.checksum,sizeof(r2->checksum));
      HASH_ADD_KEYPTR(hh,root2,r2->checksum,sizeof(r2->checksum),r2);
 
      hfile_chunk_t chunk;
      chunk.magic2=MAGIC2;
      chunk.size=r2->sz;
      chunk.flags=0;
      memcpy(chunk.checksum,file.checksum,sizeof(chunk.checksum));
      fwrite(&chunk,sizeof(chunk),1,fcontent);
      fwrite(fptr,r2->sz,1,fcontent);
      content_count++;
    }

    if(file.sz)  munmap(fptr,file.sz);
    close(mfd);

    idx[name_idx].content_offset=r2->off;
    idx[name_idx].name_offset=name_off;

// populate system info
    char* sysinfo[meta_system_count]={0,};

    fill_system_info(str->name,str->source,sysinfo);

    hfile_item_t chunk;
    memset(&chunk,0,sizeof(chunk));
    chunk.magic2=MAGIC2;
    chunk.size=0;
    chunk.flags=0;
    chunk.content=r2->off;
    chunk.name_idx=name_idx;
    chunk.meta_cnt=str->metas+meta_system_count;

    for(size_t i=0;i<str->metas;i++)
      chunk.size+=sizeof(hfile_meta_t)+strlen(str->vals[i])+1;

// add system metainfo

    for(size_t i=0;i<meta_system_count;i++)
      chunk.size+=sizeof(hfile_meta_t)+strlen(sysinfo[i])+1;

    fwrite(&chunk,sizeof(chunk),1,fname);

    hfile_meta_t meta;
    for(size_t i=0;i<meta_system_count;i++)
    {
      meta.idx=dict_get_str(meta_dict,meta_system[i]);
      if(meta.idx==DICT_NOT_FOUND)  abort();

      meta.size=strlen(sysinfo[i])+1;
      fwrite(&meta,sizeof(meta),1,fname);
      fwrite(sysinfo[i],meta.size,1,fname);
      free(sysinfo[i]);
    }

    for(size_t i=0;i<str->metas;i++)
    {
      meta.idx=dict_get_str(meta_dict,str->keys[i]);
      if(meta.idx==DICT_NOT_FOUND)  abort();
      meta.size=strlen(str->vals[i])+1;
      fwrite(&meta,sizeof(meta),1,fname);
      fwrite(str->vals[i],meta.size,1,fname);
    }
    name_count++;
    utils_line_free(str);
  }

  header_names.chunks=name_count;
  header_content.chunks=content_count;

  rewind(fname);
  rewind(fcontent);
  fwrite(&header_names,sizeof(header_names),1,fname);
  fwrite(&header_content,sizeof(header_content),1,fcontent);

  fclose(fname);
  fclose(fcontent);


//*************** update checksums
  {
    checksum_t* cs=checksum_init();
    uint8_t checksum[CHECKSUM_SIZE]; 
    memset(checksum,0,CHECKSUM_SIZE);
    checksum_update(cs,(void*)idx,total_items*sizeof(hfile_idx_item_t));
    checksum_finalize(cs,checksum);
    memcpy(&idx_header->checksum,checksum,sizeof(idx_header->checksum));
    msync(idx_mem,idx_size,MS_SYNC);
  }

  update_checksum(n->content_name);
  update_checksum(n->names_name);

  ret=0;

err2:

  munmap(idx_mem,idx_size);
  close(idx_fd);

err:

  while(root2)
  {
    r2=root2;
    HASH_DELETE(hh,root2,r2);
    free(r2);
  }
  log("hfile archive creation %s, time taken %s",ret ? "failed" : "successfull", toc);

  dict_free(names_dict);
  dict_free(meta_dict);

  names_free(n);
  free(bf);
  fclose(f);
  return ret;
}

ssize_t hfile_maxlen(const hfile_t* hf)
{
  return hf ? dict_get_max(hf->names_dict) : -1;
}


int hfile_extract(hfile_t* h,const char* prefix,const char* regex,mode_t mode)
{
  if(!h)  return -1;
  if(!mode) mode=0777;
  if(!prefix || !*prefix)  prefix="./";

  FILE *list=0;
  {
    char* bf=0;
    asprintf(&bf,"%s/.source.list",prefix);
    utils_mkpath(bf,0777);
    list=fopen(bf,"w");
    if(!list)
    {
      log("Can not create output <%s>",bf);
      free(bf);
      return -1;
    }
    free(bf);
  }
  setlinebuf(list);

//  if(!list)  perror("creating source list");

  hfile_idx_item_t* data=h->idx.data;

  for(size_t i=0;i<h->idx.header.chunks;i++)
  {
    uint64_t name_offset=data[i].name_offset;
    uint64_t content_offset=data[i].content_offset;

    if(name_offset==(uint64_t)(-1LL) || content_offset==(uint64_t)(-1LL))  continue;
    hfile_item_t* name=h->names.base+name_offset;
    if(name->flags)  continue;
    const char* filename=dict_get_byidx(h->names_dict,name->name_idx);
    uint32_t* magic2=(void*)name;

    if(!filename || *magic2!=MAGIC2 || i!=name->name_idx)
    {
      log("integrity broken for %jd offset (%zd:%d index)",name_offset,i,name->name_idx);
      continue;
    }

    if(regex && *regex && fnmatch(regex,filename,FNM_EXTMATCH)==FNM_NOMATCH)  continue;

    hfile_chunk_t* chunk=h->content.base+content_offset;
    magic2=(void*)chunk;
    if(*magic2!=MAGIC2)
    {
      log("integrity broken for content of file <%s> (%zd:%d index)",filename,i,name->name_idx);
      continue;
    }

    char* new_name=0;
    asprintf(&new_name,"%s/%s",prefix,filename);
    if(utils_mkpath(new_name,mode))
    {
      log("Error creating path for %s",new_name);
      free(new_name);
      continue;
    }

    FILE* f=fopen(new_name,"wb");
    if(!f)
    {
      log("Error creating file %s",new_name);
      free(new_name);
      continue;
    }

    void* content=chunk+1;
    size_t content_size=chunk->size;

    if(content_size && fwrite(content,1,content_size,f)!=content_size)
    {
      log("Can not write content for file %s",new_name);
      fclose(f);
      unlink(new_name);
      free(new_name);
      continue;
    }
    fclose(f);

    void* meta_ptr=name+1;
    export_attrs(h->meta_dict,new_name,meta_ptr,name->meta_cnt);

    fprintf(list,"%s\t:%s",filename,new_name);
    for(size_t m=0;m<name->meta_cnt;m++)
    {
      hfile_meta_t* meta=meta_ptr;
      void* meta_val=meta+1;
      const char* meta_name=dict_get_byidx(h->meta_dict,meta->idx);
      meta_ptr=meta_val+meta->size;
      if(!meta_name || *meta_name=='_')  continue;
      fprintf(list,"\t%s:%s",meta_name,(char*)meta_val);
    }
    fprintf(list,"\n");
    free(new_name);
  }

  fclose(list);
  return 0;
}


int hfile_integrity_check(const char* base)
{
  hfile_t* h=hfile_open(base);
  if(!h)
  {
    log("integrity check failed");
  }
// TODO
  hfile_free(h);
  return !h;
}

//! print base stat
int hfile_stat(const hfile_t* h)
{
  if(!h)
  {
    log("no input\n");
    return -1;
  }

  printf("UUID: %s\n",dict_get_uuid(h->names_dict));
  printf("Resident size: %lu\n",dict_get_bytes(h->names_dict)+dict_get_bytes(h->meta_dict));
  printf("Disk size: %lu\n",dict_get_bytes(h->names_dict)+dict_get_bytes(h->meta_dict)+h->idx.header.size+h->names.header.size+h->content.header.size);
  printf("Total names: %u\n",dict_get_size(h->names_dict));
  printf("Valid names: %u\n",h->names.header.chunks);
  printf("Unique files: %u\n",h->content.header.chunks);
  printf("Distinct properties: %u\n",dict_get_size(h->meta_dict));
  printf("\n");

  return 0;
}


//accessors:

ssize_t hfile_name_count(const hfile_t* h)
{
  return h ? dict_get_size(h->names_dict) : -1;
}

ssize_t hfile_file_count(const hfile_t* h)
{
  return h ? h->content.header.chunks : -1;
}

const char* hfile_name_by_idx(const hfile_t* h,size_t idx)
{
  return h ? dict_get_byidx(h->names_dict,idx) : 0;
}

ssize_t hfile_idx_by_name(const hfile_t* h,const char* name)
{
  if(!h || !name || !*name)  return -1;
  size_t rv=dict_get_str(h->names_dict,name);
  return rv==DICT_NOT_FOUND ? rv : -1;;
}


const void* hfile_file_by_name(const hfile_t* h,const char* name,size_t* size)
{
  if(!h || !name || !*name)  return 0;
  ssize_t n=dict_get_str(h->names_dict,name);
  if(n<0 || n==DICT_NOT_FOUND)  return 0;

  uint64_t off=h->idx.data[n].content_offset;
  if(off==HFILE_NOT_FOUND)  return 0;

  void* ptr=h->content.base+off;
  hfile_chunk_t* chunk=ptr;

  *size=chunk->size-sizeof(hfile_chunk_t);
  return chunk+1;
}


static int fill_system_info(const char* name,const char* file,char** array)
{
  if(!file || !*file || !array)  return -1;

  struct stat st;

  if(stat(file,&st))  return -1;

  array[0]=strdup(name);
  asprintf(array+1,"%u",st.st_uid);
  asprintf(array+2,"%u",st.st_gid);
  asprintf(array+3,"%u",st.st_mode&0777);
  asprintf(array+4,"%lu",st.st_atime);
  asprintf(array+5,"%lu",st.st_mtime);
  return 0;
}

static int update_checksum(const char* file)
{
  struct stat st;
  if(stat(file,&st) || st.st_size<sizeof(hfile_header_t))
  {
    log("stat error on %s : %s",file,strerror(errno));
    return -1;
  }

  void* mem=MAP_FAILED;
  int fd=open(file,O_RDWR);
  if(fd<0 || (mem=mmap(0,st.st_size,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0)) == MAP_FAILED)
  {
    log("mmap error on %s : %s",file,strerror(errno));
    close(fd);
    return -1;
  }
  hfile_header_t* head=mem;
  void* chunks=mem+sizeof(*head);
  head->size=st.st_size;

  checksum_t* cs=checksum_init();
  uint8_t checksum[CHECKSUM_SIZE]; 
  memset(checksum,0,CHECKSUM_SIZE);
  checksum_update(cs,chunks,head->size-sizeof(hfile_header_t));
  checksum_finalize(cs,checksum);
  memcpy(&head->checksum,checksum,sizeof(head->checksum));
  msync(mem,st.st_size,MS_SYNC);
  munmap(mem,st.st_size);
  close(fd);
  return 0;
}


static void export_attrs(const dict_t* meta_dict,const char* new_name,void* meta_ptr,size_t meta_cnt)
{
  mode_t mode=0;
  struct utimbuf utm={0,0};
  uid_t uid=getuid();
  uid_t gid=getgid();

  for(size_t i=0;i<meta_cnt;i++)
  {
    hfile_meta_t* meta=meta_ptr;
    void* meta_val=meta+1;
    const char* meta_name=dict_get_byidx(meta_dict,meta->idx);
    meta_ptr=meta_val+meta->size;

    if(!meta_name)
    {
      log("Invalid property %hd for %s",meta->idx,new_name);
      continue;
    }

    if(!strcmp(meta_name,"_uid"))
      uid=atol(meta_val);
    else if(!strcmp(meta_name,"_gid"))
      gid=atol(meta_val);
    else if(!strcmp(meta_name,"_mode"))
      mode=atol(meta_val);
    else if(!strcmp(meta_name,"_mtime"))
      utm.modtime=atol(meta_val);
    else if(!strcmp(meta_name,"_atime"))
      utm.actime=atol(meta_val);
  }

  chmod(new_name,0777);
  utime(new_name,&utm);
  chown(new_name,uid,gid);
  chmod(new_name,mode);
}


void hfile_ret_free(hfile_ret_t* r)
{
  if(!r)  return;
  free(r->keys);
  free(r->vals);
  free(r);
}

static hfile_ret_t* hfile_get_int(const hfile_t* h,size_t n);

hfile_ret_t* hfile_get(const hfile_t* h,const char* name)
{
  if(!h || !name || !*name)  return 0;
  ssize_t n=dict_get_str(h->names_dict,name);
  if(n<0 || n==DICT_NOT_FOUND)  return 0;
  return hfile_get_int(h,n);
}

static hfile_ret_t* hfile_get_int(const hfile_t* h,size_t n)
{
  if(!h || n>=h->idx.header.chunks)  return 0;

  uint64_t off=h->idx.data[n].content_offset;
  if(off==HFILE_NOT_FOUND)  return 0;
  uint64_t off_name=h->idx.data[n].name_offset;
  if(off_name==HFILE_NOT_FOUND)  return 0;

  void* ptr=h->content.base+off;
  hfile_chunk_t* chunk=ptr;

  ptr=h->names.base+off_name;
  hfile_item_t* item=ptr;
  if(item->magic2!=MAGIC2 || chunk->magic2!=MAGIC2)  return 0;

  hfile_ret_t* ret=calloc(1,sizeof(*ret));
  ret->name=dict_get_byidx(h->names_dict,n);

  ret->size=chunk->size;
  ret->content=chunk+1;
  ret->checksum=chunk->checksum;

  ret->metas=item->meta_cnt;
  ret->keys=calloc(ret->metas,sizeof(char*));
  ret->vals=calloc(ret->metas,sizeof(char*));

  void* meta=item+1;

  for(size_t i=0;i<ret->metas;i++)
  {
    hfile_meta_t* m=meta;
    meta=m+1;
    ret->keys[i]=dict_get_byidx(h->meta_dict,m->idx) ?: "--";
    ret->vals[i]=meta;
    meta+=m->size;
  }

  return ret;
}


hfile_it_t* hfile_it_init(const hfile_t* hf)
{
  if(!hf)  return 0;
  hfile_it_t* rv=malloc(sizeof(*rv));
  rv->cur=0;
  rv->hf=hf;
  return rv;
}


void hfile_it_free(hfile_it_t* it)
{
  if(!it)  return;
  free(it);
}

int hfile_it_rewind(hfile_it_t* it)
{
  if(!it)  return 1;
  it->cur=0;
  return 0;
}


hfile_ret_t* hfile_it_get(hfile_it_t* h)
{
  if(!h)  return 0;
  while(h->cur>=h->hf->idx.header.chunks)
  {
    hfile_ret_t* ret=hfile_get_int(h->hf,h->cur++);
    if(ret)  return ret;
  }
  return 0;
}


hfile_ret_t* hfile_get_rand_name(const hfile_t* h)
{
  if(!h)  return 0;
  for(;;)
  {
    hfile_ret_t* ret=hfile_get_int(h,rand()%h->idx.header.chunks);
    if(ret)  return ret;
  }
}


static void dump_header(const hfile_header_t* h,FILE *f)
{
  char chksum_buf[2*CHECKSUM_SIZE+1]={0,};

  fprintf(f,"Header:\n");

  fprintf(f,"\tmagic:\t%08x\n",h->magic);
  fprintf(f,"\tversion:\t%08x\n",h->version);
  fprintf(f,"\tsize:\t%lu\n",h->size);
  fprintf(f,"\tchunks:\t%u\n",h->chunks);
  fprintf(f,"\tcreation time:\t%s",ctime(&h->tm));
  fprintf(f,"\tUUID:\t%s\n",h->uuid);

  utils_bin2hex(chksum_buf,h->checksum,sizeof(h->checksum));
  fprintf(f,"\tchecksum:\t%s\n",chksum_buf);
}

int hfile_dump(const hfile_t* hf,const char* out)
{
  if(!hf || !out || !*out)  return -1;
  mkdir(out,0777);
  struct stat st;
  if(stat(out,&st) || !(S_ISDIR(st.st_mode)))  return 1;
  char *bf=0;
  FILE *f=0;

// dump index
  asprintf(&bf,"%s/data.idx.dump",out);
  f=fopen(bf,"w");
  if(!f)
    log("can not create file <%s>",bf);
  else
  {
    dump_header(&hf->idx.header,f);
    for(size_t i=0;i<hf->idx.header.chunks;i++)
      fprintf(f,"%zd\t%ld\t%ld\n",i,hf->idx.data[i].name_offset,hf->idx.data[i].content_offset);
    fclose(f);
  }
  free(bf);bf=0;

// dump names hash
  asprintf(&bf,"%s/names.hash.dump",out);
  f=fopen(bf,"w");
  if(!f)
    log("can not create file <%s>",bf);
  else
  {
    dict_dump(hf->names_dict,f);
    fclose(f);
  }
  free(bf);bf=0;

// dump meta hash
  asprintf(&bf,"%s/meta.hash.dump",out);
  f=fopen(bf,"w");
  if(!f)
    log("can not create file <%s>",bf);
  else
  {
    dict_dump(hf->meta_dict,f);
    fclose(f);
  }
  free(bf);bf=0;

// dump names info
  asprintf(&bf,"%s/names.content.dump",out);
  f=fopen(bf,"w");
  if(!f)
    log("can not create file <%s>",bf);
  else
  {
    dump_header(&hf->names.header,f);
    hfile_item_t* item=hf->names.items;
    for(size_t i=0;i<hf->names.header.chunks;i++)
    {
      fprintf(f,"%zd\t[%zu]\t%08x\t%d\t%02hhx\t%ld\t%d\t%hu\t:",i,((void*)item)-(void*)hf->names.items,
                item->magic2,item->size,item->flags,item->content,item->name_idx,item->meta_cnt);

      hfile_meta_t* meta=(void*)(item+1);
      for(size_t j=0;j<item->meta_cnt;j++)
      {
        fprintf(f,"\t%hu[%hu]\t%s",meta->idx,meta->size,(char*)(meta+1));
        meta=((void*)(meta+1))+meta->size;
      }
      fprintf(f,"\n");
      item=((void*)(item+1))+item->size;
    }

    fclose(f);
  }
  free(bf);bf=0;


// dump content
  asprintf(&bf,"%s/data.content.dump",out);
  f=fopen(bf,"w");
  if(!f)
    log("can not create file <%s>",bf);
  else
  {
    dump_header(&hf->content.header,f);

    hfile_chunk_t* item=hf->content.files;
    for(size_t i=0;i<hf->content.header.chunks;i++)
    {
      char chksum_buf[2*CHECKSUM_SIZE+1]={0,};
      char chksum_buf2[2*CHECKSUM_SIZE+1]={0,};
      utils_bin2hex(chksum_buf,item->checksum,sizeof(item->checksum));

      {
        checksum_t* cs=checksum_init();
        uint8_t checksum[CHECKSUM_SIZE]; 
        memset(checksum,0,CHECKSUM_SIZE);
        checksum_update(cs,(void*)(item+1),item->size);
        checksum_finalize(cs,checksum);
        utils_bin2hex(chksum_buf2,checksum,sizeof(checksum));
      }

      fprintf(f,"%zd\t[%zu]\t%08x\t%d\t%02hhx\t%s\t%s\n",i,(void*)item-(void*)hf->content.files,
                item->magic2,item->size,item->flags,chksum_buf,chksum_buf2);


      item=((void*)(item+1))+item->size;
    }
    fclose(f);
  }
  free(bf);bf=0;

  return 0;
}

int hfile_genlist(const hfile_t* h,const char* out)
{
  if(!h || !out || !*out)  return -1;

  FILE *list=fopen(out,"w");
  if(!list)
    crash("can not create filelist");

  hfile_idx_item_t* data=h->idx.data;

  for(size_t i=0;i<h->idx.header.chunks;i++)
  {
    uint64_t name_offset=data[i].name_offset;
    uint64_t content_offset=data[i].content_offset;

    if(name_offset==(uint64_t)(-1LL) || content_offset==(uint64_t)(-1LL))  continue;
    hfile_item_t* name=h->names.base+name_offset;
    if(name->flags)  continue;
    const char* filename=dict_get_byidx(h->names_dict,name->name_idx);
    uint32_t* magic2=(void*)name;

    if(!filename || *magic2!=MAGIC2 || i!=name->name_idx)
    {
      log("integrity broken for %jd offset (%zd:%d index)",name_offset,i,name->name_idx);
      continue;
    }

    void* meta_ptr=name+1;

    fprintf(list,"%s",filename);
    for(size_t m=0;m<name->meta_cnt;m++)
    {
      hfile_meta_t* meta=meta_ptr;
      void* meta_val=meta+1;
      const char* meta_name=dict_get_byidx(h->meta_dict,meta->idx);
      meta_ptr=meta_val+meta->size;
      if(!meta_name /*|| *meta_name=='_'*/)  continue;
      fprintf(list,"\t%s:%s",meta_name,(char*)meta_val);
    }
    fprintf(list,"\n");
  }

  fclose(list);
  return 0;
}

