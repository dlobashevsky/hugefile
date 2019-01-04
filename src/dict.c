#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include <cmph.h>
#include <uuid/uuid.h>

#include <uthash.h>

#include "common.h"
#include "dict.h"


//! default algorithm
#define DICT_ALGO	CMPH_BDZ

static const uint32_t magic=MAGIC;

typedef struct dict_t
{
  uint8_t uuid[UUID_SIZE];		//!< common uuid
  uint8_t cmph;				//!< flag, if 1 then cmph used else simple uthash
  void* hash;				//!< cmph hash
  uint32_t max;				//!< maximal key length
  uint32_t sz;				//!< count of items
  uint64_t msz;				//!< memory allocated for strings
  uint8_t* mem;				//!< storage
  uint32_t* data;			//!< indices for strings
} dict_t;


dict_t* dict_init_strings(const char* uuid,char** data,size_t sz)
{
  if(!data || !sz)
    return 0;

  dict_t* rv=calloc(1,sizeof(*rv));

  if(!uuid)
  {
    uuid_t u;
    uuid_generate_random(u);
    uuid_unparse_lower(u,rv->uuid);
  }
  else
    memcpy(rv->uuid,uuid,sizeof(rv->uuid));

  rv->sz=sz;
  size_t bmem=sz;
  for(size_t i=0;i<sz;i++)
  {
    size_t l=strlen(data[i]);
    bmem+=l;
  }

  rv->data=malloc(sizeof(uint32_t)*sz);
  memset(rv->data,0xff,sz*sizeof(uint32_t));

  cmph_io_adapter_t *source=cmph_io_vector_adapter(data,sz);
  cmph_config_t *config = cmph_config_new(source);
  cmph_config_set_algo(config,DICT_ALGO);

  rv->hash = cmph_new(config);
  cmph_config_destroy(config);
  cmph_io_vector_adapter_destroy(source);
  if(!rv->hash)
  {
    log("hash generation failed, check uniquess");
    free(rv->data);
    free(rv);
    return 0;
  }

  rv->mem=malloc(bmem);
  rv->msz=bmem;
  uint32_t* t=malloc(sizeof(uint32_t)*sz);
  uint32_t c=0;
  for(size_t i=0;i<sz;i++)
  {
    size_t u=strlen(data[i]);
    t[i]=c;
    memcpy(rv->mem+t[i],data[i],++u);
    c+=u;
  }

  while(sz--)
  {
    size_t l=strlen(data[sz]);
    if(l>rv->max) rv->max=l;
    ssize_t q=cmph_search(rv->hash,data[sz],l);
    if(q<0)
    {
      log("hash creation internal error");
      crash("integrity broken");
    }
    if(rv->data[q]!=0xffffffff)
    {
      log("hash creation internal error");
      crash("integrity broken");
    }
    rv->data[q]=t[sz];
  }
  free(t);

  return rv;
}


void dict_free(dict_t* ph)
{
  if(!ph)
    return;
  cmph_destroy(ph->hash);
  free(ph->data);
  free(ph->mem);
  free(ph);
}


static uint32_t dict_tsvadapter_count(FILE *f)
{
  uint32_t count = 0;

  rewind(f);

  size_t z=0;
  char *bf=0;

  while(getline(&bf,&z,f)>=0)
  {
    if(!*bf || strchr(" \t\n\r\f",*bf))  continue;
    count++;
  }

  rewind(f);
  free(bf);
  return count;
}

static int dict_tsvadapter_key_read(void *data, char **key, uint32_t *keylen)
{
  FILE *f=data;
  *key = 0;
  *keylen = 0;

  size_t z=0;
  char *bf=0;

  for(;;)
  {
    if(getline(&bf,&z,f)<0)  return -1;
    if(*bf && !strchr(" \t\n\r\f",*bf))  break;
  }

  bf=strsep(&bf,"\t\n\r\f");
  *key=bf;
  *keylen=strlen(bf);

  return (int)(*keylen);
}


static void dict_tsvadapter_destroy(cmph_io_adapter_t* key_source)
{
  free(key_source);
}

static void dict_tsvadapter_dispose(void *data, char *key, uint32_t keylen)
{
  free(key);
}

static void dict_tsvadapter_rewind(void *data)
{
  rewind(data);
}


cmph_io_adapter_t *dict_tsvadapter(FILE* f)
{
  cmph_io_adapter_t* key_source = (cmph_io_adapter_t *)malloc(sizeof(cmph_io_adapter_t));
  key_source->data = f;
  key_source->nkeys = dict_tsvadapter_count(f);
  key_source->read = dict_tsvadapter_key_read;
  key_source->dispose = dict_tsvadapter_dispose;
  key_source->rewind = dict_tsvadapter_rewind;
  return key_source;
}


dict_t* dict_init_tsv(const char* uuid,const char* file)
{
  if(!file || !*file)
    return 0;

  FILE* f=fopen(file,"r");
  if(!f)  return 0;


  cmph_io_adapter_t *source = dict_tsvadapter(f);
  cmph_config_t *config = cmph_config_new(source);
  cmph_config_set_algo(config, DICT_ALGO);
  cmph_t* hash = cmph_new(config);
  cmph_config_destroy(config);
  dict_tsvadapter_destroy(source);

  if(!hash)
  {
    fclose(f);
    log("creating MPH fro <%s> failed, check uniqueness",file);
    return 0;
  }

  dict_t* rv=calloc(1,sizeof(*rv));

  if(!uuid)
  {
    uuid_t u;
    uuid_generate_random(u);
    uuid_unparse_lower(u,rv->uuid);
  }
  else
    memcpy(rv->uuid,uuid,sizeof(rv->uuid));

  rv->hash=hash;

//  rv->msz=rv->sz=dict_tsvadapter_count(f);

  char* bf=0;
  size_t z=0;

  rewind(f);

  while(getline(&bf,&z,f)>=0)
  {
    if(!*bf || strchr(" \t\n\r\f",*bf))  continue;
    bf=strsep(&bf,"\t\n\r\f");
    rv->msz+=strlen(bf);
    rv->sz++;
  }
  rv->msz+=rv->sz;
  rv->mem=malloc(rv->msz);
  rv->data=malloc(sizeof(uint32_t)*rv->sz);
  memset(rv->data,0xff,rv->sz*sizeof(uint32_t));

  size_t cnt=0;
  size_t off=0;

  uint32_t* t=malloc(sizeof(uint32_t)*rv->sz);
  rewind(f);

  while(getline(&bf,&z,f)>=0)
  {
    if(!*bf || strchr(" \t\n\r\f",*bf))  continue;
    bf=strsep(&bf,"\t\n\r\f");
    size_t u=strlen(bf);
    t[cnt]=off;
    memcpy(rv->mem+t[cnt],bf,++u);

    off+=u;
    cnt++;
  }

  rewind(f);
  cnt=0;

  while(getline(&bf,&z,f)>=0)
  {
    if(!*bf || strchr(" \t\n\r\f",*bf))  continue;
    bf=strsep(&bf,"\t\n\r\f");
    size_t u=strlen(bf);

    ssize_t q=cmph_search(rv->hash,bf,u);

    if(q<0)
    {
      log("hash creation internal error");
      crash("integrity broken");
    }
    if(rv->data[q]!=0xffffffff)
    {
      log("hash creation internal error");
      crash("integrity broken");
    }
    if(u>rv->max) rv->max=u;
    rv->data[q]=t[cnt++];
  }

  free(t);
  free(bf);

  fclose(f);
  return rv;
}


ssize_t dict_get_max(const dict_t* d)
{
  return d ? d->max : -1;
}

uint32_t dict_get_size(const dict_t* ph)
{
  return ph ? ph->sz : 0;
}

uint32_t dict_get(const dict_t* ph,const void* key,size_t keylen)
{
  if(!ph || !key || !keylen)
    return -1;
  ssize_t rv=cmph_search(ph->hash,key,keylen);
  return (rv<0 || rv>=ph->sz) ? DICT_NOT_FOUND : (!memcmp(ph->mem+ph->data[rv],key,keylen) ? rv : -1);
}

uint32_t dict_get_str(const dict_t* ph,const char* key)
{
  if(!ph || !key)
    return -1;
  size_t l=strlen(key);
  ssize_t rv=cmph_search(ph->hash,key,l);
  return (rv<0 || rv>=ph->sz) ? DICT_NOT_FOUND : (!memcmp(ph->mem+ph->data[rv],key,l+1) ? rv : -1);
}

const char* dict_get_byidx(const dict_t* ph,size_t idx)
{
  if(!ph || idx>=ph->sz)  return 0;
  return ph->mem+ph->data[idx];
}

uint64_t dict_get_bytes(const dict_t* ph)
{
  if(!ph)
    return 0;
  return cmph_packed_size(ph->hash)+sizeof(*ph)+sizeof(ph->data[0])*ph->sz+ph->msz;
}

const char* dict_get_uuid(const dict_t* ph)
{
  return ph ? ph->uuid : 0;
}


int dict_save(const dict_t* data,const char* fn)
{
  FILE *f=fopen(fn,"wb");
  int rv=dict_save_file(data,f);
  fclose(f);
  return rv;
}

dict_t* dict_load(const char* fn)
{
  FILE *f=fopen(fn,"rb");
  if(!f)
    return 0;
  dict_t* rv=dict_load_file(f);
  fclose(f);
  return rv;
}

int dict_save_file(const dict_t* ph,FILE* f)
{
  if(!ph || !f)
    return 1;

  if(fwrite(&magic,1,sizeof(magic),f)!=sizeof(magic)) return 1;
  if(fwrite(ph->uuid,1,sizeof(ph->uuid),f)!=sizeof(ph->uuid)) return 1;
  if(fwrite(&ph->sz,1,sizeof(ph->sz),f)!=sizeof(ph->sz)) return 1;
  if(fwrite(&ph->msz,1,sizeof(ph->msz),f)!=sizeof(ph->msz)) return 1;
  if(fwrite(&ph->max,1,sizeof(ph->max),f)!=sizeof(ph->max)) return 1;
  if(fwrite(ph->data,1,(ph->sz*sizeof(uint32_t)),f)!=(ph->sz*sizeof(uint32_t))) return 1;
  if(fwrite(ph->mem,1,ph->msz,f)!=ph->msz) return 1;

  cmph_dump(ph->hash,f);
  return 0;
}

dict_t* dict_load_file(FILE* f)
{
  if(!f)
    return 0;

  dict_t* rv=0;

  uint32_t m=0;
  if(fread(&m,1,sizeof(m),f)!=sizeof(m)) goto err;
  if(m!=magic)  goto err;
  rv=calloc(sizeof(dict_t),1);

  if(fread(rv->uuid,1,sizeof(rv->uuid),f)!=sizeof(rv->uuid)) goto err;
  if(fread(&rv->sz,1,sizeof(rv->sz),f)!=sizeof(rv->sz)) goto err;
  if(fread(&rv->msz,1,sizeof(rv->msz),f)!=sizeof(rv->msz)) goto err;
  if(fread(&rv->max,1,sizeof(rv->max),f)!=sizeof(rv->max)) goto err;

  rv->data=malloc(sizeof(uint32_t)*rv->sz);
  if(fread(rv->data,1,rv->sz*sizeof(uint32_t),f)!=(rv->sz*sizeof(uint32_t))) goto err;

  rv->mem=malloc(rv->msz);
  if(fread(rv->mem,1,rv->msz,f)!=rv->msz) goto err;

  if(rv->hash=cmph_load(f))
    return rv;
err:
  dict_free(rv);
  return 0;
}


void dict_dump(const dict_t* d,FILE *f)
{
  if(!d || !f)  return;
  fprintf(f,"uuid %s\n",d->uuid);
  fprintf(f,"size %u msz %ju\n",d->sz,d->msz);
  for(size_t i=0;i<d->sz;i++)
  {
    const char* s=d->mem+d->data[i];
    uint32_t h=dict_get_str(d,s);
    fprintf(f,"%zd\t%u\t%u\t%s\t%s\n",i,h,d->data[h],s,d->mem+d->data[h]);
  }
}


