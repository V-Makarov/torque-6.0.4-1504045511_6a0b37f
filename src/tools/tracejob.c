/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif
#include "pbs_ifl.h"
#include "log.h"
#include "tracejob.h"


/* path from pbs home to the log files */

const char *mid_path[] =
  {
  "server_priv/accounting",
  "server_logs",
  "mom_logs",
  "sched_logs"
  };

struct log_entry *log_lines;
int ll_cur_amm;
int ll_max_amm;


int main(

  int   argc,
  char *argv[])

  {
  /* Array for the log entries for the specified job */
  FILE *fp;
  int i, j;
  int file_count;
  char *filenames[MAX_LOG_FILES_PER_DAY];  /* full path of logfiles to read */

  struct tm *tm_ptr;
  time_t t, t_save;
  signed char c;
  char *prefix_path = NULL;
  int number_of_days = 1;
  char *endp;
  short error = 0;
  int opt;
  char no_acct = 0, no_svr = 0, no_mom = 0, no_schd = 0;
  char verbosity = 1;
  int wrap = -1;
  int log_filter = 0;
  int event_type;
  char filter_excessive = 0;
  int excessive_count;

#if defined(FILTER_EXCESSIVE)
  filter_excessive = 1;
#endif

#if defined(EXCESSIVE_COUNT)
  excessive_count = EXCESSIVE_COUNT;
#endif

  while ((c = getopt(argc, argv, "qzvamslw:p:n:f:c:")) != EOF)
    {
    switch (c)
      {

      case 'q':

        verbosity = 0;

        break;

      case 'v':

        verbosity = 2;

        break;

      case 'a':

        no_acct = 1;

        break;

      case 's':

        no_svr = 1;

        break;

      case 'm':

        no_mom = 1;

        break;

      case 'l':

        no_schd = 1;

        break;

      case 'z':

        filter_excessive = filter_excessive ? 0 : 1;

        break;

      case 'c':

        excessive_count = strtol(optarg, &endp, 10);

        if (*endp != '\0')
          error = 1;

        break;

      case 'w':

        wrap = strtol(optarg, &endp, 10);

        if (*endp != '\0')
          error = 1;

        break;

      case 'p':

        prefix_path = optarg;

        break;

      case 'n':

        number_of_days = strtol(optarg, &endp, 10);

        if (*endp != '\0')
          error = 1;

        break;

      case 'f':

        if (!strcmp(optarg, "error"))
          log_filter |= PBSEVENT_ERROR;
        else if (!strcmp(optarg, "system"))
          log_filter |= PBSEVENT_SYSTEM;
        else if (!strcmp(optarg, "admin"))
          log_filter |= PBSEVENT_ADMIN;
        else if (!strcmp(optarg, "job"))
          log_filter |= PBSEVENT_JOB;
        else if (!strcmp(optarg, "job_usage"))
          log_filter |= PBSEVENT_JOB_USAGE;
        else if (!strcmp(optarg, "security"))
          log_filter |= PBSEVENT_SECURITY;
        else if (!strcmp(optarg, "sched"))
          log_filter |= PBSEVENT_SCHED;
        else if (!strcmp(optarg, "debug"))
          log_filter |= PBSEVENT_DEBUG;
        else if (!strcmp(optarg, "debug2"))
          log_filter |= PBSEVENT_DEBUG2;
        else if (isdigit(optarg[0]))
          {
          log_filter = strtol(optarg, &endp, 16);

          if (*endp != '\0')
            error = 1;
          }
        else
          error = 1;

        break;

      default:

        error = 1;

        break;
      }
    }    /* END while ((c = getopt(argc,argv,"zvamslw:p:n:f:c:")) != EOF) */


  /* no jobs */

  if ((error != 0) || (argc == optind))
    {
    printf("USAGE: %s [-a|s|l|m|q|v|z] [-c count] [-w size] [-p path] [-n days] [-f filter_type] <JOBID>\n",
           strip_path(argv[0]));

    printf(
      "   -p : path to PBS_SERVER_HOME\n"
      "   -w : number of columns of your terminal\n"
      "   -n : number of days in the past to look for job(s) [default 1]\n"
      "   -f : filter out types of log entries, multiple -f's can be specified\n"
      "        error, system, admin, job, job_usage, security, sched, debug, \n"
      "        debug2, or absolute numeric hex equivalent\n"
      "   -z : toggle filtering excessive messages\n");
    printf(
      "   -c : what message count is considered excessive\n"
      "   -a : don't use accounting log files\n"
      "   -s : don't use server log files\n"
      "   -l : don't use scheduler log files\n"
      "   -m : don't use mom log files\n"
      "   -q : quiet mode - hide all error messages\n"
      "   -v : verbose mode - show more error messages\n");

    printf("default prefix path = %s\n",
           PBS_SERVER_HOME);

#if defined(FILTER_EXCESSIVE)
    printf("filter_excessive: ON\n");
#else
    printf("filter_excessive: OFF\n");
#endif
    return(1);
    }  /* END if ((error != 0) || (argc == optind)) */

  if (wrap == -1)
    wrap = get_cols();

  time(&t);

  t_save = t;

  for (opt = optind;opt < argc;opt++)
    {
    for (i = 0, t = t_save;i < number_of_days;i++, t -= SECONDS_IN_DAY)
      {
      tm_ptr = localtime(&t);

      for (j = 0;j < 4;j++)
        {
        if ((j == IND_ACCT && no_acct) || (j == IND_SERVER && no_svr) ||
            (j == IND_MOM && no_mom)   || (j == IND_SCHED && no_schd))
          continue;

        file_count = log_path(prefix_path, j, tm_ptr, filenames);
        
        /* there can be multiple server and mom log files per day */
        /* traverse filenames until we have got them all */
        
        if (file_count < 0)
          {
          printf("Error getting file names\n");
          continue;
          }

        for (; file_count > 0; file_count--)
          {
          if ((fp = fopen(filenames[file_count-1], "r")) == NULL)
            {
            if (verbosity >= 1)
              perror(filenames[file_count-1]);

            continue;
            }

          if (parse_log(fp, argv[opt], j) < 0)
            {
            /* no valid entries located in file */

            if (verbosity >= 1)
              {
              fprintf(stderr, "%s: No matching job records located\n",
                      filenames[file_count-1]);
              }
            }
          else if (verbosity >= 2)
            {
            fprintf(stderr, "%s: Successfully located matching job records\n",
                    filenames[file_count-1]);
            }

          fclose(fp);
          free(filenames[file_count-1]);
          } /* end of for file_count */
        }
      }    /* END for (i) */

    if (filter_excessive)
      filter_excess(excessive_count);

    qsort(log_lines, ll_cur_amm, sizeof(struct log_entry), sort_by_date);

    if (ll_cur_amm != 0)
      {
      printf("\nJob: %s\n\n",
             log_lines[0].name);
      }

    for (i = 0;i < ll_cur_amm;i++)
      {
      if (log_lines[i].log_file == 'A')
        event_type = 0;
      else
        event_type = strtol(log_lines[i].event, &endp, 16);

      if (!(log_filter & event_type) && !(log_lines[i].no_print))
        {
        printf("%-20s %-5c",
               log_lines[i].date,
               log_lines[i].log_file);

        line_wrap(log_lines[i].msg, 26, wrap);
        }
      }
    }    /* END for (opt) */

  return(0);
  }  /* END main() */




/*
 *
 * parse_log - parse out entires of a log file for a specific job
 *      and return them in log_entry structures
 *
 *        fp    - the log file
 *        job   - the name of the job
 *        ind   - which log file - index in enum index
 *
 * returns nothing
 * modifies global variables: loglines, ll_cur_amm, ll_max_amm
 *
 */

int parse_log(

  FILE *fp,   /* I */
  char *job,  /* I */
  int   ind)  /* I */

  {

  struct log_entry tmp; /* temporary log entry */
  char buf[32768]; /* buffer to read in from file */
  char *pa, *pe;   /* pointers to use for splitting */
  int field_count; /* which field in log entry */
  int j = 0;

  struct tm tms; /* used to convert date to unix date */
  static char none[1] = { '\0' };
  int lineno = 0;

  int logcount = 0;

  tms.tm_isdst = -1; /* mktime() will attempt to figure it out */

  while (fgets(buf, sizeof(buf), fp) != NULL)
    {
    lineno++;
    j++;

    buf[strlen(buf) - 1] = '\0';

    field_count = 0;
    pa = buf;
    memset(&tmp, 0, sizeof(struct log_entry));

    for(field_count = 0; (pa != NULL) && (field_count <= FLD_MSG); field_count++) 
      {

      /* instead of using strtok every time, conditionally advance the pa (the field pointer)
       * on semicolons. This prevents data from getting cut out of messages with semicolons in
       * them */
      if(field_count < FLD_MSG) 
        {
        if((pe = strchr(pa, ';')))
          *pe = '\0';
        } 
      else 
        {
        pe = NULL;
        }

      switch (field_count) 
        
        {
        case FLD_DATE:

          tmp.date = pa;
          if(ind == IND_ACCT)
            field_count += 2;

          break;

      case FLD_EVENT:
        
          tmp.event = pa;
        
          break;

      case FLD_OBJ:
        
          tmp.obj = pa;
        
          break;

      case FLD_TYPE:
        
          tmp.type = pa;
        
        
          break;

      case FLD_NAME:
        
          tmp.name = pa;
        
          break;

      case FLD_MSG:
        
          tmp.msg = pa;
        
          break;
      }

      if(pe)
        pa = pe + 1;
      else
        pa = NULL;

    } /* END for (field_count) */

    if ((tmp.name != NULL) &&
        !strncmp(job, tmp.name, strlen(job)) &&
        !isdigit(tmp.name[strlen(job)]))
      {
      if (ll_cur_amm >= ll_max_amm)
        alloc_more_space();

      free_log_entry(&log_lines[ll_cur_amm]);

      if (tmp.date != NULL)
        {
        log_lines[ll_cur_amm].date = strdup(tmp.date);

        if (sscanf(tmp.date, "%d/%d/%d %d:%d:%d", &tms.tm_mon, &tms.tm_mday, &tms.tm_year, &tms.tm_hour, &tms.tm_min, &tms.tm_sec) != 6)
          log_lines[ll_cur_amm].date_time = -1; /* error in date field */
        else
          {
          if (tms.tm_year > 1900)
            tms.tm_year -= 1900;

          log_lines[ll_cur_amm].date_time = mktime(&tms);
          }
        }

      if (tmp.event != NULL)
        log_lines[ll_cur_amm].event = strdup(tmp.event);
      else
        log_lines[ll_cur_amm].event = none;

      if (tmp.obj != NULL)
        log_lines[ll_cur_amm].obj = strdup(tmp.obj);
      else
        log_lines[ll_cur_amm].obj = none;

      if (tmp.type != NULL)
        log_lines[ll_cur_amm].type = strdup(tmp.type);
      else
        log_lines[ll_cur_amm].type = none;

      if (tmp.name != NULL)
        log_lines[ll_cur_amm].name = strdup(tmp.name);
      else
        log_lines[ll_cur_amm].name = none;

      if (tmp.msg != NULL)
        log_lines[ll_cur_amm].msg = strdup(tmp.msg);
      else
        log_lines[ll_cur_amm].msg = none;

      switch (ind)
        {

        case IND_SERVER:
          log_lines[ll_cur_amm].log_file = 'S';
          break;

        case IND_SCHED:
          log_lines[ll_cur_amm].log_file = 'L';
          break;

        case IND_ACCT:
          log_lines[ll_cur_amm].log_file = 'A';
          break;

        case IND_MOM:
          log_lines[ll_cur_amm].log_file = 'M';
          break;

        default:
          log_lines[ll_cur_amm].log_file = 'U'; /* undefined */
        }

      log_lines[ll_cur_amm].lineno = lineno;

      ll_cur_amm++;

      logcount++;
      }
    }    /* END while (fgets(buf,sizeof(buf),fp) != NULL) */

  if (logcount == 0)
    {
    /* FAILURE */

    return(-1);
    }

  /* SUCCESS */

  return(0);
  }  /* END parse_log() */




/*
 *
 * sort_by_date - compare function for qsort.  It compares two time_t
 *   variables
 *
 */

int sort_by_date(

  const void *v1,  /* I */
  const void *v2)  /* I */

  {

  struct log_entry *l1, *l2;

  l1 = (struct log_entry*) v1;
  l2 = (struct log_entry*) v2;

  if (l1->date_time < l2->date_time)
    {
    return(-1);
    }
  else if (l1->date_time > l2->date_time)
    {
    return(1);
    }

  if (l1->log_file == l2->log_file)
    {
    if (l1->lineno < l2->lineno)
      {
      return(-1);
      }
    else if (l1->lineno > l2->lineno)
      {
      return(1);
      }
    }

  return(0);
  }




/*
 *
 * sort_by_message - compare function used by qsort.  Compares the message
 *
 */
int sort_by_message(const void *v1, const void *v2)
  {
  return strcmp(((struct log_entry*) v1) -> msg, ((struct log_entry*) v2) -> msg);
  }

/*
 *
 * strip_path - strips all leading path and returns the command name
 *   i.e. /usr/bin/vi => vi
 *
 *   path - the path to strip
 *
 * returns striped path
 *
 */
char *strip_path(char *path)
  {
  char *p;

  p = path + strlen(path);

  while (p != path && *p != '/')
    p--;

  if (*p == '/')
    p++;

  return p;
  }

/*
 *
 * free_log_entry - free the interal data used by a log entry
 *
 *   lg - log entry to free
 *
 * returns nothing
 *
 */
void free_log_entry(struct log_entry *lg)
  {
  if (lg -> date != NULL)
    free(lg -> date);

  lg -> date = NULL;

  if (lg -> event != NULL)
    free(lg -> event);

  lg -> event = NULL;

  if (lg -> obj != NULL)
    free(lg -> obj);

  lg -> obj = NULL;

  if (lg -> type != NULL)
    free(lg -> type);

  lg -> type = NULL;

  if (lg -> name != NULL)
    free(lg -> name);

  lg -> name = NULL;

  if (lg -> msg != NULL)
    free(lg -> msg);

  lg -> msg = NULL;

  lg -> log_file = '\0';
  }

/*
 *
 * line_wrap - wrap lines at word margen and print
 *      The first line will be printed.  The rest will be indented
 *      by start and will not go over a max line length of end chars
 *
 *
 *   line  - the line to wrap
 *   start - ammount of whitespace to indent subsquent lines
 *   end   - number of columns in the terminal
 *
 * returns nothing
 *
 */

void line_wrap(char *line, int start, int end)
  {
  int wrap_at;
  int total_size;
  int start_index;
  char *cur_ptr;
  char *start_ptr;

  start_ptr = line;
  total_size = strlen(line);
  wrap_at = end - start;
  start_index = 0;

  if (end == 0)
    printf("%s\n", line);
  else
    {
    while (start_index < total_size)
      {
      if (start_index + wrap_at < total_size)
        {
        cur_ptr = start_ptr + wrap_at;

        while (cur_ptr > start_ptr && *cur_ptr != ' ')
          cur_ptr--;

        if (cur_ptr == start_ptr)
          {
          cur_ptr = start_ptr + wrap_at;

          while (*cur_ptr != ' ' && *cur_ptr != '\0')
            cur_ptr++;
          }

        *cur_ptr = '\0';
        }
      else
        cur_ptr = line + total_size;

      /* first line, don't indent */
      if (start_ptr == line)
        printf("%s\n", start_ptr);
      else
        printf("%*s%s\n", start, " ", start_ptr);

      start_ptr = cur_ptr + 1;

      start_index = cur_ptr - line;
      }
    }
  }





/*
 *
 * log_path - create the path to a log file
 *
 *   path - prefix path
 *   index  - index into the prefix_path array
 *   tm_ptr - time pointer to create filename from
 *   filenames - returned filenames
 *
 * returns -1 if unsuccessful otherwise the number of filenames found
 *
 */

int log_path(

  char      *path,
  int        index,
  struct tm *tm_ptr,
  char      *filenames[])

  {
  static char buf[512];
  static char cmd[512 + 20];
  char        pbuf[512 + 10];
  int         filecount = 0;
  FILE       *fp;

  sprintf(buf, "%s/%s/%04d%02d%02d",
          (path != NULL) ? path : PBS_SERVER_HOME,
          mid_path[index],
          tm_ptr->tm_year + 1900,
          tm_ptr->tm_mon + 1,
          tm_ptr->tm_mday);

  filenames[filecount] = (char *)malloc(strlen(buf)+1);

  if(!filenames[filecount])
  {
    perror("malloc failed in log_path");
    return(-1);
  }

  strcpy(filenames[filecount],buf);
  filecount++;

  sprintf(cmd, "ls -1t %s.* 2> /dev/null",
    buf);

  if ((fp = popen(cmd, "r")) != NULL)
    {
    while (fgets(pbuf, 512, fp) != NULL)
      {
      filenames[filecount] = (char *)malloc(strlen(pbuf)+1);

      if (filenames[filecount] == NULL)
        {
        perror("malloc failed in log_path");
        pclose(fp);
        return(-1);
        }

      if (isspace(pbuf[strlen(pbuf)-1]))
        {
        pbuf[strlen(pbuf)-1] = '\0';
        }
      strcpy(filenames[filecount],pbuf);
      filecount++;
      }

    pclose(fp);
    }

  return(filecount);
  }




int get_cols(void)
  {

  struct winsize ws;

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

  return ws.ws_col;
  }

/*
 *
 * alloc_space - double the allocation of current log entires
 *
 */
void
alloc_more_space(void)
  {
  int old_amm = ll_max_amm;

  if (ll_max_amm == 0)
    ll_max_amm = DEFAULT_LOG_LINES;
  else
    ll_max_amm *= 2;

  if ((log_lines = (struct log_entry *)realloc(log_lines, ll_max_amm * sizeof(struct log_entry))) == NULL)
    {
    perror("Error allocating memory");
    exit(1);
    }

  memset(&log_lines[old_amm], 0, (ll_max_amm - old_amm) * sizeof(struct log_entry));
  }





/*
 *
 * filter_excess - count and set the no_print flags if the count goes over
 *         the message threshold
 *
 *   threshold - if the number of messages exceeds this, don't print them
 *
 * returns nothing
 *
 * NOTE: log_lines array will be sorted in place
 */

void filter_excess(int threshold)
  {
  int cur_count = 1;
  char *msg;
  int i;
  int j = 0;

  if (ll_cur_amm)
    {
    qsort(log_lines, ll_cur_amm, sizeof(struct log_entry), sort_by_message);
    msg = log_lines[0].msg;

    for (i = 1; i < ll_cur_amm; i++)
      {
      if (strcmp(log_lines[i].msg, msg) == 0)
        cur_count++;
      else
        {
        if (cur_count >= threshold)
          {
          /* we want to print 1 of the many messages */
          j++;

          for (; j < i; j++)
            log_lines[j].no_print = 1;
          }

        j = i;

        cur_count = 1;
        msg = log_lines[i].msg;
        }
      }

    if (cur_count >= threshold)
      {
      j++;

      for (; j < i; j++)
        log_lines[j].no_print = 1;
      }
    }
  }

