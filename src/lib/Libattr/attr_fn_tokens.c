#include <pbs_config.h>   /* the master config generated by configure */

#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"

#include "log.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"

/*
 * decode_tokens - validate the token request
 *
 * Returns: 0 if ok
 *  >0 error number if error
 *  *patr elements set
 */

int decode_tokens(

  pbs_attribute *patr,
  const char   *name,  /* pbs_attribute name */
  const char *rescn, /* resource name, unused here */
  const char    *val,   /* pbs_attribute value */
  int            perm)  /* only used for resources */


  {
  int ret = 0;
  char * colon;
  float count;

  if (val != NULL)
    {
    ret = PBSE_BADATVAL; /* Assume bad until proven otherwise */
    colon = strstr((char *)val, ":");

    if (colon != NULL)
      {
      count = atof(++colon);

      if (count > 0.0 && count < 1000.0)
        {
        ret = 0;
        }
      }
    }

  if (ret == 0)
    {
    ret = decode_str(patr, name, rescn, val, perm);
    }

  return ret;
  }
