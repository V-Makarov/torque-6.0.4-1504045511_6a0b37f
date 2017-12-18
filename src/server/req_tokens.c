#include <pbs_config.h>   /* the master config generated by configure */

#include "libpbs.h"
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "queue.h"
#include "credential.h"
#include "batch_request.h"
#include "net_connect.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"
#include "pbs_job.h"
#include "pbs_nodes.h"

#include <stdio.h>

/*
 * manager_oper_chk - check the @host part of a manager or operator acl
 * entry to insure it is fully qualified.  This is to prevent
 * input errors when setting the list.
 * This is the at_action() routine for the server attributes
 * "managers" and "operators"
 */

int token_chk(
    
  pbs_attribute *pattr, 
  void          *pobject, 
  int            actmode)
  
  {
  char   *entry;
  int    err = 0;
  float                   count = 0;
  int       i;

  struct array_strings *pstr;

  if (actmode == ATR_ACTION_FREE)
    return (0); /* no checking on free */

  if ((pstr = pattr->at_val.at_arst) == (struct array_strings *)0)
    return (0);

  for (i = 0; i < pstr->as_usedptr; ++i)
    {
    entry = strchr(pstr->as_string[i], (int)':');

    if (entry == (char *)0)
      {
      err = PBSE_IVALREQ;
      break;
      }

    entry++;

    count = atof(entry);

    if (count <= 0 || count > 1000)
      {
      err = PBSE_IVALREQ;
      }
    }

  return (err);
  }

int compare_tokens(
    
  char *token1,
  char *token2)

  {
  char *entry1;
  char *entry2;
  int ret = 0;
  int size1 = 0;
  int size2 = 0;

  entry1 = strstr(token1, ":");
  entry2 = strstr(token2, ":");

  size1 = entry1 - token1;
  size2 = entry2 - token2;

  if ((entry1 != NULL)
      && (entry2 != NULL)
      && (size1 == size2)
      && (strncmp(token1, token2, size1) == 0))
    {
    ret = 1;
    }

  return ret;
  }

/*
 * chk_dup_acl - check for duplicate in list (array_strings)
 * Return 0 if no duplicate, 1 if duplicate within the new list or
 * between the new and old list.
 */

static int chk_dup_token(
    
  struct array_strings *old,
  struct array_strings *new_string)

  {
  int i;
  int j;

  int ret = 0;

  if (new_string != NULL && old != NULL)
    {
    for (i = 0; i < new_string->as_usedptr; ++i)
      {

      /* first check against self */

      for (j = 0; j < new_string->as_usedptr; ++j)
        {

        if (i != j)
          {
          if (compare_tokens(new_string->as_string[i], new_string->as_string[j]) != 0)
            {
            ret = 1;
            break;
            }
          }
        }

      for (j = 0; j < old->as_usedptr; ++j)
        {
        if (compare_tokens(new_string->as_string[i], old->as_string[j]) != 0)
          {
          ret = 1;
          break;
          }
        }

      }
    }

  /* next check new against existing (old) strings */

  return ret;
  }


int set_tokens(
    
  pbs_attribute *attr, 
  pbs_attribute *new_attr, 
  enum batch_op  op)

  {
  struct array_strings *pas;
  struct array_strings *newpas;

  pas = attr->at_val.at_arst; /* array of strings control struct */
  newpas = new_attr->at_val.at_arst; /* array of strings control struct */

  switch (op)
    {

    case SET:

    case INCR:

      if (chk_dup_token(pas, newpas) != 0)
        {
        return PBSE_DUPLIST;
        }

      break;

    case DECR:
      break;

    default:
      return (PBSE_INTERNAL);
    }

  return (set_arst(attr, new_attr, op));
  }

