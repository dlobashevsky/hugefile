#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include "common.h"
#include "checksum.h"
#include "utils.h"
#include "dict.h"
#include "hfile.h"
#include "memcache.h"


static const char* usage="hugefle manipulation program\n"
"usage:\n"
"hugefile -h\n"
"\tprint this help\n"
"hugefile -c -d database_to_create -s source_filelist\n"
"\tcreate hugefile from list of files, filelist format see in README.md, in simplest case it is a result of find $DIR -type f >filelist\n"
"hugefile -x -d database -o target_folder [-f filter] [-s filelist_for_mapping]\n"
"\textract all (or selected) files from database to specified folder\n"
"hugefile -t -d database\n"
"\tperform consistency check, report out to stderr\n"
"hugefile -p -d database -o outfile\n"
"\tdump structure to loooong text file\n"
"hugefile -i -d database\n"
"\tprint base statistic to stderr\n"
"hugefile -r -d database -o repaired_database\n"
"\trepair database by copy to another database\n"
"hugefile -l -d database -o filelist\n"
"\tgenerate filelist from database\n"
"\n";

//"\t-a -d database -s source_filelist -o output_database\n"
//"\tappend filelist to database, result is new database\n"

//"\t-j -d database -s database2 -o output_database\n"
//"\tjoin two databases to new, newer files will replace older\n"

//"hugefile -m config.json\n"
//"\trun as memcache server (GET only), fields of JSON file described in documentation\n"


// extract by list -x -d [-s list | -f filter]
// join databases -j db1 db2 db3 ....


static int main_create(const char* database,const char* source);
static int main_extract(const char* database,const char* output,const char* filter);
static int main_test(const char* database);
static int main_dump(const char* database,const char* output);
static int main_stat(const char* database);
static int main_repair(const char* database,const char* output);
static int main_list(const char* database,const char* output);
static int main_memcache(const char* source);
static int main_append(const char* database,const char* source,const char* output);
static int main_join(const char* database1,const char* database2,const char* output);

int main(int ac,char** av)
{
  if(ac<2)
  {
    fputs(usage,stderr);
    return 0;
  }

  int c;
  char command=0;
  char* database=0;
  char* source=0;
  char* output=0;
  char* filter=0;

  opterr=0;

  while((c=getopt(ac,av,"hcxtpirlad:s:o:f:"))!=-1)
    switch(c)
    {
      case 'h':
        fputs(usage,stderr);
        return 0;

      case 'c':
      case 'x':
      case 't':
      case 'p':
      case 'i':
      case 'r':
      case 'l':
      case 'a':
        if(command)
        {
          log("mutual exclusive commands -%c and -%c",command,c);
          fputs(usage,stderr);
          return 1;
        }
        command=c;
        continue;
      case 'd':
        database=optarg;
        continue;

      case 'f':
        filter=optarg;
        continue;

      case 's':
        source=optarg;
        continue;
      case 'o':
        output=optarg;
        continue;
/*
      case 'm':
        if(command)
        {
          log("mutual exclusive commands -%c and -%c",command,c);
          fputs(usage,stderr);
          return 1;
        }
        command=c;
        source=optarg;
        continue;
*/
      default:
        log("unknoun option -%c",c);
        fputs(usage,stderr);
        return 1;
    }


  switch(command)
  {
    case 'c':
      return main_create(database,source);
    case 'x':
      return main_extract(database,output,filter);
    case 't':
      return main_test(database);
    case 'p':
      return main_dump(database,output);
    case 'i':
      return main_stat(database);
    case 'r':
      return main_repair(database,output);
    case 'l':
      return main_list(database,output);
    case 'm':
      return main_memcache(source);
    case 'a':
      return main_append(database,source,output);
  }

  log("sorry, no valid command");
  return 0;
}



static int main_create(const char* database,const char* source)
{
  int ret=hfile_build(database,source,0);
  log("database \"%s\" build %ssuccessfull",database,ret ? "un" : "");
  return ret;
}

static int main_extract(const char* database,const char* output,const char* filter)
{
  hfile_t* hf=hfile_open(database);
  if(!hf)
  {
    log("fail to open database \"%s\"",database);
    return 1;
  }
  int ret=hfile_extract(hf,output,filter,0777);
  hfile_free(hf);

  if(ret)
    log("fail to extract from database \"%s\" to \"%s\"",database,output);
  return ret;
}

static int main_test(const char* database)
{
  log("sorry, not yet implemented");
  return 0;
}

static int main_dump(const char* database,const char* output)
{
  hfile_t* hf=hfile_open(database);
  if(!hf)
  {
    log("can not open database <%s>",database);
    return -1;
  }
  int ret=hfile_dump(hf,output);
  hfile_free(hf);
  return ret;
}

static int main_stat(const char* database)
{
  hfile_t* hf=hfile_open(database);
  if(!hf)
  {
    log("can not open database <%s>",database);
    return -1;
  }
  int ret=hfile_stat(hf);
  hfile_free(hf);
  return ret;
  return 0;
}

static int main_repair(const char* database,const char* output)
{
  log("sorry, not yet implemented");
  return 0;
}

static int main_list(const char* database,const char* output)
{
  hfile_t* hf=hfile_open(database);
  if(!hf)
  {
    log("can not open database <%s>",database);
    return -1;
  }

  int ret=hfile_genlist(hf,output);
  hfile_free(hf);
  return ret;
}

static int main_memcache(const char* source)
{
  log("sorry, not yet implemented");
  return 0;
}

static int main_append(const char* database,const char* source,const char* output)
{
  log("sorry, not yet implemented");
  return 0;
}

