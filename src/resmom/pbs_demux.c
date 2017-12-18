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

/*
 * pbs_demux - handle I/O from multiple node job
 *
 * Standard Out and Standard Error of each task is bound to
 * stream sockets connected to pbs_demux which inputs from the
 * various streams and writes to the JOB's out and error.
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "server_limits.h"

#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#ifdef ENABLE_BLCR
#include <libcr.h>
#endif /* ENABLE_BLCR */

#include "lib_ifl.h"
#include "pbs_helper.h"


enum rwhere {invalid, new_out, new_err, old_out, old_err};

struct routem
  {
  enum rwhere r_where;
  short  r_nl;
  };

struct pollfd *readset = NULL;


void readit(

  int            sock,
  struct routem *prm)

  {
  int   amt;
  char  buf[256];
  FILE *fil;
  int   i;
  char *pc;

  // confirm readset has been initialized
  if (readset == NULL)
    return;

  if (prm->r_where == old_out)
    fil = stdout;
  else
    fil = stderr;

  i = 0;

  if ((amt = read_ac_socket(sock, buf, 256)) > 0)
    {

    for (pc = buf + i;pc < buf + amt;++pc)
      {
#ifdef DEBUG

      if (prm->r_nl != 0)
        {
        fprintf(fil, "socket %d: ",
                sock);

        prm->r_nl = 0;
        }

#endif /* DEBUG */

      putc(*pc, fil);

      if (*pc == '\n')
        {
        prm->r_nl = 1;

        fflush(fil);
        }
      }
    }
  else
    {
    close(sock);

    prm->r_where = invalid;

    // remove socket from readset
    readset[sock].fd = -1;
    readset[sock].events = 0;
    readset[sock].revents = 0;
    }

  return;
  }  /* END readit() */

#ifdef ENABLE_BLCR
static int demux_callback(void* arg)
{
    fflush(stdout);
    fflush(stderr);

    cr_checkpoint(CR_CHECKPOINT_OMIT);

    return 0;
}
#endif /* ENABLE_BLCR */

int main(

  int   UNUSED(argc),
  char *argv[])

  {
  int i;
  int maxfd;
  int main_sock_out = 3;
  int main_sock_err = 4;
  int n;
  int newsock;
  pid_t parent;

  struct pollfd *pollset;
  int pollset_size_bytes;

  struct routem *routem;

#ifdef ENABLE_BLCR
  if (cr_init() < 0)
    {
    perror("Failed to initialize BLCR.");
    exit(5);
    }

  (void)cr_register_callback(demux_callback, NULL, CR_THREAD_CONTEXT);
#endif /* ENABLE_BLCR */

  parent = getppid();

  /* disable cookie search - PW - mpiexec patch */

  /*
  cookie = getenv("PBS_JOBCOOKIE");

  if (cookie == 0)
    {
    fprintf(stderr, "%s: no PBS_JOBCOOKIE found in the env\n",
      argv[0]);

    exit(3);
    }

  #ifdef DEBUG
  printf("Cookie found in environment: %s\n",
    cookie);
  #endif
  */

  if((maxfd = sysconf(_SC_OPEN_MAX)) < 0)
    {
    perror("unexpected return from sysconf.");

    exit(5);
    }

  routem = (struct routem *)calloc(maxfd, sizeof(struct routem));

  if (routem == NULL)
    {
    perror("cannot alloc memory for routem");

    exit(5);
    }

  for (i = 0;i < maxfd;++i)
    {
    routem[i].r_where = invalid;
    routem[i].r_nl    = 1;
    }

  routem[main_sock_out].r_where = new_out;
  routem[main_sock_err].r_where = new_err;

  pollset_size_bytes = maxfd * sizeof(struct pollfd);

  readset = (struct pollfd *)malloc(maxfd * sizeof(struct pollfd));
  if (readset == NULL)
    {
    perror("cannot alloc memory for readset");

    exit(5);
    }

  // set initial values
  for (i = 0; i < maxfd; i++)
    {
    readset[i].fd = -1;
    readset[i].events = 0;
    readset[i].revents = 0;
    }

  readset[main_sock_out].fd = main_sock_out;
  readset[main_sock_out].events = POLLIN;

  readset[main_sock_err].fd = main_sock_err;
  readset[main_sock_err].events = POLLIN;

  // allocate local pollset to hold a copy of readset
  pollset = (struct pollfd *)malloc(pollset_size_bytes);
  if (pollset == NULL)
    {
    perror("cannot alloc memory for pollset");

    exit(5);
    }

  if (listen(main_sock_out, TORQUE_LISTENQUEUE) < 0)
    {
    perror("listen on out");

    exit(5);
    }

  if (listen(main_sock_err, TORQUE_LISTENQUEUE) < 0)
    {
    perror("listen on err");

    exit(5);
    }

  while (1)
    {
    // copy readset to local set for poll
    memcpy(pollset, readset, pollset_size_bytes);

    // wait for up to 10sec
    n = poll(pollset, maxfd, 10000);

    if (n == -1)
      {
      if (errno == EINTR)
        {
        n = 0;
        }
      else
        {
        fprintf(stderr, "%s: poll failed\n",
          argv[0]);

        exit(1);
        }
      }
    else if (n == 0)
      {
      /* NOTE:  on TRU64, init process does not have pid==1 */

      if (getppid() != parent)
        {
#ifdef DEBUG
        fprintf(stderr, "%s: Parent has gone, and so will I\n",
                argv[0]);
#endif /* DEBUG */

        break;
        }
      }    /* END else if (n == 0) */

    for (i = 0; (n > 0) && (i < maxfd); i++)
      {
      if (pollset[i].revents == 0)
        continue;

      // decrement count of structures with non-zero return events
      n--;

      if ((pollset[i].revents & POLLIN))
        {
        /* this socket has data */

        switch ((routem + i)->r_where)
          {

          case new_out:

          case new_err:

            newsock = accept(i, 0, 0);

            (routem + newsock)->r_where = (routem + i)->r_where == new_out ?
            old_out :
            old_err;

            // add new socket to readset for future polling
            readset[newsock].fd = newsock;
            readset[newsock].events = POLLIN;
            readset[newsock].revents = 0;

            break;

          case old_out:

          case old_err:

            readit(i, routem + i);

            break;

          default:

            fprintf(stderr, "%s: internal error\n",
                    argv[0]);

            exit(2);

            /*NOTREACHED*/

            break;
          }
        }
      }
    }    /* END while(1) */

  return(0);
  }  /* END main() */

/* END pbs_demux.c */
