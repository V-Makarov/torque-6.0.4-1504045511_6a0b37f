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

#include <sys/types.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "pbs_job.h"
#include "pbs_error.h"
#include "pbs_helper.h"

#define HOLD_ENCODE_SIZE 3

/*
 * This file contains special decode and encode functions for the hold-types
 * pbs_attribute.  All other functions for this pbs_attribute are the standard
 * _b (boolean) routines.
 *
 * decode_hold - decode string into hold pbs_attribute
 *
 * Returns: 0 if 0k
 *  >0 error number if error
 *  *patr members set
 */

int decode_hold(

  pbs_attribute *patr,
  const char * UNUSED(name),  /* pbs_attribute name */
  const char * UNUSED(rescn),  /* resource name - unused here */
  const char    *val,  /* pbs_attribute value */
  int          UNUSED(perm)) /* only used for resources */

  {
  const char  *pc;

  patr->at_val.at_long = 0;

  if ((val != (char *)0) && (strlen(val) > (size_t)0))
    {
    for (pc = val; *pc != '\0'; pc++)
      {
      switch (*pc)
        {

        case 'n':
          patr->at_val.at_long = HOLD_n;
          break;

        case 'u':
          patr->at_val.at_long |= HOLD_u;
          break;

        case 'o':
          patr->at_val.at_long |= HOLD_o;
          break;

        case 's':
          patr->at_val.at_long |= HOLD_s;
          break;

        case 'a':
          patr->at_val.at_long |= HOLD_a;
          break;

        case 'l':
          patr->at_val.at_long |= HOLD_l;
          break;

        default:
          return (PBSE_BADATVAL);
        }
      }

    patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    }
  else
    {
    patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) |
                     ATR_VFLAG_MODIFY;
    }

  return (0);
  }

/*
 * encode_str - encode pbs_attribute of type ATR_TYPE_STR into attr_extern
 *
 * Returns: >0 if ok
 *   =0 if no value, so no link added to list
 *   <0 if error
 */
/*ARGSUSED*/

int encode_hold(

  pbs_attribute  *attr,   /* ptr to pbs_attribute */
  tlist_head     *phead,  /* head of attrlist */
  const char    *atname, /* name of pbs_attribute */
  const char    *rsname, /* resource name or null */
  int            UNUSED(mode),   /* encode mode, unused here */
  int            UNUSED(perm))   /* only used for resources */


  {
  int       i;
  svrattrl *pal;

  if (!attr)
    return (-1);

  if (!(attr->at_flags & ATR_VFLAG_SET))
    return (0);

  pal = attrlist_create(atname, rsname, HOLD_ENCODE_SIZE + 1);

  if (pal == (svrattrl *)0)
    return (-1);

  i = 0;

  if (attr->at_val.at_long == 0)
    *(pal->al_value + i++) = 'n';
  else
    {
    if (attr->at_val.at_long & HOLD_s)
      *(pal->al_value + i++) = 's';

    if (attr->at_val.at_long & HOLD_o)
      *(pal->al_value + i++) = 'o';

    if (attr->at_val.at_long & HOLD_u)
      *(pal->al_value + i++) = 'u';

    if (attr->at_val.at_long & HOLD_a)
      *(pal->al_value + i++) = 'a';

    if (attr->at_val.at_long & HOLD_l)
      *(pal->al_value + i++) = 'l';
    }

  pal->al_flags = attr->at_flags;

  append_link(phead, &pal->al_link, pal);

  return (1);
  }

/*
 * comp_hold - compare two attributes of type hold
 *
 * Returns: +1 if 1st != 2nd
 *    0 if 1st == 2nd
 */

int comp_hold(
   
  pbs_attribute *attr,
  pbs_attribute *with)

  {
  if (!attr || !with)
    return -1;

  if (attr->at_val.at_long == with->at_val.at_long)
    return 0;
  else
    return 1;
  }
