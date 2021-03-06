/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

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

#include <stdlib.h>
#include <assert.h>
#include "daemon_ids.h"

int biterd_num_compute_nodes()
{
   return 1;
}

int biterd_ranks_in_cn(int cn_id)
{
   char *proc_s = getenv("PROCS");
   if (proc_s)
      return atoi(proc_s);
      
   assert(0);
}

int biterd_unique_num_for_cn(int cn_id)
{
   return 0;
}

int biterd_get_rank(int compute_node_id, int client_id)
{
   return client_id;
}

int biterd_register_rank(int session_id, uint32_t client_id, uint32_t rank)
{
   return 0;
}
