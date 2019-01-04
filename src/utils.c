#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "common.h"
#include "utils.h"

#define HIRES_TIME_STACK_LEVEL		128

uint64_t utils_getclock(void)
{
  struct timeval t;
  gettimeofday(&t,0);
  return t.tv_sec*1000ULL+(t.tv_usec+500ULL)/1000ULL;
}



typedef struct string_list_t
{
  char* key;
  char* val;
  struct string_list_t* next;
} string_list_t;

utils_line_t* utils_line_parse(char* buf)
{
  if(!buf)  return 0;
  if(!*buf || *buf=='\t' || *buf=='\n' || *buf=='\r')  return 0;
  char* p=strchr(buf,'\n'); if(p) *p=0;
  p=strchr(buf,'\r'); if(p) *p=0;

// right trim input line
  ssize_t z=strlen(buf);
  while(--z  && buf[z]=='\t') buf[z]=0;

  utils_line_t* rv=calloc(1,sizeof(*rv));
  rv->name=buf;
  rv->source=buf;
  p=strchr(buf,'\t');
  if(!p)  return rv;
  *p++=0;

  string_list_t* l=0;
  char* pair=0;
  while(pair=strsep(&p,"\t\n\r"))
    if(*pair)
    {
      if(*pair==':')
      {
        rv->source= ++pair;
        continue;
      }
      char* div=strchr(pair,':');
      if(!div)  continue;
      *div++=0;
      rv->metas++;
      string_list_t* new=malloc(sizeof(*new));
      new->next=l;
      new->key=pair;
      new->val=div;
      l=new;
    }

  rv->keys=malloc(rv->metas*2*sizeof(rv->keys[0]));
  rv->vals=rv->keys+rv->metas;

  size_t i=0;

  while(l)
  {
    rv->keys[i]=l->key;
    rv->vals[i++]=l->val;
    string_list_t* next=l->next;
    free(l);
    l=next;
  }

  return rv;
}

void utils_line_free(utils_line_t* s)
{
  if(!s)  return;
  free(s->keys);
  free(s);
}


int utils_mkpath(char* file_path, mode_t mode)
{
  if(!file_path || !*file_path)  return -1;
  char* p;
  for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) 
  {
    *p='\0';
    if (mkdir(file_path, mode)==-1) 
    {
      if (errno!=EEXIST) { *p='/'; return -1; }
    }
    *p='/';
  }
  return 0;
}


static const uint8_t hex_tbl[16]=
{
  '0','1','2','3','4','5','6','7',
  '8','9','a','b','c','d','e','f'
};


int utils_bin2hex(char* res,const uint8_t* src,size_t sz)
{
  if(!res || !src)  return 0;
  *res=0;
  if(!sz)  return 0;
  while(sz--)
  {
    *res++=hex_tbl[(*src&0xf0)>>4];
    *res++=hex_tbl[*src&0xf];
    src++;
  }
  *res=0;
  return 0;
}

size_t utils_getCPUs(void)
{
  size_t rv=0;
  cpu_set_t np;
  sched_getaffinity(0,sizeof(np),&np);
  for(size_t i=0;i<sizeof(np);i++)
    rv+=!!(CPU_ISSET(i,&np));
  return rv;
}

uintmax_t utils_time_cpu(void)
{
  struct timespec tm;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tm);
  return tm.tv_sec*1000000ULL+(tm.tv_nsec+500ULL)/1000ULL;
}


uintmax_t utils_time_cpu_res(void)
{
  struct timespec tm;
  clock_getres(CLOCK_PROCESS_CPUTIME_ID,&tm);
  if(tm.tv_sec) abort();
  return tm.tv_nsec;
}

static __thread struct 
{
  ssize_t depth;
  uintmax_t data[HIRES_TIME_STACK_LEVEL];
} hirestime={depth:-1};


uintmax_t utils_time_abs(void)
{
  struct timeval t;
  gettimeofday(&t,0);
  return t.tv_sec*1000ULL+(t.tv_usec+500ULL)/1000ULL;
}

void utils_time_push(void)
{
  if(hirestime.depth+1>=HIRES_TIME_STACK_LEVEL)
    abort();
  hirestime.data[++hirestime.depth]=utils_time_abs();
}

uintmax_t utils_time_get(void)
{
  return utils_time_abs()-hirestime.data[hirestime.depth];
}

uintmax_t utils_time_pop(void)
{
  return utils_time_abs()-hirestime.data[(hirestime.depth>=0)?hirestime.depth--:0];
}

char* utils_time_format(uintmax_t time)
{
  char *bf;
  if(time<60000)
  {
     asprintf(&bf,"%ju ms",time);
     return bf;
  }
  
  uintmax_t secs=time/1000U;
  uintmax_t mins=secs/60;
  uintmax_t hours=mins/60;
  uintmax_t days=hours/24;
  uintmax_t weeks=days/7;

  secs%=60;
  mins%=60;
  hours%=24;
  days%=7;
  
  if(weeks)
  {
     asprintf(&bf,"%ju weeks %ju days %02ju:%02ju:%02ju.%03ju",weeks,days,hours,mins,secs,time%1000);
     return bf;
  }
  if(days)
  {
     asprintf(&bf,"%ju days %02ju:%02ju:%02ju.%03ju",days,hours,mins,secs,time%1000);
     return bf;
  }

  asprintf(&bf,"%02ju:%02ju:%02ju.%03ju",hours,mins,secs,time%1000);
  return bf;
}
