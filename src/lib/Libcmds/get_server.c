#include "license_pbs.h" /* See here for the software license */
/*
 * get_server
 *
 * ------------------------------------
 * As specified in section 5 of the ERS:
 *
 *  5.1.2.  Directing Requests to Correct Server
 *
 *  A  command  shall  perform  its  function  by  sending   the
 *  corresponding  request  for  service  to the a batch server.
 *  The choice of batch servers to which to send the request  is
 *  governed by the following ordered set of rules:
 *
 *  1. For those commands which require or accept a job identif-
 *     ier  operand, if the server is specified in the job iden-
 *     tifier operand as @server, then the batch  requests  will
 *     be sent to the server named by server.
 *
 *  2. For those commands which require or accept a job identif-
 *     ier  operand  and  the @server is not specified, then the
 *     command will attempt to determine the current location of
 *     the  job  by  sending  a  Locate Job batch request to the
 *     server which created the job.
 *
 *  3. If a server component of a destination  is  supplied  via
 *     the  -q  option,  such  as  on  qsub and qselect, but not
 *     qalter, then the server request is sent to that server.
 *
 *  4. The server request is sent to the  server  identified  as
 *     the default server, see section 2.6.3.
 *     [pbs_connect() implements this]
 *
 *  2.6.3.  Default Server
 *
 *  When a server is not specified to a client, the client  will
 *  send  batch requests to the server identified as the default
 *  server.  A client identifies the default server by  (a)  the
 *  setting  of  the environment variable PBS_DEFAULT which con-
 *  tains a destination, or (b) the  destination  in  the  batch
 *  administrator established file {PBS_DIR}/server_name.
 * ------------------------------------
 *
 * Takes a job_id_in string as input, calls parse_jobid to separate
 * the pieces, then applies the above rules in order
 * If things go OK, the function value is set to 0,
 * if errors, it is set to 1.
 *
 * Full legal syntax is:
 *  seq_number[.parent_server[:port]][@current_server[:port]]
 *
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <vector>
#include <string>

#include "pbs_ifl.h"
#include "pbs_error.h"
#include "net_connect.h"

#define notNULL(x) (((x)!=NULL) && (strlen(x)>(size_t)0))

extern int parse_jobid(const char *, char **, char **, char **);



int TShowAbout_exit(void)

  {
  char *dserver;
  char *servervar;

  char  tmpLine[1024];

  dserver = pbs_default();

  servervar = getenv("PBS_DEFAULT");

  strcpy(tmpLine, PBS_DEFAULT_FILE);

  tmpLine[strlen(tmpLine) - strlen("/pbs_server") - 1] = '\0';

  fprintf(stderr, "HomeDir:   %s  InstallDir: %s  Server: %s%s\n",
          tmpLine,
          PBS_INSTALL_DIR,
          dserver,
          (servervar != NULL) ? " (PBS_DEFAULT is set)" : "");

  fprintf(stderr, "BuildDir:  %s\n", PBS_SOURCE_DIR);

  fprintf(stderr, "BuildUser: %s\n", PBS_BUILD_USER);

  fprintf(stderr, "BuildHost: %s\n", PBS_BUILD_HOST);

  fprintf(stderr, "BuildDate: %s\n", PBS_BUILD_DATE);

  fprintf(stderr, "Version:   %s\n", PACKAGE_VERSION);

  fprintf(stderr, "Commit:  %s\n", GIT_HASH);

  exit(0);
  }  /* END TShowAbout_exit() */



int get_server_and_job_ids(

  const char               *job_id_in,
  std::vector<std::string> &potential_ids,
  std::string              &server_out)

  {
  char *seq_number = NULL;
  char *parent_server = NULL;
  char *current_server = NULL;
  char  def_server[PBS_MAXSERVERNAME + 1];
  char  full_servername[PBS_MAXSERVERNAME + 1];
  char *c;

  /* parse the job_id_in into components */

  if (!strcasecmp("all",job_id_in))
    {
    potential_ids.push_back(job_id_in);
    server_out[0] = '\0';
    }
  else
    {
    if (parse_jobid(job_id_in, &seq_number, &parent_server, &current_server))
      {
      return(PBSE_BAD_PARAMETER);
      }
    
    /* Apply the above rules, in order, except for the locate job request.
     * That request is only sent if the job is not found on the local server. */
    
    if (notNULL(current_server))
      {
      /* @server found */
      server_out = current_server;
      }
    else
      {
      /* can't locate a server, so return a NULL to tell pbs_connect to use default */
      server_out[0] = '\0';
      }
    
    if (!notNULL(parent_server))
      {
      parent_server = pbs_default();

      if (notNULL(parent_server))
        {
        char *colon = strchr(parent_server, ':');

        if (colon != NULL)
          *colon = '\0';

        snprintf(def_server, PBS_MAXSERVERNAME, "%s", parent_server);
        
        c = def_server;
        
        while ((*c != '\n') && (*c != '\0'))
          c++;
        
        *c = '\0';
        }
      }

    if (!notNULL(parent_server))
      {
      return(PBSE_SERVER_NOT_FOUND);
      }

    std::string jid_out(seq_number);
    jid_out += ".";
    jid_out += parent_server;

    potential_ids.push_back(jid_out);

    if (get_fullhostname(parent_server, full_servername, PBS_MAXSERVERNAME, NULL) == 0)
      {
      if (strcmp(parent_server, full_servername))
        {
        // If this isn't the full name, add that to the list
        jid_out = seq_number;
        jid_out += ".";
        jid_out += full_servername;
        potential_ids.push_back(jid_out);
        }
      }
    }
  
  return(PBSE_NONE);
  } // END get_server_and_job_ids()



/**
 * Take a job ID string and parse it into
 * job ID and server name parts.
 *
 * @param job_id_in Input string of the form <job number>[.<parent server name>][@<host server url>]
 * @param job_id_out Output string containing only the job ID part of the input specification.
 * @param server_out Output string containing nothing or a host url if specified.
 */
int get_server(

  const char *job_id_in,       /* read only */
  char       *job_id_out,      /* write only */
  int         job_id_out_size, /* sizeof the out buffer */
  char       *server_out,      /* write only */
  int         server_out_size) /* sizeof the out buffer */

  {
  char *seq_number = NULL;
  char *parent_server = NULL;
  char *current_server = NULL;
  char def_server[PBS_MAXSERVERNAME + 1];
  char *c;

  /* parse the job_id_in into components */

  if (!strcasecmp("all",job_id_in))
    {
    snprintf(job_id_out, job_id_out_size, "%s", job_id_in);
    server_out[0] = '\0';
    }
  else
    {
    if (parse_jobid(job_id_in, &seq_number, &parent_server, &current_server))
      {
      return(PBSE_BAD_PARAMETER);
      }
    
    /* Apply the above rules, in order, except for the locate job request.
     * That request is only sent if the job is not found on the local server. */
    
    if (notNULL(current_server))
      {
      /* @server found */
      snprintf(server_out, server_out_size, "%s", current_server);
      }
    else
      {
      /* can't locate a server, so return a NULL to tell pbs_connect to use default */
      server_out[0] = '\0';
      }
    
    if (notNULL(parent_server))
      {
      if (notNULL(current_server))
        {
        snprintf(job_id_out, job_id_out_size, "%s.%s", seq_number, parent_server);
        /* parent_server might not be resolvable if current_server specified */
        }
      else
        {
        snprintf(job_id_out, job_id_out_size, "%s.%s", seq_number, parent_server);
        }
      
      if ((c = strchr(parent_server, ':')) != 0)
        {
        if (*(c - 1) == '\\')
          c--;
        
        snprintf(job_id_out + strlen(job_id_out), job_id_out_size - strlen(job_id_out), "%s", c);
        }
      }
    else
      {
      parent_server = pbs_default();
      
      if ((parent_server == (char *)NULL) || (*parent_server == '\0'))
        {
        return(PBSE_SERVER_NOT_FOUND);
        }

      snprintf(def_server, PBS_MAXSERVERNAME, "%s", parent_server);
      
      c = def_server;
      
      while ((*c != '\n') && (*c != '\0'))
        c++;
      
      *c = '\0';
      
      snprintf(job_id_out, job_id_out_size, "%s.%s", seq_number, def_server);
      }    /* END else */
    }
  
  return(PBSE_NONE);
  }  /* END get_server() */

/* END get_server.c */

