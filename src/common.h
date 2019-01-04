

#define HFILE_VERSION		0x00000001U

//! use huge pages
#define HFILE_USE_HUGEPAGES	0

//! check control sum on open
#define HFILE_CHECKSUM_ONOPEN	1

#define HFILE_NOT_FOUND		((uint64_t)(-1LL))

// constants

#define MAGIC		0x0deccaU
#define MAGIC2		0xdeadc0deU
#define UUID_SIZE	37

#define CHECKSUM_SIZE	20
#define CHECKSUM_NAME	"SHA1"

#ifdef DEBUG
#define log(s,...)	fprintf(stderr,"# %s <%s:%u>:\t" s "\n", __func__,__FILE__,__LINE__, ##__VA_ARGS__)
#else
#define log(s,...)	fprintf(stderr,s "\n", ##__VA_ARGS__)
#endif

#define md_malloc(x)	({ void* tmp__=malloc(x); if(!tmp__) crash("memory error"); tmp__; })
#define md_free		free
#define md_calloc(x)	({ void* tmp__=calloc(x,1); if(!tmp__) crash("memory error"); tmp__; })
#define md_strdup(x)	({ void* tmp__=x ? strdup(x) : 0; if(!tmp__) crash("memory error"); tmp__; })
#define md_realloc(x,y)	({ void* tmp__=realloc(x,y); if(!tmp__) crash("memory error"); tmp__; })

#define md_tmalloc(x,y)		((x*)md_malloc(sizeof(x)*(y)))
#define md_tcalloc(x,y)		((x*)md_calloc(sizeof(x)*(y)))
#define md_pmalloc(x)		((void*)(md_malloc((x)*sizeof(void*))))
#define md_pcalloc(x)		((void*)(md_calloc((x)*sizeof(void*))))
#define md_new(x)		((typeof(x))md_calloc(sizeof(x[0])))
#define md_anew(x,y)		((typeof(x))md_calloc(sizeof(x[0])*(y)))

__attribute__ ((noreturn))
static inline void error_abort(const char* msg,const char* func,const char* file,int line)
{
  fprintf(stderr,"ERROR %s <%s:%d>\t",func,file,line);
  perror(msg);abort();
}

#define crash(x_)	error_abort(x_,__func__,__FILE__,__LINE__)
