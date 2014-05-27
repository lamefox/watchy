#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "watchy.h"

static void print_help    (const char *);
static void print_version (const char *);

static void
print_help (const char * arg)
{
  printf ("Usage: %s [options] name:pid name:pid...\nOptions:\n", arg);
  printf ("\t--help|-h\tPrint this help\n");
  printf ("\t--key|-k\tKey to send host stats to\n");
  printf ("\t--version|-v\tPrint version string\n");
  printf ("\t--port|-p\tPort of server\n");
  printf ("\t--hostname|-b\tHostname of server\n");
  printf ("\n");
}

static void
print_version (const char * arg)
{
  printf ("%s version %s\n", arg, PACKAGE_STRING);
}

int main (int argc, char **argv)
{
  int c, port = 0;
  char * bind = NULL, * key = NULL;
  while (1)
    {
      static struct option long_options [] = {
        { "help",     no_argument,       0, 'h' },
        { "version",  no_argument,       0, 'v' },
        { "hostname", required_argument, 0, 'b' },
        { "port",     required_argument, 0, 'p' },
	{ "key",      required_argument, 0, 'k' },
        { 0, 0, 0, 0 }
      };
      int option_index = 0;
      c = getopt_long (argc, argv, "hvb:p:i:k:", long_options, &option_index);
      if (c == -1)
        break;

      switch (c)
        {
        case 'h':
          print_help (*argv);
          return 0;

        case 'v':
          print_version (*argv);
          return 0;

        case 'b':
          bind = strdup (optarg);
          break;

        case 'p':
          port = atoi (optarg);
          break;

	case 'k':
	  key = strdup (optarg);
	  break;

        default:
          break;
        }
    }

  if ((bind == NULL) || (port == 0))
    {
      fprintf (stderr, "Stats destination bind and port unset see --help\n");
      return -1;
    }

  if (optind >= argc)
    {
      fprintf (stderr, "No proccess to watch will attempt to watch host if key is specified\n");
    }

  size_t offs = 0, plen = argc - optind;
  pid_t pids [plen];
  memset (&pids, 0, sizeof (pids));

  while (optind < argc)
    {
      const char * pair = argv [optind++];

      size_t len = strlen (pair);
      char key [len];
      char value [len];
      memset (key, 0, len);
      memset (value, 0, len);

      char * p = strchr (pair, ':');
      size_t poffs = p - pair + 1;
      if (poffs >= len || poffs == 0)
	{
	  fprintf (stderr, "Pid pair [%s] seems invalid\n", pair);
	  continue;
	}

      strncpy (key, pair, poffs - 1);
      strncpy (value, pair + poffs, len - poffs);
      const pid_t ipid = atoi (value);

      printf ("Trying to watch pid [%i] posting to [udp://%s@%s:%i]\n",
	      ipid, key, bind, port);
      int rkill = kill (ipid, 0);
      if (rkill != 0)
	{
	  fprintf (stderr, "pid [%i] invalid [%s]\n", ipid, strerror (errno));
	  continue;
	}

      pids [offs] = fork ();
      switch (pids [offs])
	{
	case -1:
	  fprintf (stderr, "Error forking watcher for %i\n", ipid);
	  break;

	case 0:
	  {
	    int retval = watchy_watchpid (key, bind, port, ipid);
	    if (retval != WTCY_NO_ERROR)
	      fprintf (stderr, "Error watching pid %i - %s\n",
		       ipid, watchy_strerror (retval));
	    exit (0);
	  }
	  break;

	default:
	  break;
	}
      offs++;
    }

  if (key != NULL)
    watchy_watchHost (key, bind, port);
  free (bind);
  free (key);

  // don't care about error conditions here
  size_t i;
  for (i = 0; i < plen; ++ i)
    waitpid (pids [i], NULL, 0);
  return 0;
}