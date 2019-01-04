//! \file
//! \brief hugefile API

#define HFILE_FORMAT_VERSION		1

//! need gzip data
//#define HFILE_FLAG_GZIP


//! set if file is deleted
#define HFILE_FILE_FLAG_DELETED		1
//! set if file is compressed
#define HFILE_FILE_FLAG_CORRUPTED	2
//! set if file is compressed
//#define HFILE_FILE_FLAG_GZIP		4


typedef struct hfile_t hfile_t;


typedef struct hfile_ret_t
{
  const char* name;
  size_t size;
  void* content;
  const uint8_t* checksum;
  size_t metas;
  const char** keys;
  const char** vals;
  size_t dups;
} hfile_ret_t;

//! open with main hash in memory
hfile_t* hfile_open(const char* base);
//! destructor
void hfile_free(hfile_t*);

//! build new index file from text with filenames
int hfile_build(const char* result,const char* input,uint32_t flags);
//! extract files to given folder
int hfile_extract(hfile_t*,const char* folder_to,const char* regex,mode_t dirmode);

#if 0
//! repair data file
int hfile_repair(const char* source,const char* repaired);

//! create new database from old and append filelist
int hfile_append(const char* result,const char* database,const char* source_list);

#endif

//! do deep integrity check
int hfile_integrity_check(const char* base);
//! print base stat
int hfile_stat(const hfile_t*);
//! dump index to text files with additional info
int hfile_dump(const hfile_t*,const char* outfolder);
//! dump list of files suitable for import
int hfile_genlist(const hfile_t*,const char* outfile);

//accessors:

//! get names count
ssize_t hfile_name_count(const hfile_t*);
//! get name by index
const char* hfile_name_by_idx(const hfile_t*,size_t idx);
//! get index by name
ssize_t hfile_idx_by_name(const hfile_t*,const char* name);

//! get files count
ssize_t hfile_file_count(const hfile_t*);
//! get file content by index
const void* hfile_file_by_name(const hfile_t*,const char* name,size_t* size);

//! get maximal filename length
ssize_t hfile_maxlen(const hfile_t*);


hfile_ret_t* hfile_get(const hfile_t* h,const char* name);
void hfile_ret_free(hfile_ret_t*);

// scanning

//! iterator by file name
typedef struct hfile_it_t
{
  const hfile_t* hf;
  size_t cur;
} hfile_it_t;

//! ctr, is
hfile_it_t* hfile_it_init(const hfile_t*);
//! dtr
void hfile_it_free(hfile_it_t*);
//! get file
hfile_ret_t* hfile_it_get(hfile_it_t* h);
//! restart iterator
int hfile_it_rewind(hfile_it_t*);

//! get random name
hfile_ret_t* hfile_get_rand_name(const hfile_t* h);

