#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <locale.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <microhttpd.h>

#include "common.h"
#include "checksum.h"
#include "utils.h"
#include "dict.h"
#include "hfile.h"

#define HEADER_PREFIX		"http_"



static size_t served_size=0;
static size_t served_count=0;
static size_t served200=0;
static size_t served404=0;
static time_t started=0;

static int answer(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data,
                  size_t *upload_data_size, void **con_cls);


static const char* usage="Usage:"
"\t./hfile_http -b <listen_socket> -p <listen_port> -d <database_file> -z -x prefix\n"
"Options (with default values):\n"
// "\t-b 127.0.0.1\tbind address\n"
"\t-p 8080\tlisten port\n"
"\t-t 8\tthgread pool size\n"
"\t-z\tdaemonize, default run in foreground\n"
"\t-d <datafile>\tdatafile to server, mandatory parameter, there is no default value\n"
"\t-l <logfile>\tfile to log errors, stderr default\n"
"\t-x <prefix>\tprefix to place before url, i.e. GET /tiles/12/234/546.png with prefix /tiles/ will search 12/234/546.png in database\n"
"\t-h\tthis help\n\n"
;

static char* prefix=0;
static hfile_t* hf=0;

int main(int ac,char** av)
{
  if(ac<2)
  {
    fputs(usage,stderr);
    return 0;
  }

  int c;

  int bg=0;
  int bind_port= -1;
  int pool= -1;
  char* database=0;
  char* log_file=0;

  opterr=0;

  while((c=getopt(ac,av,"hzb:p:d:l:x:t:"))!=-1)
    switch(c)
    {
      case 'h':
        fputs(usage,stderr);
        return 0;

      case 'z':
        bg=1;
        continue;

      case 'd':
        database=optarg;
        continue;

      case 'p':
        bind_port=atol(optarg);
        continue;
      case 't':
        pool=atol(optarg);
        continue;
      case 'l':
        log_file=optarg;
        continue;
      case 'x':
        prefix=optarg;
        continue;

      default:
        fprintf(stderr,"unknoun option -%c\n",c);
        fputs(usage,stderr);
        return 1;
    }

  if(!database)
    return fputs("No database to serve",stderr);

  if(bind_port<0)  bind_port=8080;
  if(!prefix)  prefix="";
  if(pool<=1) pool=8;

  if(log_file)
  {
    FILE* f=fopen(log_file,"a");
    if(!f)
    {
      fprintf(stderr,"Error log file creating for <%s>: %s\n",log_file,strerror(errno));
      exit(1);
    }
    fclose(f);
  }

  if(bg)  daemon(1,0);

  if(log_file)
  {
    FILE* f=fopen(log_file,"a");
    if(!f)  abort();
    fclose(stderr);
    stderr=f;
  }

  setlinebuf(stderr);

  hf=hfile_open(database);
  if(!hf)
  {
    fprintf(stderr,"no database '%s', %s:%u\n",database,__func__,__LINE__);
    abort();
  }

  setlocale(LC_ALL,"");

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK,&mask,0);
  int sfd=signalfd(-1,&mask,0);

  started=time(0);

  struct MHD_Daemon *proc=MHD_start_daemon(MHD_USE_SELECT_INTERNALLY|MHD_USE_EPOLL,bind_port,0,0,&answer,0,
    MHD_OPTION_LISTENING_ADDRESS_REUSE,1,
    MHD_OPTION_THREAD_POOL_SIZE,pool,
  MHD_OPTION_END);

  if(!proc)  return 1;

  for(;;)
  {
    struct signalfd_siginfo si;
    read(sfd, &si, sizeof(si));
    if(si.ssi_signo==SIGTERM || si.ssi_signo==SIGINT)
      break;

    if(si.ssi_signo!=SIGUSR1)
      continue;

    fprintf(stderr,"STAT: uptime: %zd, reauests: %zd, success: %zd, 404: %zd, total size: %zd\n",
            time(0)-started,served_count,served200,served404,served_size);

  }

  MHD_stop_daemon(proc);
  hfile_free(hf);
  fputs("HTTP server stopped\n",stderr);
  if(log_file) fclose(stderr);
  return 0;
}


static const char* err404="<html><body>Not found</body></html>";

static int answer404(struct MHD_Connection *connection)
{
  struct MHD_Response *response=MHD_create_response_from_buffer(strlen(err404),(void*)err404,MHD_RESPMEM_PERSISTENT);
  if(!response) abort();
  int ret=MHD_queue_response(connection,MHD_HTTP_NOT_FOUND,response);
  MHD_destroy_response(response);
  served404++;
  return ret;
}

static int answer(void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
  served_count++;
  size_t url_len=strlen(url);
  size_t prefix_len=strlen(prefix);
  if(strcmp(method,"GET") && strcmp(method,"HEAD"))  return answer404(connection);
  if(prefix_len+hfile_maxlen(hf)<url_len)  return answer404(connection);
  if(prefix_len>=url_len)  return answer404(connection);
  if(memcmp(url,prefix,prefix_len))  return answer404(connection);

//  char name[prefix_len+url_len];
//  snprintf(name,prefix_len+url_len,"%s%s",prefix,url);
  hfile_ret_t* r=hfile_get(hf,url+prefix_len);
  if(!r) return answer404(connection);;

  struct MHD_Response *response=0;

  if(!strcmp(method,"GET"))
  {
    response=MHD_create_response_from_buffer(r->size,r->content,MHD_RESPMEM_PERSISTENT);
    served_size+=r->size;
  }
  else
    response=MHD_create_response_from_buffer(0,"",MHD_RESPMEM_PERSISTENT);

  if(!response)
  {
    fprintf(stderr,"can not create response\n");
    abort();
  }

  for(size_t i=0;i<r->metas;i++)
  {
    if(!strcmp(r->keys[i],"_mime"))
    {
      MHD_add_response_header(response,"Content-Type",r->vals[i]);
      continue;
    }

    if(!strcmp(r->keys[i],"_mtime"))
    {
      time_t t=strtoull(r->vals[i],0,10);
      struct tm tm;
      char ft[32]={0,};
      gmtime_r(&t,&tm);
      strftime(ft,sizeof(ft),"%a, %d %b %Y %T GMT",&tm);
      MHD_add_response_header(response,"Last-Modified",ft);
      continue;
    }
    if(strlen(r->keys[i])>strlen(HEADER_PREFIX) && !memcmp(HEADER_PREFIX,r->keys[i],strlen(HEADER_PREFIX)))
      MHD_add_response_header(response,r->keys[i]+strlen(HEADER_PREFIX),r->vals[i]);
  }

  char etag[2*CHECKSUM_SIZE+1]={0,};
  utils_str2hex(etag,r->checksum);
  MHD_add_response_header(response,"ETag",etag);

// expires
// ? Accept-Ranges ??

  int ret=MHD_queue_response(connection,MHD_HTTP_OK,response);
  MHD_destroy_response (response);

  hfile_ret_free(r);
  served200++;
  return ret;
}


