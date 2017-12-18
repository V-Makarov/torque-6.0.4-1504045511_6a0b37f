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

#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portability.h"
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"
#include "pbs_job.h"
#include "pbs_helper.h"

#include "complete_req.hpp"
#include "pbs_nodes.h"

/*
 * This file contains general functions for manipulating an pbs_attribute array.
 * Included is:
 * attr_atomic_set()
 * attr_atomic_node_set()
 * attr_atomic_kill()
 *
 * The prototypes are declared in "attr_func.h"
 */


/*
 * attr_atomic_set - atomically set a pbs_attribute array with values from
 * an svrattrl
*/

int attr_atomic_set(

  struct svrattrl *plist,
  pbs_attribute   *old,
  pbs_attribute   *new_attr,
  attribute_def   *pdef,
  int              limit,
  int              unkn,
  int              privil,
  int             *badattr)

  {
  int           acc;
  int           index;
  int           listidx;
  resource      *prc;
  int            rc;
  pbs_attribute  temp;
  int            resc_access_perm = privil; /* set privilege for decode_resc() */

  for (index = 0;index < limit;index++)
    clear_attr(new_attr + index, pdef + index);

  listidx = 0;

  rc = PBSE_NONE;

  while (plist != NULL)
    {
    listidx++;

    if ((index = find_attr(pdef, plist->al_name, limit)) < 0)
      {
      if (unkn < 0)
        {
        /*unknown attr isn't allowed*/
        rc =  PBSE_NOATTR;

        break;
        }
      else
        {
        index = unkn;  /* if unknown attr are allowed */
        }
      }

    /* have we privilege to set the pbs_attribute ? */

    acc = (pdef + index)->at_flags & ATR_DFLAG_ACCESS;

    if ((acc & privil & ATR_DFLAG_WRACC) == 0)
      {
      if (privil & ATR_DFLAG_SvWR)
        {
        /* from a daemon, just ignore this pbs_attribute */

        plist = (svrattrl *)GET_NEXT(plist->al_link);

        continue;
        }

      /* from user, error if can't write pbs_attribute */

      rc = PBSE_ATTRRO;

      break;
      }

    /* decode new_attr value */

    clear_attr(&temp, pdef + index);

    /* 
     * special gpu cases
     * 1) if only ncpus is specified, delete gpus resource if any
     * 2) if both ncpus and gpus specified, replace both
     */
    
    if ((strcmp(plist->al_name,ATTR_l) == 0) &&
      (strcmp(plist->al_resc,"ncpus") == 0))
      {
      char      *pc;
      if ((pc = strstr(plist->al_value,":gpus=")) != NULL)
        {
        /* save off gpu resource list then add new resource_list entry for it */
        char *gpuval;

        gpuval = strdup(pc+6);

        (*pc) = '\0';

        if (gpuval != NULL)
          {
          rc = (pdef + index)->at_decode(&temp, plist->al_name, (char *)"gpus",
              gpuval,ATR_DFLAG_ACCESS);

          free(gpuval);
          if (rc != 0)
            {
            if ((rc == PBSE_UNKRESC) && (unkn > 0))
              rc = 0; /* ignore the "error" */
            else
              break;
            }
          }
        }
      else
        {
        /* delete old resource_list.gpus value if any.
         * this can be done by setting it to zero
         */
        rc = (pdef + index)->at_decode(&temp, plist->al_name, (char *)"gpus",
            0,ATR_DFLAG_ACCESS);
        if (rc != 0)
          {
          if ((rc == PBSE_UNKRESC) && (unkn > 0))
            rc = 0; /* ignore the "error" */
          else
            break;
          }
        }
      }

    rc = (pdef + index)->at_decode(&temp, plist->al_name, plist->al_resc, plist->al_value,resc_access_perm);
    if (rc != 0)
      {
      if ((rc == PBSE_UNKRESC) && (unkn > 0))
        rc = 0; /* ignore the "error" */
      else
        break;
      }

    /* duplicate current value, if set AND not already dup-ed */

    if (((old + index)->at_flags & ATR_VFLAG_SET) &&
        !((new_attr + index)->at_flags & ATR_VFLAG_SET))
      {
      if ((rc = (pdef + index)->at_set(new_attr + index, old + index, SET)) != 0)
        break;

      /*
       * we need to know if the value is changed during
       * the next step, so clear MODIFY here; including
       * within resources.
       */

      (new_attr + index)->at_flags &= ~ATR_VFLAG_MODIFY;

      if ((new_attr + index)->at_type == ATR_TYPE_RESC)
        {
        prc = (resource *)GET_NEXT((new_attr + index)->at_val.at_list);

        while (prc)
          {
          prc->rs_value.at_flags &= ~ATR_VFLAG_MODIFY;
          prc = (resource *)GET_NEXT(prc->rs_link);
          }
        }
      }

    /* update new copy with temp, MODIFY is set on ones changed */

    if ((plist->al_op != INCR) && (plist->al_op != DECR) &&
        (plist->al_op != SET) && (plist->al_op != INCR_OLD))
      {
      plist->al_op = SET;
      }

    if (temp.at_flags & ATR_VFLAG_SET)
      {
      if ((rc = (pdef + index)->at_set(new_attr + index, &temp, plist->al_op)) != 0)
        {
        (pdef + index)->at_free(&temp);

        break;
        }
      }
    else if (temp.at_flags & ATR_VFLAG_MODIFY)
      {
      (pdef + index)->at_free(new_attr + index);

      (new_attr + index)->at_flags |= ATR_VFLAG_MODIFY;
      }

    (pdef + index)->at_free(&temp);

    if (plist->al_link.ll_next == NULL)
      break;

    plist = (struct svrattrl *)GET_NEXT(plist->al_link);
    } /* END while (plist != NULL) */

  if (rc != 0)
    {
    *badattr = listidx;

    for (index = 0; index < limit; index++)
      (pdef + index)->at_free(new_attr + index);

    return(rc);
    }

  return(0);
  }  /* END attr_atomic_set() */




/*
 * attr_atomic_node_set - atomically set an pbs_attribute array with
 * values from an svrattrl
*/

int attr_atomic_node_set(

  struct svrattrl *plist,    /* list of pbs_attribute modif structs */
  pbs_attribute   * UNUSED(old),
  pbs_attribute   *new_attr,      /* new pbs_attribute array begins here */
  attribute_def   *pdef,     /* begin array  definition structs */
  int              limit,    /* number elts in definition array */
  int              unkn,     /* <0 unknown attrib not permitted */
  int              privil,   /* requester's access privileges   */
  int             *badattr,  /* return list position where bad   */
  bool             dont_update_nodes_file)

  {
  int           acc;
  int           index;
  int           listidx;
  int           rc = 0;
  pbs_attribute temp;

  listidx = 0;

  while (plist)
    {
    /*Traverse loop for each client entered pbs_attribute*/

    listidx++;

    if ((index = find_attr(pdef, plist->al_name, limit)) < 0)
      {
      if (unkn < 0)         /*if unknown attr not allowed*/
        {
        rc =  PBSE_NOATTR;
        break;
        }
      else
        index = unkn;  /*if unknown attr are allowed*/
      }
    else if (dont_update_nodes_file == true)
      {
      // These attributes cannot edit the nodes file
      if ((index == ND_ATR_properties) ||
          (index == ND_ATR_ntype) ||
          (index == ND_ATR_ttl) ||
          (index == ND_ATR_acl) ||
          (index == ND_ATR_requestid))
        {
        rc = PBSE_CANT_EDIT_NODES;
        break;
        }
      }


    /* The name of the pbs_attribute is in the definitions list*/
    /* Now, have we privilege to set the pbs_attribute ?       */
    /* Check access capabilities specified in the attrdef  */
    /* against the requestor's privilege level        */

    acc = (pdef + index)->at_flags & ATR_DFLAG_ACCESS;

    if ((acc & privil & ATR_DFLAG_WRACC) == 0)
      {
      if (privil & ATR_DFLAG_SvWR)
        {
        /*  from a daemon, just ignore this pbs_attribute */
        plist = (struct svrattrl *)GET_NEXT(plist->al_link);
        continue;
        }
      else
        {
        /*from user, no write access to pbs_attribute     */
        rc = PBSE_ATTRRO;
        break;
        }
      }

    /*decode new value*/

    clear_attr(&temp, pdef + index);

    if ((rc = (pdef + index)->at_decode(&temp, plist->al_name,
                                        plist->al_resc, plist->al_value,0)) != 0)
      {
      if ((rc == PBSE_UNKRESC) && (unkn > 0))
        rc = 0;              /*ignore the "error"*/
      else
        break;
      }

    /*update "new" with "temp", MODIFY is set on "new" if changed*/

    (new_attr + index)->at_flags &= ~ATR_VFLAG_MODIFY;

    if ((plist->al_op != INCR) && (plist->al_op != DECR) &&
        (plist->al_op != SET) && (plist->al_op != INCR_OLD))
      plist->al_op = SET;


    if (temp.at_flags & ATR_VFLAG_SET)
      {
      /* "temp" has a data value, use it to update "new" */

      if ((rc = (pdef + index)->at_set(new_attr + index, &temp, plist->al_op)) != 0)
        {
        (pdef + index)->at_free(&temp);
        break;
        }
      }
    else if (temp.at_flags & ATR_VFLAG_MODIFY)
      {

      (pdef + index)->at_free(new_attr + index);
      (new_attr + index)->at_flags |= ATR_VFLAG_MODIFY;
      }

    (pdef + index)->at_free(&temp);
    plist = (struct svrattrl *)GET_NEXT(plist->al_link);
    }

  if (rc != 0)
    {

    /*"at_free" functions get invoked by upstream caller*/
    /*invoking attr_atomic_kill() on the array of       */
    /*node-pbs_attribute structs-- any hanging structs are  */
    /*freed and then the node-pbs_attribute array is freed  */

    *badattr = listidx;   /*the svrattrl that gave a problem*/
    }

  return (rc);
  }



/*
 * attr_atomic_kill - kill (free) a temporary pbs_attribute array which
 * was set up by attr_atomic_set().
 *
 * at_free() is called on each element on the array, then
 * the array itself is freed.
 */

void attr_atomic_kill(
    
  pbs_attribute *temp,
  attribute_def *pdef,
  int            limit)

  {
  int i;

  for (i = 0; i < limit; i++)
    (pdef + i)->at_free(temp + i);

  (void)free(temp);
  }