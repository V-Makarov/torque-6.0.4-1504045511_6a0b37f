#include "license_pbs.h" /* See here for the software license */
#include "trq_auth_daemon.h"
#include <pbs_config.h>   /* the master config generated by configure */
#include <sys/stat.h> /* umask */

#include <stdlib.h> /* calloc, free */
#include <stdio.h> /* printf */
#include <string.h> /* strcat */
#include <pthread.h> /* threading functions */
#include <errno.h> /* errno */
#include <syslog.h> /* openlog and syslog */
#include <unistd.h> /* getgid, fork */
#include <grp.h> /* setgroups */
#include <ctype.h> /*isspace */
#include <getopt.h> /*getopt_long */
#include <sys/types.h>
#include <pwd.h>   /* getuid */
#include <string>
#include <iostream>
#include <sstream>
#include "pbs_error.h" /* PBSE_NONE */
#include "pbs_constants.h" /* AUTH_IP */
#include "pbs_ifl.h" /* pbs_default, PBS_BATCH_SERVICE_PORT, TRQ_AUTHD_SERVICE_PORT */
#include "net_connect.h" /* TRQAUTHD_SOCK_NAME */
#include "../lib/Libnet/lib_net.h" /* start_listener */
#include "lib_ifl.h" /* process_svr_conn */
#include "../lib/Liblog/chk_file_sec.h" /* IamRoot */
#include "../lib/Liblog/pbs_log.h" /* logging stuff */
#include "../include/log.h"  /* log events and event classes */
#include "csv.h" /*csv_nth() */
#include "pbs_helper.h"


#define MAX_BUF 1024
#define TRQ_LOGFILES "client_logs"

extern char *msg_daemonname;
extern int debug_mode;
extern pbs_net_t trq_server_addr;
extern char *trq_hostname;

bool        down_server = false;
bool        use_log = true;
bool        daemonize_server = true;
static int  changed_msg_daem = 0;
std::string active_pbs_server;

/* Get the name of the active pbs_server */
int load_trqauthd_config(

  std::string  &default_server_name,
  int          *t_port,
  char        **trqauthd_unix_domain_port)

  {
  int rc = PBSE_NONE;
  char *tmp_name = NULL;
  int  unix_domain_len;

  tmp_name = pbs_default();

  if ((tmp_name == NULL) ||
      (tmp_name[0] == '\0'))
    rc = PBSE_BADHOST;
  else
    {
    /* Currently this only display's the port for the trq server
     * from the lib_ifl.h file or server_name file (The same way
     * the client utilities determine the pbs_server port)
     */
    printf("hostname: %s\n", tmp_name);
    default_server_name = tmp_name;
    PBS_get_server(tmp_name, (unsigned int *)t_port);
    if (*t_port == 0)
      *t_port = PBS_BATCH_SERVICE_PORT;

    unix_domain_len = strlen(TRQAUTHD_SOCK_DIR);
    unix_domain_len += strlen(TRQAUTHD_SOCK_NAME) + 2; /* on for the "/" and one for zero termination*/
    *trqauthd_unix_domain_port = (char *)malloc(unix_domain_len);
    if (*trqauthd_unix_domain_port == NULL)
      {
      fprintf(stderr, "could not allocate memory for unix domain port");
      return(PBSE_MEM_MALLOC);
      }

    strcpy(*trqauthd_unix_domain_port, TRQAUTHD_SOCK_DIR);
    strcat(*trqauthd_unix_domain_port, "/");
    strcat(*trqauthd_unix_domain_port, TRQAUTHD_SOCK_NAME);
    set_active_pbs_server(tmp_name, *t_port);
    }

  return rc;
  }

int load_ssh_key(
    char ** UNUSED(ssh_key) )
  {
  int rc = PBSE_NONE;
  return rc;
  }



void initialize_globals_for_log(const char *port)
  {
  strcpy(pbs_current_user, "trqauthd");   
  if ((msg_daemonname = strdup(pbs_current_user)))
    changed_msg_daem = 1;
  log_set_hostname_sharelogging(active_pbs_server.c_str(), port);
  }

int init_trqauth_log(const char *server_port)
  {
  const char *path_home = PBS_SERVER_HOME;
  int eventclass = PBS_EVENTCLASS_TRQAUTHD;
  char path_log[MAXPATHLEN + 1];
  char *log_file=NULL;
  char  error_buf[MAX_BUF];
  int  rc;

  rc = log_init(NULL, NULL);
  if (rc != PBSE_NONE)
    return(rc);
  if (use_log == false)
    {
    rc = PBSE_NONE;
    return(rc);
    }
  log_get_set_eventclass(&eventclass, SETV);

  initialize_globals_for_log(server_port);
  sprintf(path_log, "%s/%s", path_home, TRQ_LOGFILES);
  if ((mkdir(path_log, 0755) == -1) && (errno != EEXIST))
    {
       openlog("daemonize_trqauthd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
       syslog(LOG_ALERT, "Failed to create client_logs directory: %s errno: %d error message: %s", path_log, errno, strerror(errno));
       sprintf(error_buf,"Failed to create client_logs directory: %s, error message: %s",path_log,strerror(errno));
       log_err(errno,__func__,error_buf);
       closelog();
       return(PBSE_SYSTEM);
    }
    pthread_mutex_lock(&log_mutex);
    rc = log_open(log_file, path_log);
    pthread_mutex_unlock(&log_mutex);

    return(rc);

  }


int daemonize_trqauthd(const char *server_ip, const char *server_port, void *(*process_meth)(void *))
  {
  int gid;
  pid_t pid;
  int   rc;
  char  error_buf[MAX_BUF];
  char msg_trqauthddown[MAX_BUF];
  char unix_socket_name[MAXPATHLEN + 1];
  const char *path_home = PBS_SERVER_HOME;

  umask(022);

  gid = getgid();
  /* secure supplemental groups */
  if(setgroups(1, (gid_t *)&gid) != 0)
    {
    fprintf(stderr, "Unable to drop secondary groups. Some MAC framework is active?\n");
    snprintf(error_buf, sizeof(error_buf),
                     "setgroups(group = %lu) failed: %s\n",
                     (unsigned long)gid, strerror(errno));
    fprintf(stderr, "%s\n", error_buf);
    return(1);
    }

  /* change working directory to PBS_SERVER_HOME */
  if (chdir(path_home) == -1)
    {
    sprintf(error_buf, "unable to change to directory %s", path_home);

    log_err(-1, __func__, error_buf);

    return(1);
    }

  if (getenv("PBSDEBUG") != NULL)
    daemonize_server = false;
  if (daemonize_server)
    {
    pid = fork();
    if(pid > 0)
      {
      /* parent. We are done */
      return(0);
      }
    else if (pid < 0)
      {
      /* something went wrong */
      fprintf(stderr, "fork failed. errno = %d\n", errno);
      return(PBSE_RMSYSTEM);
      }
    else
      {
      fprintf(stderr, "trqauthd daemonized - port %s\n", server_port);
      /* If I made it here I am the child */
      fclose(stdin);
      fclose(stdout);
      fclose(stderr);
      /* We closed 0 (stdin), 1 (stdout), and 2 (stderr). fopen should give us
         0, 1 and 2 in that order. this is a UNIX practice */
      if (fopen("/dev/null", "r") == NULL)
        perror(__func__);

      if (fopen("/dev/null", "r") == NULL)
        perror(__func__);

      if (fopen("/dev/null", "r") == NULL)
        perror(__func__);
      }
    }
  else
    {
    fprintf(stderr, "trqauthd port: %s\n", server_port);
    }

    /* start the listener */
    snprintf(unix_socket_name, sizeof(unix_socket_name), "%s/%s", TRQAUTHD_SOCK_DIR, TRQAUTHD_SOCK_NAME);
    rc = start_domainsocket_listener(unix_socket_name, process_meth);
    if(rc != PBSE_NONE)
      {
      openlog("daemonize_trqauthd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
      syslog(LOG_ALERT, "trqauthd could not start: %d\n", rc);
      log_err(rc, "daemonize_trqauthd", (char *)"trqauthd could not start");
      pthread_mutex_lock(&log_mutex);
      log_close(1);
      pthread_mutex_unlock(&log_mutex);
      if (changed_msg_daem && msg_daemonname) 
        {
          free(msg_daemonname);
        }
      exit(-1);
      }
    snprintf(msg_trqauthddown, sizeof(msg_trqauthddown),
      "TORQUE authd daemon shut down and no longer listening on IP:port %s:%s",
      server_ip, server_port);
    log_record(PBSEVENT_SYSTEM | PBSEVENT_FORCE, PBS_EVENTCLASS_TRQAUTHD,
      msg_daemonname, msg_trqauthddown);
    pthread_mutex_lock(&log_mutex);
    log_close(1);
    pthread_mutex_unlock(&log_mutex);
    if (changed_msg_daem && msg_daemonname)
      {
      free(msg_daemonname);
      }
    exit(0);
  }


void parse_command_line(int argc, char **argv)
  {
  int c;
  int option_index = 0;
  int iterator;
  static struct option long_options[] = {
            {"about",   no_argument,      0,  0 },
            {"help",    no_argument,      0,  0 },
            {"version", no_argument,      0,  0 },
            {0,         0,                0,  0 }
  };

  while ((c = getopt_long(argc, argv, "DdFn", long_options, &option_index)) != -1)
    {
    switch (c)
      {
      case 0:
        switch (option_index)  /* One of the long options was passed */
          {
          case 0:   /*about*/
            fprintf(stderr, "torque user authorization daemon version %s\n", VERSION);
            exit(0);
            break;
          case 1:   /* help */
            iterator = 0;
            fprintf(stderr, "Usage: trqauthd [FLAGS]\n");
            while (long_options[iterator].name != 0)
              {
              fprintf(stderr, "  --%s\n", long_options[iterator++].name);
              }
            fprintf(stderr, "\n  -D // RUN IN DEBUG MODE\n");
            fprintf(stderr, "  -d // terminate trqauthd\n");
            fprintf(stderr, "  -F // do not fork (use when running under systemd)\n");
            fprintf(stderr, "\n");
            exit(0);
            break;
          case 2:   /* version */
            fprintf(stderr, "Version: %s \nCommit: %s\n", VERSION, GIT_HASH);
            exit(0);
            break;
          }
        break;

      case 'D':
        daemonize_server = false;
        break;

      case 'd':
        down_server = true;
        break;
       
      case 'F':
        // use when running under systemd
        daemonize_server = false;
        break;

       case 'n':

         use_log=false;
         fprintf(stderr, "trqauthd logging disabled\n");

         break;

      default:
        fprintf(stderr, "Unknown command line option\n");
        exit(1);
        break;
      }
    } 
  }

int terminate_trqauthd()
  {
  int rc = PBSE_NONE;
  int sock = -1;
  char write_buf[MAX_LINE];
  char *read_buf = NULL;
  char log_buf[MAX_BUF];
  long long read_buf_len = MAX_LINE;
  long long ret_code;
  uid_t     myrealuid;
  pid_t     mypid;
  struct passwd *pwent;

  myrealuid = getuid();
  pwent = getpwuid(myrealuid);
  if (pwent == NULL)
    {
    snprintf(log_buf, MAX_BUF, "cannot get account info: uid %d, errno %d (%s)\n", (int)myrealuid, errno, strerror(errno));
    log_event(PBSEVENT_ADMIN, PBS_EVENTCLASS_SERVER, __func__, log_buf);
    return(PBSE_SYSTEM);
    }

  mypid = getpid();
  sprintf(write_buf, "%d|%d|%s|%d|", TRQ_DOWN_TRQAUTHD, (int )strlen(pwent->pw_name), pwent->pw_name, mypid);

  if((rc = connect_to_trqauthd(&sock)) != PBSE_NONE)
    {
    fprintf(stderr, "Could not connect to trqauthd. trqauthd may already be down\n");
    }
  else if ((rc = socket_write(sock, write_buf, strlen(write_buf))) < 0)
    {
    fprintf(stderr, "Failed to send termnation request to trqauthd: %d\n", rc);
    }
  else if ((rc = socket_read_num(sock, &ret_code)) != PBSE_NONE)
    {
    fprintf(stderr, "trqauthd did not give proper response. Check to see if traquthd has terminated: %d\n", rc);
    }
  else if (ret_code != PBSE_NONE)
    {
    rc = socket_read_str(sock, &read_buf, &read_buf_len);
    fprintf(stderr, "trqauthd not shutdown. %s\n", read_buf);
    }
  else if ((rc = socket_read_str(sock, &read_buf, &read_buf_len)) != PBSE_NONE)
    {
    fprintf(stderr, "trqauthd did not respond. Check to see if trqauthd has terminated: %d\n", rc);
    }
  else if( (rc = connect_to_trqauthd(&sock)) != PBSE_NONE) /* We do this because the accept loop on trqauthd 
                                                             is still waiting for a command before it realizes 
                                                             it is terminated */
    {
    fprintf(stdout, "\ntrqauthd has been terminated\n");
    }
  else
    {
    fprintf(stdout, "\ntrqauthd has been terminated\n");
    }

  if (sock != -1)
    close(sock);

  if (read_buf != NULL)
    {
    free(read_buf);
    }

  return(rc);
  }


extern "C"
{
int trq_main(

  int    argc,
  char **argv,
  char ** UNUSED(envp))

  {
  int rc = PBSE_NONE;
  char *the_key = NULL;
  char *sign_key = NULL;
  int trq_server_port = 0;
  char *daemon_port = NULL;
  void *(*process_method)(void *) = process_svr_conn;

  parse_command_line(argc, argv);

  if (IamRoot() == 0)
    {
    printf("This program must be run as root!!!\n");
    return(PBSE_IVALREQ);
    }

  rc = set_trqauthd_addr();
  if (rc != PBSE_NONE)
    return(rc);

  if (down_server == true)
    {
    rc = terminate_trqauthd();
    return(rc);
    }

  if ((rc = load_trqauthd_config(active_pbs_server, &trq_server_port, &daemon_port)) != PBSE_NONE)
    {
    fprintf(stderr, "Failed to load configuration. Make sure the $TORQUE_HOME/server_name file exists\n");
    }
  else if ((rc = check_trqauthd_unix_domain_port(daemon_port)) != PBSE_NONE)
    {
    fprintf(stderr, "trqauthd unix domain file %s already bound.\n trqauthd may already be running \n", daemon_port);
    }
  else if ((rc = load_ssh_key(&the_key)) != PBSE_NONE)
    {
    }
  else if((rc = init_trqauth_log(daemon_port)) != PBSE_NONE)
    {
    fprintf(stderr, "ERROR: Failed to initialize trqauthd log\n");
    }
  else if ((rc = validate_server(active_pbs_server, trq_server_port, the_key, &sign_key)) != PBSE_NONE)
    {
    }
  else if ((rc = daemonize_trqauthd(AUTH_IP, daemon_port, process_method)) == PBSE_NONE)
    {
    }
  else
    {
    printf("Daemon exit requested\n");
    }

  if (the_key != NULL)
    free(the_key);

  if (daemon_port != NULL)
    free(daemon_port);

  return rc;
  }
}