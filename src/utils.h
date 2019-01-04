//! \file
//! \brief string parsers and file utils

typedef struct utils_line_t
{
  const char* name;
  const char* source;
  uint16_t metas;
  const char** keys;
  const char** vals;
} utils_line_t;

//! parse line
utils_line_t* utils_line_parse(char* buf);
//! free structure
void utils_line_free(utils_line_t*);

//! create recursive path
int utils_mkpath(char* file_path, mode_t mode);

//! convert data to hexadecimal, caller responsible for allocate at least 2*src_size+1 bytes
int utils_bin2hex(char* res,const uint8_t* src,size_t src_size);

//! get current time in milliseconds precision
uint64_t utils_getclock(void);

//! get number of CPUs/cores in system
size_t utils_getCPUs(void);

//! get absolute time in ms
uintmax_t utils_time_abs(void);
//! set mark for HiRes time
void utils_time_push(void);
//! get HiRes time
uintmax_t utils_time_get(void);
//! get HiRes time and destroy mark
uintmax_t utils_time_pop(void);

//! get foramtted time, return  must be freed by caller by free, not sp_free
char* utils_time_format(uintmax_t time);

#define utils_time_get_auto					\
 ({                     					\
   char *_tm=utils_time_format(utils_time_get());		\
   char *_rv=strdupa(_tm);					\
  free(_tm);							\
  _rv;								\
 })

#define utils_time_pop_auto					\
 ({                     					\
   char *_tm=utils_time_format(utils_time_pop());		\
   char *_rv=strdupa(_tm);					\
  free(_tm);							\
  _rv;								\
 })

#define tic utils_time_push()

#define toc							\
 ({                     					\
   char *_tm=utils_time_format(utils_time_pop());		\
   char *_rv=strdupa(_tm);					\
   free(_tm);							\
  _rv;								\
 })


#define str2auto(x_)						\
 ({                     					\
   char *rv_=strdupa(x_);					\
   free(x_);							\
   rv_;								\
 })



//! return cpu time in us
uintmax_t utils_time_cpu(void);
//! return cpu time resolution in ns
uintmax_t utils_time_cpu_res(void);

