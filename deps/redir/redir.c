/* $Id$
 *
 * redir    - a utility for redirecting tcp connections
 *
 * Author:  Nigel Metheringham
 *          Nigel.Metheringham@ThePLAnet.net
 *
 * Based on, but much modified from, code originally written by
 * sammy@freenet.akron.oh.us - original header is below.
 *
 * redir is released under the GNU General Public license,
 * version 2, or at your discretion, any later version.
 *
 */

/* 
 * redir is currently maintained by Sam Creasey (sammy@oh.verio.com).
 * Please send patches, etc. there.
 *
 */

/* 980601: dl9sau
 * added some nice new features:
 *
 *   --bind_addr=my.other.ip.address
 *       forces to use my.other.ip.address for the outgoing connection
 *   
 *   you can also specify, that redir listens not on all IP addresses of
 *   your system but only for the given one, i.e.:
 *      if my host has the addresses
 *        irc.thishost.my.domain  and  mail.thishost.my.domain
 *      but you want that your users do connect for the irc redir service
 *      only on irc.thishost.my.domain, then do it this way:
 *        redir irc.fu-berlin.de irc.thishost.mydomain:6667 6667
 *   my need was that:
 *        addr1.first.domain  6667 redirects to irc.first.net  port 6667
 *   and  addr2.second.domain 6667 redirects to irc.second.net port 6667
 *   while addr1 and addr2 are the same maschine and the ports can be equal.
 *
 *  enjoy it!
 *    - thomas  <thomas@x-berg.in-berlin.de>, <dl9sau@db0tud.ampr.org>
 *
 *  btw: i tried without success implementing code for the following scenario:
 *    redir --force_addr irc.fu-berlin.de 6667 6667
 *  if "--force_addr" is given and a user connects to my system, that address
 *  of my system will be used on the outgoing connection that the user
 *  connected to.
 *  i was not successful to determine, to which of my addresses the user
 *  has connected.
 */
 
/* 990320 added support for ftp connection done by the client, now this code 
 *        should work for all ftp clients.
 *      
 *   - harald <harald.holzer@eunet.at>
 */
 
/* 991221 added options to simulate a slow connection and to limit
 *      bandwidth.
 *
 *   - Emmanuel Chantr�au <echant@maretmanu.org>
 */

#define  VERSION "2.2.1"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#ifdef USE_TCP_WRAPPERS
#include <tcpd.h>
#endif

#define debug(x)  if (dodebug) fprintf(stderr, x)
#define debug1(x,y)     if (dodebug) fprintf(stderr, x, y)

/* let's set up some globals... */
int dodebug = 0;
int dosyslog = 0;
unsigned char reuse_addr = 1;
unsigned char linger_opt = 0;
char * bind_addr = NULL;
struct sockaddr_in addr_out;
int timeout = 0;

#ifndef NO_FTP
int ftp = 0;
#endif

int transproxy = 0;

#ifndef NO_SHAPER
int max_bandwidth = 0;
int random_wait = 0;
int wait_in_out=3; /* bit 0: wait for "in", bit 1: wait for "out" */
int wait_in=1;
int wait_out=1;
#endif

unsigned int bufsize=4096;
char *connect_str = NULL;     /* CONNECT string passed to proxy */
char * ident = NULL;

#ifndef NO_FTP
/* what ftp to redirect */
#define FTP_PORT 1
#define FTP_PASV 2
#endif

#ifdef USE_TCP_WRAPPERS
struct request_info request;
int     allow_severity = LOG_INFO;
int     deny_severity = LOG_WARNING;
#endif /* USE_TCP_WRAPPERS */

#ifdef NEED_STRRCHR
#define strrchr rindex
#endif /* NEED_STRRCHR */

#define REDIR_IN 1
#define REDIR_OUT 0

/* prototype anything needing it */
void do_accept(int servsock, struct sockaddr_in *target);
int bindsock(char *addr, int port, int fail);

#ifndef NO_SHAPER
/* Used in this program to write something in a socket, it has the same
   parameters and return value as "write", but with the flag "in": true if
   it's the "in" socket and false if it's the "out" socket */
static inline ssize_t redir_write (int fd, const void *buf, size_t size, int in)
{
  ssize_t result;
  int wait;

  wait=in ? wait_in : wait_out;
  if( random_wait > 0 && wait) {
    fd_set empty;
    struct timeval waitbw; /* for bandwidth */
    int rand_time;
    
    FD_ZERO(&empty);

    rand_time=rand()%(random_wait*2);
    debug1("random wait: %u\n", rand_time);
    waitbw.tv_sec=rand_time/1000;
    waitbw.tv_usec=rand_time%1000;

    select (1, &empty, NULL, NULL, &waitbw);
  }

  result=write(fd, buf, size);

  if( max_bandwidth > 0 && wait) {
    fd_set empty;
    unsigned long bits;
    struct timeval waitbw; /* for bandwidth */

    FD_ZERO(&empty);

      /* wait to be sure tu be below the allowed bandwidth */
    bits=size*8;
    debug1("bandwidth wait: %lu\n", 1000*bits/max_bandwidth);
    waitbw.tv_sec=bits/max_bandwidth;
    waitbw.tv_usec=(1000*(bits%max_bandwidth))/max_bandwidth;

    select (1, &empty, NULL, NULL, &waitbw);
  }

  return result;
}
#else
/* macro if traffic shaper is disabled */
#define redir_write(fd, buf, size, in) write(fd, buf,size)
#endif


#ifdef NEED_STRDUP
char *
strdup(char * str)
{
      char * result;

      if (result = (char *) malloc(strlen(str) + 1))
            strcpy(result, str);

      return result;
}
#endif /* NEED_STRDUP */

void
redir_usage(char *name)
{
      fprintf(stderr,"usage:\n");
      fprintf(stderr, 
            "\t%s --lport=<n> --cport=<n> [options]\n", 
            name);
      fprintf(stderr, "\t%s --inetd --cport=<n>\n", name);
      fprintf(stderr, "\n\tOptions are:-\n");
      fprintf(stderr, "\t\t--lport=<n>\t\tport to listen on\n");
      fprintf(stderr, "\t\t--laddr=IP\t\taddress of interface to listen on\n");
      fprintf(stderr, "\t\t--cport=<n>\t\tport to connect to\n");
      fprintf(stderr, "\t\t--caddr=<host>\t\tremote host to connect to\n");
      fprintf(stderr, "\t\t--inetd\t\trun from inetd\n");
      fprintf(stderr, "\t\t--debug\t\toutput debugging info\n");
      fprintf(stderr, "\t\t--timeout=<n>\tset timeout to n seconds\n");
      fprintf(stderr, "\t\t--syslog\tlog messages to syslog\n");
      fprintf(stderr, "\t\t--name=<str>\ttag syslog messages with 'str'\n");
      fprintf(stderr, "\t\t--connect=<str>\tCONNECT string passed to proxy server\n");
#ifdef USE_TCP_WRAPPERS
      fprintf(stderr, "\t\t            \tAlso used as service name for TCP wrappers\n");
#endif /* USE_TCP_WRAPPERS */
      fprintf(stderr, "\t\t--bind_addr=IP\tbind() outgoing IP to given addr\n");

#ifndef NO_FTP
      fprintf(stderr, "\t\t--ftp=<type>\t\tredirect ftp connections\n");
      fprintf(stderr, "\t\t\twhere type is either port, pasv, both\n");
#endif

      fprintf(stderr, "\t\t--transproxy\trun in linux's transparent proxy mode\n");
#ifndef NO_SHAPER
        /* options for bandwidth */
        fprintf(stderr, "\t\t--bufsize=<octets>\tsize of the buffer\n");
        fprintf(stderr, "\t\t--maxbandwidth=<bit-per-sec>\tlimit the bandwidth\n");
        fprintf(stderr, "\t\t--random_wait=<millisec>\twait before each packet\n");
        fprintf(stderr, "\t\t--wait_in_out=<flag>\t1 wait for in, 2 out, 3 in&out\n");
        /* end options for bandwidth */
#endif
      fprintf(stderr, "\n\tVersion %s.\n", VERSION);
      exit(2);
}

void
parse_args(int argc,
         char * argv[],
         char ** target_addr,
         int * target_port,
           char ** local_addr,
         int * local_port,
         int * timeout,
         int * dodebug,
         int * inetd,
         int * dosyslog,
         char ** bind_addr,
#ifndef NO_FTP
         int * ftp,
#endif
         int *transproxy,
#ifndef NO_SHAPER
           unsigned int * bufsize,
           int * max_bandwidth,
           int * random_wait,
           int * wait_in_out,
#endif
         char **connect_str)
{
      static struct option long_options[] = {
            {"lport", required_argument, 0, 'l'},
            {"laddr", required_argument, 0, 'a'},
            {"cport", required_argument, 0, 'r'},
            {"caddr", required_argument, 0, 'c'},
            {"bind_addr", required_argument, 0, 'b'},
            {"debug",    no_argument,       0, 'd'},
            {"timeout",  required_argument, 0, 't'},
            {"inetd",    no_argument,       0, 'i'},
            {"ident",    required_argument, 0, 'n'},
            {"name",     required_argument, 0, 'n'},
            {"syslog",   no_argument,       0, 's'},
            {"ftp",      required_argument,       0, 'f'},
            {"transproxy", no_argument,     0, 'p'},
            {"connect", required_argument, 0, 'x'},
                {"bufsize",  required_argument,       0, 'z'},
                {"max_bandwidth",  required_argument,       0, 'm'},
                {"random_wait",  required_argument,       0, 'w'},
                {"wait_in_out",  required_argument,       0, 'o'},
            {0,0,0,0}         /* End marker */
      };
      
      int option_index = 0;
      extern int optind;
      int opt;
      struct servent *portdesc;
      char *lport = NULL;
      char *tport = NULL;
#ifndef NO_FTP
      char *ftp_type = NULL;
#endif
 
      *local_addr = NULL;
      *target_addr = NULL;
      *target_port = 0;
      *local_port = 0;

      while ((opt = getopt_long(argc, argv, "disfpn:t:b:a:l:r:c:x:z:m:w:o:", 
                          long_options, &option_index)) != -1) {
            switch (opt) {
            case 'x':
                  *connect_str = optarg;
                  break;
            case 'a':
                  *local_addr = optarg;
                  break;

            case 'l':
                  lport = optarg;
                  break;

            case 'r':
                  tport = optarg;
                  break;

            case 'c':
                  *target_addr = optarg;
                  break;

            case 'b':
                  *bind_addr = optarg;
                  break;

            case 'd':
                  (*dodebug)++;
                  break;

            case 't':
                  *timeout = atol(optarg);
                  break;

            case 'i':
                  (*inetd)++;
                  break;

            case 'n':
                  /* This is the ident which is added to syslog messages */
                  ident = optarg;
                  break;

            case 's':
                  (*dosyslog)++;
                  break;

#ifndef NO_FTP        
            case 'f':
                  ftp_type = optarg;
                  if(!ftp_type) {
                        redir_usage(argv[0]);
                        exit(1);
                  }
                  break;
#endif           

            case 'p':
                  (*transproxy)++;
                  break;

#ifndef NO_SHAPER
                case 'z':
                  *bufsize = (unsigned int)atol(optarg);
                  break;
 
                case 'm':
                  *max_bandwidth = atol(optarg);
                  break;
 
                case 'w':
                  *random_wait = atol(optarg);
                  break;
 
                case 'o':
                  *wait_in_out = atol(optarg);
                  wait_in=*wait_in_out & 1;
                  wait_out=*wait_in_out & 2;
                  break;
#endif 
            default:
                  redir_usage(argv[0]);
                  exit(1);
                  break;
            }
      }

      if(tport == NULL)
      {
            redir_usage(argv[0]);
            exit(1);
      }

      if ((portdesc = getservbyname(tport, "tcp")) != NULL) {
            *target_port = ntohs(portdesc->s_port);
      } else {
            *target_port = atol(tport);
      }
    
      /* only check local port if not running from inetd */
      if(!(*inetd)) {
            if(lport == NULL)
            {
                  redir_usage(argv[0]);
                  exit(1);
            }
       
            if ((portdesc = getservbyname(lport, "tcp")) != NULL) 
                  *local_port = ntohs(portdesc->s_port);
            else
                  *local_port = atol(lport);
      } /* if *inetd */

      if (!ident) {
            if ((ident = (char *) strrchr(argv[0], '/'))) {
                  ident++;
            } else {
                  ident = argv[0];
            }
      }

#ifndef NO_FTP
      /* some kind of ftp being forwarded? */
      if(ftp_type) {
            if(!strncasecmp(ftp_type, "port", 4)) 
                  *ftp = FTP_PORT;
            else if(!strncasecmp(ftp_type, "pasv", 4))
                  *ftp = FTP_PASV;
            else if(!strncasecmp(ftp_type, "both", 4))
                  *ftp = FTP_PORT | FTP_PASV;
            else {
                  redir_usage(argv[0]);
                  exit(1);
            }
      }
#endif            
    
      openlog(ident, LOG_PID, LOG_DAEMON);

      return;
}

#ifndef NO_FTP
/* with the --ftp option, this one changes passive mode replies from
   the ftp server to point to a new redirector which we spawn,
   now it also change the PORT commando when the client accept the
   dataconnection */
   
void ftp_clean(int send, char *buf, unsigned long *bytes, int ftpsrv)
{

      char *port_start;
      int rporthi, lporthi;
      int lportlo, rportlo;
      int lport, rport;
      int remip[4];
      int localsock;
      int socksize = sizeof(struct sockaddr_in);

      struct sockaddr_in newsession;
      struct sockaddr_in sockname;

      if (ftpsrv == 0)
      {
            /* is this a port commando ? */
            if(strncmp(buf, "PORT", 4)) {
                  redir_write(send, buf, (*bytes), REDIR_OUT);
                  return;
            }
            /* parse the old address out of the buffer */
            port_start = strchr(buf, ' ');

            sscanf(port_start, " %d,%d,%d,%d,%d,%d", &remip[0], &remip[1],
                   &remip[2], &remip[3], &rporthi, &rportlo);
      } else {
            /* is this a passive mode return ? */
            if(strncmp(buf, "227", 3)) {
                  redir_write(send, buf, (*bytes), REDIR_OUT);
                  return;
            }
            
            /* parse the old address out of the buffer */
            port_start = strchr(buf, '(');
            
            sscanf(port_start, "(%d,%d,%d,%d,%d,%d", &remip[0], &remip[1],
                   &remip[2], &remip[3], &rporthi, &rportlo);
      }
    
      /* get the outside interface so we can listen */
      if(getsockname(send, (struct sockaddr *)&sockname, &socksize) != 0) {
            perror("getsockname");
            exit(1);
      }

      rport = (rporthi << 8) | rportlo;

      /* we need to listen on a port for the incoming connection.
         we will use the port 0, so let the system pick one. */

      localsock = bindsock(inet_ntoa(sockname.sin_addr), 0, 1);

      
      /* get the real info */
      if(getsockname(localsock, (struct sockaddr *)&sockname, &socksize) < 0) {
            perror("getsockname");
            if (dosyslog)
                  syslog(LOG_ERR, "getsockname failed: %m");
            exit(1);
      }

      lport = ntohs(sockname.sin_port);

      lporthi=(lport >> 8 ) & 0xff;
      lportlo=lport & 0xff;

      /* check to see if we bound */
      if(localsock == -1) {
            fprintf(stderr, "ftp: unable to bind new listening address\n");
            exit(1);
      }
      if (ftpsrv == 0) {
            /* send the new port and ipaddress to the server */
            (*bytes) = sprintf(buf, "PORT %d,%d,%d,%d,%d,%d\n",
                           sockname.sin_addr.s_addr & 0xff, 
                           (sockname.sin_addr.s_addr >> 8) & 0xff, 
                           (sockname.sin_addr.s_addr >> 16) & 0xff,
                           sockname.sin_addr.s_addr >> 24, lporthi, lportlo);
      } else {
            /* send the new port and ipaddress to the client */
            (*bytes) = sprintf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\n",
                           sockname.sin_addr.s_addr & 0xff, 
                           (sockname.sin_addr.s_addr >> 8) & 0xff, 
                           (sockname.sin_addr.s_addr >> 16) & 0xff,
                           sockname.sin_addr.s_addr >> 24, lporthi, lportlo);
      }
      newsession.sin_port = htons(rport);
      newsession.sin_family = AF_INET;
      newsession.sin_addr.s_addr = remip[0] | (remip[1] << 8)
            | (remip[2] << 16) | (remip[3] << 24);

      debug1("ftpdata server ip: %s\n", inet_ntoa(newsession.sin_addr));
      debug1("ftpdata server port: %d\n", rport);
      debug1("listening for ftpdata on port %d\n", lport);
      debug1("listening for ftpdata on addr %s\n", inet_ntoa(sockname.sin_addr));


      /* now that we're bound and listening, we can safely send the new
         string without fear of them getting a connection refused. */
      redir_write(send, buf, (*bytes), REDIR_OUT);     

      /* make a new process to handle the dataconnection correctly,
         for the PASV mode this isn't a problem because after sending the 
         PASV command, the data connection, get active. For the PORT command
         the server must send a success, if starting here with the copyloop
         the success command never arrive the client.*/
      
      switch(fork())
      {
      case -1: /* Error */
            syslog(LOG_ERR, "Couldn't fork: %m");
            _exit(1);
      case 0:  /* Child */
      {
            /* turn off ftp checking while the data connection is active */
            ftp = 0;
            do_accept(localsock, &newsession);
            close(localsock);
            _exit(0);
      }
      default: /* Parent */
      { close(localsock); }
      }
      return;
}
#endif


void
copyloop(int insock, 
       int outsock,
       int timeout_secs)
{
      fd_set iofds;
      fd_set c_iofds;
      int max_fd;             /* Maximum numbered fd used */
      struct timeval timeout;
      unsigned long bytes;
      unsigned long bytes_in = 0;
      unsigned long bytes_out = 0;
      unsigned int start_time, end_time;
      char buf[bufsize];

      /* Record start time */
      start_time = (unsigned int) time(NULL);

      /* Set up timeout */
      timeout.tv_sec = timeout_secs;
      timeout.tv_usec = 0;

      /* file descriptor bits */
      FD_ZERO(&iofds);
      FD_SET(insock, &iofds);
      FD_SET(outsock, &iofds);

    
      if (insock > outsock) {
            max_fd = insock;
      } else {
            max_fd = outsock;
      }

      debug1("Entering copyloop() - timeout is %d\n", timeout_secs);
      while(1) {
            (void) memcpy(&c_iofds, &iofds, sizeof(iofds));


            if (select(max_fd + 1,
                     &c_iofds,
                     (fd_set *)0,
                     (fd_set *)0,
                     (timeout_secs ? &timeout : NULL)) <= 0) {
                  /*        syslog(LLEV,"connection timeout: %d sec",timeout.tv_sec);*/
                  break;
            }

            if(FD_ISSET(insock, &c_iofds)) {
                  if((bytes = read(insock, buf, sizeof(buf))) <= 0)
                        break;
#ifndef NO_FTP
                  if (ftp & FTP_PORT)
                        /* if we're correcting FTP, lookup for a PORT commando
                           in the buffer, if yes change this and establish 
                           a new redirector for the data */
                        ftp_clean(outsock, buf, &bytes,0); 
                  else
#endif
                        if(redir_write(outsock, buf, bytes, REDIR_OUT) != bytes)
                              break;
                  bytes_out += bytes;
            }
            if(FD_ISSET(outsock, &c_iofds)) {
                  if((bytes = read(outsock, buf, sizeof(buf))) <= 0)
                        break;
                  /* if we're correcting for PASV on ftp redirections, then
                     fix buf and bytes to have the new address, among other
                     things */
#ifndef NO_FTP
                  if(ftp & FTP_PASV)
                        ftp_clean(insock, buf, &bytes,1);
                  else 
#endif
                        if(redir_write(insock, buf, bytes, REDIR_IN) != bytes)
                              break;
                  bytes_in += bytes;
            }
      }
      debug("Leaving main copyloop\n");

/*
  setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
  setsockopt(insock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(SO_LINGER)); 
  setsockopt(outsock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
  setsockopt(outsock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(SO_LINGER)); 
*/

      shutdown(insock,0);
      shutdown(outsock,0);
      close(insock);
      close(outsock);
      debug("copyloop - sockets shutdown and closed\n");
      end_time = (unsigned int) time(NULL);
      debug1("copyloop - connect time: %8d seconds\n", end_time - start_time);
      debug1("copyloop - transfer in:  %8ld bytes\n", bytes_in);
      debug1("copyloop - transfer out: %8ld bytes\n", bytes_out);
      if (dosyslog) {
            syslog(LOG_NOTICE, "disconnect %d secs, %ld in %ld out",
                   (end_time - start_time), bytes_in, bytes_out);
      }
      return;
}

void doproxyconnect(int socket)
{
      char buf[128];
      int x;
      /* write CONNECT string to proxy */
      sprintf((char *) &buf, "CONNECT %s HTTP/1.0\n\n", connect_str);
      x = write(socket, (char *) &buf, strlen(buf));
      if (x < 1) {
            perror("doproxyconnect: failed");
            exit(1);
      }
      /* now read result */
      x = read(socket, (char *) &buf, sizeof(buf));
      if (x < 1) {
            perror("doproxyconnect: failed reading fra proxy");
            exit(1);
      }
      /* no more error checking for now -- something should be added later */
      /* HTTP/1.0 200 Connection established */
}


/* lwait for a connection and move into copyloop...  again,
   ftp redir will call this, so we don't dupilcate it. */

void
do_accept(int servsock, struct sockaddr_in *target)
{

      int clisock;
      int targetsock;
      struct sockaddr_in client;
      int clientlen = sizeof(client);
      int accept_errno;
     
      debug("top of accept loop\n");
      if ((clisock = accept(servsock, (struct  sockaddr  *) &client, 
                        &clientlen)) < 0) {

            accept_errno = errno;
            perror("server: accept");

            if (dosyslog)
                  syslog(LOG_ERR, "accept failed: %m");

            /* determine if this error is fatal */
            switch(accept_errno) {
                  /* non-fatal errors */
            case EHOSTUNREACH:
            case ECONNRESET:
            case ETIMEDOUT:
                  return;

                  /* all other errors assumed fatal */
            default:
                  exit(1);
            }

      }
     
      debug1("peer IP is %s\n", inet_ntoa(client.sin_addr));
      debug1("peer socket is %d\n", client.sin_port);

      /*
       * Double fork here so we don't have to wait later
       * This detaches us from our parent so that the parent
       * does not need to pick up dead kids later.
       *
       * This needs to be done before the hosts_access stuff, because
       * extended hosts_access options expect to be run from a child.
       */
      switch(fork())
      {
      case -1: /* Error */
            perror("(server) fork");

            if (dosyslog)
                  syslog(LOG_ERR, "(server) fork failed: %m");

            _exit(1);
      case 0:  /* Child */
            break;
      default: /* Parent */
      {
            int status;
        
            /* Wait for child (who has forked off grandchild) */
            (void) wait(&status);

            /* Close sockets to prevent confusion */
            close(clisock);
      
            return;
      }
      }

      /* We are now the first child. Fork again and exit */
        
      switch(fork())
      {
      case -1: /* Error */
            perror("(child) fork");

            if (dosyslog)
                  syslog(LOG_ERR, "(child) fork failed: %m");

            _exit(1);
      case 0:  /* Child */
            break;
      default: /* Parent */
            _exit(0);
      }
     
      /* We are now the grandchild */

#ifdef USE_TCP_WRAPPERS
      request_init(&request, RQ_DAEMON, ident, RQ_FILE, clisock, 0);
      sock_host(&request);
      sock_hostname(&request);
      sock_hostaddr(&request);

      if (!hosts_access(&request)) {
            refuse(&request);
            _exit(0);
      }

      if (dosyslog)
            syslog(LOG_INFO, "accepted connect from %s", eval_client(&request));
#endif /* USE_TCP_WRAPPERS */

      if ((targetsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        
            perror("target: socket");
        
            if (dosyslog)
                  syslog(LOG_ERR, "socket failed: %m");
            
            _exit(1);
      }

      if(transproxy) {
            memcpy(&addr_out, &client, sizeof(struct sockaddr_in));
            addr_out.sin_port = 0;
      }
          
      if (bind_addr || transproxy) {
            /* this only makes sense if an outgoing IP addr has been forced;
             * at this point, we have a valid targetsock to bind() to.. */
            /* also, if we're in transparent proxy mode, this option
               never makes sense */

            if (bind(targetsock, (struct  sockaddr  *) &addr_out, 
                   sizeof(struct sockaddr_in)) < 0) {
                  perror("bind_addr: cannot bind to forcerd outgoing addr");

/* the port parameter fetch the really port we are listening, it should
   only be different if the input value is 0 (let the system pick a 
   port) */
                  if (dosyslog)
                        syslog(LOG_ERR, "bind failed: %m");

                  _exit(1);
            }
            debug1("outgoing IP is %s\n", inet_ntoa(addr_out.sin_addr));
      }

      if (connect(targetsock, (struct  sockaddr  *) target, 
                sizeof(struct sockaddr_in)) < 0) {
            perror("target: connect");

            if (dosyslog)
                  syslog(LOG_ERR, "bind failed: %m");

            _exit(1);
      }
     
      debug1("connected to %s\n", inet_ntoa(target->sin_addr));

      /* thanks to Anders Vannman for the fix to make proper syslogging
         happen here...  */

      if (dosyslog) {
            char tmp1[20], tmp2[20];
            strcpy(tmp1, inet_ntoa(client.sin_addr));
            strcpy(tmp2, inet_ntoa(target->sin_addr));
        
            syslog(LOG_NOTICE, "connecting %s/%d to %s/%d",
                   tmp1, client.sin_port,
                   tmp2, target->sin_port);
      }

      /* do proxy stuff */
      if (connect_str)
            doproxyconnect(targetsock);

#ifndef NO_SHAPER
        /* initialise random number if necessary */
        if( random_wait > 0 ) {
          srand(getpid());
        }
#endif

      copyloop(clisock, targetsock, timeout);
      exit(0);    /* Exit after copy */
}

/* bind to a new socket, we do this out here because passive-fixups
   are going to call it too, and there's no sense dupliciting the
   code. */
/* fail is true if we should just return a -1 on error, false if we
   should bail. */

int bindsock(char *addr, int port, int fail) 
{

      int servsock;
      struct sockaddr_in server;
     
      /*
       * Get a socket to work with.  This socket will
       * be in the Internet domain, and will be a
       * stream socket.
       */
     
      if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            if(fail) {
                  return -1;
            }
            else {
                  perror("server: socket");

                  if (dosyslog)
                        syslog(LOG_ERR, "socket failed: %m");

                  exit(1);
            }
      }

      memset(&server, 0, sizeof(server));
      server.sin_family = AF_INET;
      server.sin_port = htons(port);
      if (addr != NULL) {
            struct hostent *hp;
        
            debug1("listening on %s\n", addr);
            if ((hp = gethostbyname(addr)) == NULL) {
                  fprintf(stderr, "%s: cannot resolve hostname.\n", addr);
                  exit(1);
            }
            memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
      } else {
            debug("local IP is default\n");
            server.sin_addr.s_addr = htonl(inet_addr("0.0.0.0"));
      }
     
      setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
      setsockopt(servsock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(SO_LINGER)); 
     
      /*
       * Try to bind the address to the socket.
       */
     
      if (bind(servsock, (struct  sockaddr  *) &server, 
             sizeof(server)) < 0) {
            if(fail) {
                  close(servsock);
                  return -1;
            } else {
                  perror("server: bind");

                  if (dosyslog)
                        syslog(LOG_ERR, "bind failed: %m");

                  exit(1);
            }
      }
     
      /*
       * Listen on the socket.
       */
     
      if (listen(servsock, 10) < 0) {
            if(fail) {
                  close(servsock);
                  return -1;
            } else {
                  perror("server: listen");

                  if (dosyslog)
                        syslog(LOG_ERR, "listen failed: %m");

                  exit(1);
            }
      }
     
      return servsock;
}

int
main(int argc, char *argv[])
{

      struct sockaddr_in target;
      char *target_addr;
      int target_port;
      char *local_addr;
      int local_port;
      int inetd = 0;
      char * target_ip;
      char * ip_to_target;

      debug("parse args\n");
      parse_args(argc, argv, &target_addr, &target_port, &local_addr, 
               &local_port, &timeout, &dodebug, &inetd, &dosyslog, &bind_addr,
#ifndef NO_FTP
               &ftp, 
#endif
               &transproxy, 
#ifndef NO_SHAPER
               &bufsize, &max_bandwidth, &random_wait,
               &wait_in_out,
#endif
                   &connect_str);

      /* Set up target */
      target.sin_family = AF_INET;
      target.sin_port = htons(target_port);
      if (target_addr != NULL) {
            struct hostent *hp;

            debug1("target is %s\n", target_addr);
            if ((hp = gethostbyname(target_addr)) == NULL) {
                  fprintf(stderr, "%s: host unknown.\n", target_addr);
                  exit(1);
            }
            memcpy(&target.sin_addr, hp->h_addr, hp->h_length);
      } else {
            debug("target is default\n");
            target.sin_addr.s_addr = htonl(inet_addr("0.0.0.0"));
      }

      target_ip = strdup(inet_ntoa(target.sin_addr));
      debug1("target IP address is %s\n", target_ip);
      debug1("target port is %d\n", target_port);

      /* Set up outgoing IP addr (optional);
       * we have to wait for bind until targetsock = socket() is done
       */
      if (bind_addr && !transproxy) {
            struct hostent *hp;

            fprintf(stderr, "bind_addr is %s\n", bind_addr);
            addr_out.sin_family = AF_INET;
            addr_out.sin_port = 0;
            if ((hp = gethostbyname(bind_addr)) == NULL) {
                  fprintf(stderr, "%s: cannot resolve forced outgoing IP address.\n", bind_addr);
                  exit(1);
            }
            memcpy(&addr_out.sin_addr, hp->h_addr, hp->h_length);

            ip_to_target = strdup(inet_ntoa(addr_out.sin_addr));
            debug1("IP address for target is %s\n", ip_to_target);
      }
           

      if (inetd) {
            int targetsock;
            struct sockaddr_in client;
            int client_size = sizeof(client);

#ifdef USE_TCP_WRAPPERS
            request_init(&request, RQ_DAEMON, ident, RQ_FILE, 0, 0);
            sock_host(&request);
            sock_hostname(&request);
            sock_hostaddr(&request);
      
            if (!hosts_access(&request))
                  refuse(&request);
#endif /* USE_TCP_WRAPPERS */

            if (!getpeername(0, (struct sockaddr *) &client, &client_size)) {
                  debug1("peer IP is %s\n", inet_ntoa(client.sin_addr));
                  debug1("peer socket is %d\n", client.sin_port);
            }
            if ((targetsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                  perror("target: socket");

                  if (dosyslog)
                        syslog(LOG_ERR, "targetsock failed: %m");

                  exit(1);
            }

            if(transproxy) {
                  memcpy(&addr_out, &client, sizeof(struct sockaddr_in));
                  addr_out.sin_port = 0;
            }

            if (bind_addr || transproxy) {
                  /* this only makes sense if an outgoing IP addr has been forced;
                   * at this point, we have a valid targetsock to bind() to.. */
                  if (bind(targetsock, (struct  sockaddr  *) &addr_out, 
                         sizeof(addr_out)) < 0) {
                        perror("bind_addr: cannot bind to forcerd outgoing addr");
                         
                        if (dosyslog)
                              syslog(LOG_ERR, "bind failed: %m");
                         
                        exit(1);
                  }
                  debug1("outgoing IP is %s\n", inet_ntoa(addr_out.sin_addr));
            }

            if (connect(targetsock, (struct sockaddr *) &target, 
                      sizeof(target)) < 0) {
                  perror("target: connect");

                  if (dosyslog)
                        syslog(LOG_ERR, "connect failed: %m");

                  exit(1);
            }

            if (dosyslog) {
                  syslog(LOG_NOTICE, "connecting %s/%d to %s/%d",
                         inet_ntoa(client.sin_addr), client.sin_port,
                         target_ip, target.sin_port);
            }

            /* Just start copying - one side of the loop is stdin - 0 */
            copyloop(0, targetsock, timeout);
      } else {
            int servsock;
      
            if(local_addr)
                  servsock = bindsock(local_addr, local_port, 0);
            else
                  servsock = bindsock(NULL, local_port, 0);

            /*
             * Accept connections.  When we accept one, ns
             * will be connected to the client.  client will
             * contain the address of the client.
             */

            while (1) 
                  do_accept(servsock, &target);
      }

      /* this should really never be reached */

      exit(0);

}