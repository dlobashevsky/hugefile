
#define DICT_NOT_FOUND		((uint32_t)(-1))

typedef struct dict_t dict_t;


//! create from string array, keylen taken as strlen+1.
dict_t* dict_init_strings(const char* uuid,char** data,size_t sz);
//! create from 1st field of tsv file
dict_t* dict_init_tsv(const char* uuid,const char* file);

//! destructor
void dict_free(dict_t*);

//! getter, return record number or (uint32_t)-1.
uint32_t dict_get(const dict_t*,const void* key,size_t keylen);
//! getter, return record number or (uint32_t)-1.
uint32_t dict_get_str(const dict_t*,const char* key);
//! return count of items
uint32_t dict_get_size(const dict_t*);
//! return amount of memory
uint64_t dict_get_bytes(const dict_t*);
//! get string by index
const char* dict_get_byidx(const dict_t* ph,size_t idx);
//! get uuid
const char* dict_get_uuid(const dict_t*);
//! get maximal key length
ssize_t dict_get_max(const dict_t*);


//! save
int dict_save(const dict_t*,const char* fn);
//! save in stream
int dict_save_file(const dict_t*,FILE* f);
//! load
dict_t* dict_load(const char* fn);
//! load from stream
dict_t* dict_load_file(FILE* f);

void dict_dump(const dict_t* d,FILE *f);
