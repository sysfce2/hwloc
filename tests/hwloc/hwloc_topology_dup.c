/*
 * Copyright © 2011-2024 Inria.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include "hwloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

int main(void)
{
  static hwloc_topology_t oldtopology, topology;
  hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
  struct hwloc_distances_s *distances;
  hwloc_obj_t nodes[3], cores[6];
  hwloc_uint64_t node_distances[9], core_distances[36];
  hwloc_distances_add_handle_t handle;
  unsigned i,j,nr;
  int err;

  hwloc_topology_init(&oldtopology);
  hwloc_topology_set_synthetic(oldtopology, "node:3 core:2 pu:4");
  hwloc_topology_load(oldtopology);

  for(i=0; i<3; i++) {
    nodes[i] = hwloc_get_obj_by_type(oldtopology, HWLOC_OBJ_NUMANODE, i);
    for(j=0; j<3; j++)
      node_distances[i*3+j] = (i == j ? 10 : 20);
  }
  handle = hwloc_distances_add_create(oldtopology, NULL,
                                      HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER,
                                      0);
  assert(handle);
  err = hwloc_distances_add_values(oldtopology, handle, 3, nodes, node_distances, 0);
  assert(!err);
  err = hwloc_distances_add_commit(oldtopology, handle,
                                   HWLOC_DISTANCES_ADD_FLAG_GROUP);
  assert(!err);

  for(i=0; i<6; i++) {
    cores[i] = hwloc_get_obj_by_type(oldtopology, HWLOC_OBJ_CORE, i);
    for(j=0; j<6; j++)
      core_distances[i*6+j] = (i == j ? 4 : 8);
  }
  handle = hwloc_distances_add_create(oldtopology, NULL,
                                      HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER,
                                      0);
  assert(handle);
  err = hwloc_distances_add_values(oldtopology, handle, 6, cores, core_distances, 0);
  assert(!err);
  err = hwloc_distances_add_commit(oldtopology, handle,
                                   HWLOC_DISTANCES_ADD_FLAG_GROUP);
  assert(!err);

  printf("duplicating\n");
  err = hwloc_topology_dup(&topology, oldtopology);
  assert(!err);
  printf("destroying the old topology\n");
  hwloc_topology_destroy(oldtopology);

  /* remove the entire third node */
  printf("removing one node\n");
  hwloc_bitmap_fill(cpuset);
  hwloc_bitmap_clr_range(cpuset, 16, 23);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_REMOVE_CPULESS);
  assert(!err);
  printf("checking the result\n");
  assert(hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE) == 2);

  nr = 1;
  err = hwloc_distances_get_by_type(topology, HWLOC_OBJ_NUMANODE, &nr, &distances, 0, 0);
  assert(!err);
  assert(nr == 1);
  assert(distances->nbobjs == 2);
  assert(distances->kind == (HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER));
  hwloc_distances_release(topology, distances);

  nr = 1;
  err = hwloc_distances_get_by_type(topology, HWLOC_OBJ_CORE, &nr, &distances, 0, 0);
  assert(!err);
  assert(nr == 1);
  assert(distances->nbobjs == 4);
  assert(distances->kind == (HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER));
  hwloc_distances_release(topology, distances);

  hwloc_topology_destroy(topology);

  hwloc_bitmap_free(cpuset);
  return 0;
}
