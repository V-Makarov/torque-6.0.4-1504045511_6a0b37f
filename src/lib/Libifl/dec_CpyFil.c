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
 * decode_DIS_CopyFiles() - decode a Copy Files Dependency Batch Request
 *
 * This request is used by the server ONLY.
 * The batch request structure pointed to by preq must already exist.
 *
 * Data items are: string  job id   (may be null)
 *   string  job owner  (may be null)
 *   string  execution user name
 *   string  execution group name (may be null)
 *   unsigned int direction
 *   unsigned int count of file pairs in set
 *   set of  file pairs:
 *    unsigned int flag
 *    string  local path name
 *    string  remote path name (may be null)
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdlib.h>
#include "libpbs.h"
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "credential.h"
#include "batch_request.h"
#include "dis.h"
#include "tcp.h" /* tcp_chan */

int decode_DIS_CopyFiles(
    
  struct tcp_chan *chan,
  struct batch_request *preq)

  {
  int   pair_ct;

  struct rq_cpyfile *pcf;

  struct rqfpair    *ppair;
  int   rc;

  pcf = &preq->rq_ind.rq_cpyfile;
  CLEAR_HEAD(pcf->rq_pair);

  if ((rc = disrfst(chan, PBS_MAXSVRJOBID, pcf->rq_jobid)) != 0)
    return rc;

  if ((rc = disrfst(chan, PBS_MAXUSER, pcf->rq_owner)) != 0)
    return rc;

  if ((rc = disrfst(chan, PBS_MAXUSER, pcf->rq_user))  != 0)
    return rc;

  if ((rc = disrfst(chan, PBS_MAXGRPN, pcf->rq_group)) != 0)
    return rc;

  pcf->rq_dir = disrui(chan, &rc);

  if (rc) return rc;

  pair_ct = disrui(chan, &rc);

  if (rc) return rc;

  while (pair_ct--)
    {

    ppair = (struct rqfpair *)calloc(1, sizeof(struct rqfpair));

    if (ppair == (struct rqfpair *)0)
      return DIS_NOMALLOC;

    CLEAR_LINK(ppair->fp_link);

    ppair->fp_local = 0;

    ppair->fp_rmt   = 0;

    ppair->fp_flag = disrui(chan, &rc);

    if (rc)
      {
      (void)free(ppair);
      return rc;
      }

    ppair->fp_local = disrst(chan, &rc);

    if (rc)
      {
      (void)free(ppair);
      return rc;
      }

    ppair->fp_rmt = disrst(chan, &rc);

    if (rc)
      {
      (void)free(ppair->fp_local);
      (void)free(ppair);
      return rc;
      }

    append_link(&pcf->rq_pair, &ppair->fp_link, ppair);
    }

  return 0;
  }

