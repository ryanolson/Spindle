/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <argp.h>
#include <cstring>
#include <cassert>
#include <stdlib.h>
#include <string>
#include "config.h"
#include "ldcs_api_opts.h"

#if !defined(STR)
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

#define RELOCAOUT 'a'
#define COBO 'c'
#define DEBUG 'd'
#define PRELOAD 'e'
#define FOLLOWFORK 'f'
#define RELOCSO 'l'
#define NOCLEAN 'n'
#define LOCATION 'o'
#define PUSH 'p'
#define PULL 'q'
#define STRIP 's'
#define PORT 't'
#define RELOCEXEC 'x'
#define RELOCPY 'y'

#define GROUP_RELOC 1
#define GROUP_PUSHPULL 2
#define GROUP_NETWORK 3
#define GROUP_MISC 4

static const char *YESNO = "yes|no";

static const unsigned long all_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC | 
                                            OPT_RELOCPY | OPT_FOLLOWFORK;
static const unsigned long all_network_opts = OPT_COBO;
static const unsigned long all_pushpull_opts = OPT_PUSH | OPT_PULL;
static const unsigned long all_misc_opts = OPT_STRIP | OPT_DEBUG | OPT_PRELOAD | OPT_NOCLEAN;

static const unsigned long default_reloc_opts = OPT_RELOCAOUT | OPT_RELOCSO | OPT_RELOCEXEC | 
                                                OPT_RELOCPY | OPT_FOLLOWFORK;
static const unsigned long default_network_opts = OPT_COBO;
static const unsigned long default_pushpull_opts = OPT_PUSH;
static const unsigned long default_misc_opts = OPT_STRIP;

static unsigned long enabled_opts = 0;
static unsigned long disabled_opts = 0;

static char *preload_file;
static char **mpi_argv;
static int mpi_argc;
static bool done = false;

static unsigned int spindle_port = SPINDLE_PORT;
std::string spindle_location(SPINDLE_LOC);

struct argp_option options[] = {
   { "reloc-exec", RELOCAOUT, YESNO, 0, 
     "Relocate the main executable through Spindle. Default: yes", GROUP_RELOC },
   { "reloc-libs", RELOCSO, YESNO, 0,
     "Relocate shared libraries through Spindle. Default: yes", GROUP_RELOC },
   { "reloc-python", RELOCPY, YESNO, 0,
     "Relocate python modules (.py/.pyc) files when loaded via python. Default: yes", GROUP_RELOC },
   { "reloc-exec", RELOCEXEC, YESNO, 0,
     "Relocate the targets of exec/execv/execve/... calls. Default: yes", GROUP_RELOC },
   { "follow-fork", FOLLOWFORK, YESNO, 0,
     "Relocate objects in fork'd child processes. Default: yes", GROUP_RELOC },
   { "push", PUSH, NULL, 0,
     "Use a push model where objects loaded by any process are made available to all processes", GROUP_PUSHPULL },
   { "pull", PULL, NULL, 0,
     "Use a pull model where objects are only made available to processes that require them", GROUP_PUSHPULL },
   { "cobo", COBO, NULL, 0,
     "Use a tree-based cobo network for distributing objects", GROUP_NETWORK },
   { "port", PORT, "number", 0,
     "TCP Port for Spindle servers.  Default: " STR(SPINDLE_PORT), GROUP_NETWORK },
   { "location", LOCATION, "directory", 0,
     "Back-end directory for storing relocated files.  Should be a non-shared location such as a ramdisk.  Default: " SPINDLE_LOC, GROUP_NETWORK },
   { "debug", DEBUG, YESNO, 0,
     "Hide spindle from debuggers so they think libraries come from the original locations. " 
     "Enabling this disables reloc-exec.  Default: no", GROUP_MISC },
   { "preload", PRELOAD, "FILE", 0,
     "Provides a text file containing a white-space separated list of files that should be "
     "relocated to each node before execution begins", GROUP_MISC },
   { "strip", STRIP, YESNO, 0,
     "Strip debug and symbol information from binaries before distributing them. Default: yes", GROUP_MISC },
   { "noclean", NOCLEAN, YESNO, 0,
     "Don't remove local file cache after execution.  Default: no (removes the cache)\n", GROUP_MISC },
   {0}
};

static int opt_key_to_code(int key)
{
   switch (key) {
      case RELOCAOUT: return OPT_RELOCAOUT;
      case COBO: return OPT_COBO;
      case DEBUG: return OPT_DEBUG;
      case PRELOAD: return OPT_PRELOAD;
      case FOLLOWFORK: return OPT_FOLLOWFORK;
      case RELOCSO: return OPT_RELOCSO;
      case PUSH: return OPT_PUSH;
      case PULL: return OPT_PULL;
      case STRIP: return OPT_STRIP;
      case RELOCEXEC: return OPT_RELOCEXEC;
      case RELOCPY: return OPT_RELOCPY;
      case NOCLEAN: return OPT_NOCLEAN;
      default: return 0;
   }
}

static bool multi_bits_set(unsigned long v)
{
   return (v & (v - 1)) != 0;
}

static int parse(int key, char *arg, struct argp_state *vstate)
{
   struct argp_state *state = (struct argp_state *) vstate;
   struct argp_option *entry;
   int opt = 0;

   if (done && key != ARGP_KEY_END)
      return 0;

   for (entry = options; entry->key; entry++) {
      if (entry->key == key)
         break;
   }
   opt = opt_key_to_code(key);   

   if (entry->arg == YESNO) {
      if (strcmp(arg, "yes") == 0 || strcmp(arg, "y") == 0) {
         enabled_opts |= opt;
      }
      else if (strcmp(arg, "no") == 0 || strcmp(arg, "n") == 0)
         disabled_opts |= opt;
      else {
         argp_error(state, "%s must be 'yes' or 'no'", entry->name);
      }
      return 0;
   }
   else if (entry->key && entry->arg == NULL) {
      enabled_opts |= opt;
      return 0;
   }
   else if (entry->key == PRELOAD) {
      enabled_opts |= opt;
      preload_file = arg;
      return 0;
   }
   else if (entry->key == PORT) {
      spindle_port = atoi(arg);
      if (!spindle_port) {
         argp_error(state, "Port was given a 0 value");
      }
      return 0;
   }
   else if (entry->key == LOCATION) {
      spindle_location = arg;
      return 0;
   }
   else if (key == ARGP_KEY_ARG) {
      if (state->argc == 0) {
         return 0;
      }
      return ARGP_ERR_UNKNOWN;
   }
   else if (key == ARGP_KEY_ARGS) {
      mpi_argv = state->argv + state->next;
      mpi_argc = state->argc - state->next;
      done = true;
      return 0;
   }
   else if (key == ARGP_KEY_END) {
      if (enabled_opts & disabled_opts) {
         argp_error(state, "Cannot have the same option both enabled and disabled");
      }

      /* Set one and only one network option */
      unsigned long enabled_network_opts = enabled_opts & all_network_opts;
      if (multi_bits_set(enabled_network_opts)) {
         argp_error(state, "Cannot enable multiple network options");
      }
      opts |= enabled_network_opts ? enabled_network_opts : default_network_opts;

      /* Set one and only one push/pull option */
      unsigned long enabled_pushpull_opts = enabled_opts & all_pushpull_opts;
      if (multi_bits_set(enabled_pushpull_opts)) {
         argp_error(state, "Cannot enable both push and pull options");
      }
      opts |= enabled_pushpull_opts ? enabled_pushpull_opts : default_pushpull_opts;

      /* Set any reloc options */
      opts |= all_reloc_opts & ~disabled_opts & (enabled_opts | default_reloc_opts);

      /* Set any misc options */
      opts |= all_misc_opts & ~disabled_opts & (enabled_opts | default_misc_opts);
      if ((enabled_opts & OPT_DEBUG) && (enabled_opts & OPT_RELOCAOUT)) {
         argp_error(state, "Cannot enable both --debug and --reloc-exec");
      }
      if (opts & OPT_DEBUG)
         opts &= ~OPT_RELOCAOUT;

      return 0;
   }
   else if (key == ARGP_KEY_NO_ARGS) {
      argp_error(state, "No MPI command line found");
   }
   else {
      return 0;
   }
   assert(0);
   return -1;
}

unsigned long parseArgs(int argc, char *argv[])
{
   error_t result;
   argp_program_version = PACKAGE_VERSION;
   argp_program_bug_address = PACKAGE_BUGREPORT;
   
   done = false;
   struct argp arg_parser;
   bzero(&arg_parser, sizeof(arg_parser));
   arg_parser.options = options;
   arg_parser.parser = parse;
   arg_parser.args_doc = "[OPTIONS..] mpi_command";
 
   result = argp_parse(&arg_parser, argc, argv, ARGP_IN_ORDER, NULL, NULL);
   assert(result == 0);
   return opts;
}

char *getPreloadFile()
{
   return preload_file;
}

unsigned int getPort()
{
   return spindle_port;
}

const std::string getLocation(int number)
{
   char num_s[32];
   snprintf(num_s, 32, "%d", number);
   return spindle_location + std::string("/spindle.") + std::string(num_s);
}