/*
 * Copyright © 2011-2024 Inria.  All rights reserved.
 * Copyright © 2011 Université Bordeaux.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include "hwloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

static void print_distances(const struct hwloc_distances_s *distances)
{
  unsigned nbobjs = distances->nbobjs;
  unsigned i, j;

  printf("     ");
  /* column header */
  for(j=0; j<nbobjs; j++)
    printf(" % 5d", (int) distances->objs[j]->os_index);
  printf("\n");

  /* each line */
  for(i=0; i<nbobjs; i++) {
    /* row header */
    printf("% 5d", (int) distances->objs[i]->os_index);
    /* each value */
    for(j=0; j<nbobjs; j++)
      printf(" % 5d", (int) distances->values[i*nbobjs+j]);
    printf("\n");
  }
}

static void check(hwloc_topology_t topology, unsigned nbgroups, unsigned nbnodes, unsigned nbcores, unsigned nbpus)
{
  int depth;
  unsigned nb;
  unsigned long long total_memory;

  /* sanity checks */
  depth = hwloc_topology_get_depth(topology);
  assert(depth == 3 + (nbgroups > 0));
  depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
  assert(depth == HWLOC_TYPE_DEPTH_NUMANODE);
  depth = hwloc_get_type_depth(topology, HWLOC_OBJ_GROUP);
  assert(depth == ((nbgroups > 0) ? 1 : HWLOC_TYPE_DEPTH_UNKNOWN));
  depth = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
  assert(depth == 1 + (nbgroups > 0));
  depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
  assert(depth == 2 + (nbgroups > 0));

  /* actual checks */
  nb = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);
  assert(nb == nbnodes);
  nb = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_GROUP);
  assert(nb == nbgroups);
  nb = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
  assert(nb == nbcores);
  nb = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
  assert(nb == nbpus);
  total_memory = hwloc_get_root_obj(topology)->total_memory;
  assert(total_memory == nbnodes * 1024*1024*1024); /* synthetic topology puts 1GB per node */
}

static void check_distances(hwloc_topology_t topology, unsigned nbnodes, unsigned nbcores)
{
  struct hwloc_distances_s *distance;
  unsigned nr;
  int err;

  /* node distance */
  nr = 1;
  err = hwloc_distances_get_by_type(topology, HWLOC_OBJ_NUMANODE, &nr, &distance, 0, 0);
  assert(!err);
  if (nbnodes >= 2) {
    assert(nr == 1);
    assert(distance);
    assert(distance->nbobjs == nbnodes);
    print_distances(distance);
    hwloc_distances_release(topology, distance);
  } else {
    assert(nr == 0);
  }

  /* core distance */
  nr = 1;
  err = hwloc_distances_get_by_type(topology, HWLOC_OBJ_CORE, &nr, &distance, 0, 0);
  assert(!err);
  if (nbcores >= 2) {
    assert(nr == 1);
    assert(distance);
    assert(distance->nbobjs == nbcores);
    print_distances(distance);
    hwloc_distances_release(topology, distance);
  } else {
    assert(nr == 0);
  }
}

int main(void)
{
  hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
  hwloc_obj_t nodes[3], cores[6];
  hwloc_uint64_t node_distances[9], core_distances[36];
  hwloc_distances_add_handle_t handle;
  hwloc_topology_t topology, topology2;
  hwloc_obj_t obj;
  unsigned i,j;
  int err;

  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "node:3 core:2 pu:4");
  hwloc_topology_load(topology);

  for(i=0; i<3; i++) {
    nodes[i] = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, i);
    for(j=0; j<3; j++)
      node_distances[i*3+j] = (i == j ? 10 : 20);
  }
  handle = hwloc_distances_add_create(topology, NULL,
                                      HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER,
                                      0);
  assert(handle);
  err = hwloc_distances_add_values(topology, handle, 3, nodes, node_distances, 0);
  assert(!err);
  err = hwloc_distances_add_commit(topology, handle,
                                   HWLOC_DISTANCES_ADD_FLAG_GROUP);
  assert(!err);

  for(i=0; i<6; i++) {
    cores[i] = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, i);
    for(j=0; j<6; j++)
      core_distances[i*6+j] = (i == j ? 4 : 8);
  }
  handle = hwloc_distances_add_create(topology, NULL,
                                      HWLOC_DISTANCES_KIND_VALUE_LATENCY|HWLOC_DISTANCES_KIND_FROM_USER,
                                      0);
  assert(handle);
  err = hwloc_distances_add_values(topology, handle, 6, cores, core_distances, 0);
  assert(!err);
  err = hwloc_distances_add_commit(topology, handle,
                                   HWLOC_DISTANCES_ADD_FLAG_GROUP);
  assert(!err);

  /* entire topology */
  printf("starting from full topology\n");
  check(topology, 3, 3, 6, 24);
  check_distances(topology, 3, 6);

  /* restrict to nothing, impossible */
  printf("restricting to nothing, must fail\n");
  hwloc_bitmap_zero(cpuset);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(err < 0 && errno == EINVAL);
  printf("restricting to unexisting PU:24, must fail\n");
  hwloc_bitmap_only(cpuset, 24);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(err < 0 && errno == EINVAL);
  err = hwloc_topology_refresh(topology);
  assert(!err);
  check(topology, 3, 3, 6, 24);
  check_distances(topology, 3, 6);

  /* restrict to everything, will do nothing */
  printf("restricting to everything, does nothing\n");
  hwloc_bitmap_fill(cpuset);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(!err);
  check(topology, 3, 3, 6, 24);
  check_distances(topology, 3, 6);

  /* remove a single pu (second PU of second core of second node) */
  printf("removing second PU of second core of second node\n");
  hwloc_bitmap_fill(cpuset);
  hwloc_bitmap_clr(cpuset, 13);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(!err);
  check(topology, 3, 3, 6, 23);
  check_distances(topology, 3, 6);

  /* remove the entire second core of first node */
  printf("removing entire second core of first node\n");
  hwloc_bitmap_fill(cpuset);
  hwloc_bitmap_clr_range(cpuset, 4, 7);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(!err);
  check(topology, 3, 3, 5, 19);
  check_distances(topology, 3, 5);

  /* remove the entire third node */
  printf("removing all PUs under third node, but keep that CPU-less node\n");
  hwloc_bitmap_fill(cpuset);
  hwloc_bitmap_clr_range(cpuset, 16, 23);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(!err);
  check(topology, 3, 3, 3, 11);
  check_distances(topology, 3, 3);

  /* duplicate/checkpoint to the current topology for additional testing */
  err = hwloc_topology_dup(&topology2, topology);
  assert(!err);

  /* only keep three PUs (first and last of first core, and last of last core of second node) */
  printf("restricting to 3 PUs in 2 cores in 2 nodes, and remove the CPU-less node, and auto-merge groups\n");
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set(cpuset, 0);
  hwloc_bitmap_set(cpuset, 3);
  hwloc_bitmap_set(cpuset, 15);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_REMOVE_CPULESS);
  assert(!err);
  check(topology, 0, 2, 2, 3);
  check_distances(topology, 2, 2);

  /* only keep three PUs (first and last of first core, and last of last core of second node) */
  printf("do the same on the duplicated topology, with multiple intermediate restricts\n");
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set(cpuset, 0);
  hwloc_bitmap_set(cpuset, 3);
  hwloc_bitmap_set(cpuset, 15);
  err = hwloc_topology_restrict(topology2, cpuset, 0);
  assert(!err);
  check(topology2, 3, 3, 2, 3);
  check_distances(topology2, 3, 2);
  err = hwloc_topology_restrict(topology2, cpuset, 0);
  assert(!err);
  check(topology2, 3, 3, 2, 3);
  check_distances(topology2, 3, 2);
  err = hwloc_topology_restrict(topology2, cpuset, HWLOC_RESTRICT_FLAG_REMOVE_CPULESS);
  assert(!err);
  check(topology2, 0, 2, 2, 3);
  check_distances(topology2, 2, 2);
  hwloc_topology_destroy(topology2);

  /* restrict to the third node, impossible */
  printf("restricting to only some already removed node, must fail\n");
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 16, 23);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(err == -1 && errno == EINVAL);
  check(topology, 0, 2, 2, 3);
  check_distances(topology, 2, 2);

  hwloc_topology_destroy(topology);

  /* check that restricting exactly on a Group object keeps things coherent */
  printf("restricting to a Group covering only the of the PU level\n");
  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "pu:4");
  hwloc_topology_load(topology);
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 1, 2);
  obj = hwloc_topology_alloc_group_object(topology);
  obj->cpuset = hwloc_bitmap_dup(cpuset);
  obj->name = strdup("toto");
  hwloc_topology_insert_group_object(topology, obj);
  hwloc_topology_restrict(topology, cpuset, 0);
  hwloc_topology_check(topology);
  hwloc_topology_destroy(topology);

  /* check memory-based restricting */
  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "node:3 core:2 pu:4");
  hwloc_topology_load(topology);
  printf("restricting bynodeset to two numa nodes\n");
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 1, 2);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET);
  assert(!err);
  hwloc_topology_check(topology);
  check(topology, 3, 2, 6, 24);
  printf("applying exact same restriction again\n");
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET);
  assert(!err);
  hwloc_topology_check(topology);
  check(topology, 3, 2, 6, 24);
  printf("further restricting bynodeset to a single numa node\n");
  hwloc_bitmap_only(cpuset, 1);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET|HWLOC_RESTRICT_FLAG_REMOVE_MEMLESS);
  assert(!err);
  hwloc_topology_check(topology);
  check(topology, 0, 1, 2, 8);
  printf("restricting with invalid flags\n");
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_REMOVE_MEMLESS);
  assert(err == -1);
  assert(errno == EINVAL);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET|HWLOC_RESTRICT_FLAG_REMOVE_CPULESS);
  assert(err == -1);
  assert(errno == EINVAL);

  hwloc_topology_destroy(topology);

  /* check that restricting PUs maintains ordering of normal children */
  printf("restricting so that PUs get reordered\n");
  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "node:1 core:2 pu:2(indexes=0,2,1,3)");
  hwloc_topology_load(topology);
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 1, 2);
  err = hwloc_topology_restrict(topology, cpuset, 0);
  assert(!err);
  hwloc_topology_destroy(topology);

  /* check that restricting NUMA nodes maintains ordering of normal children in remove-memless case */
  printf("restricting by nodeset so that remaining non-memless PUs get reordered\n");
  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "pack:2 l3:2 numa:1 pu:1(indexes=0,2,1,3)");
  hwloc_topology_load(topology);
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 1, 2);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET|HWLOC_RESTRICT_FLAG_REMOVE_MEMLESS);
  assert(!err);
  hwloc_topology_destroy(topology);

  /* again with intermediate restricts */
  printf("do the same on the duplicated topology, with multiple intermediate restricts\n");
  hwloc_topology_init(&topology);
  hwloc_topology_set_synthetic(topology, "pack:2 l3:2 numa:1 pu:1(indexes=0,2,1,3)");
  hwloc_topology_load(topology);
  hwloc_bitmap_zero(cpuset);
  hwloc_bitmap_set_range(cpuset, 1, 2);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET);
  assert(!err);
  err = hwloc_topology_restrict(topology, cpuset, HWLOC_RESTRICT_FLAG_BYNODESET|HWLOC_RESTRICT_FLAG_REMOVE_MEMLESS);
  assert(!err);
  hwloc_topology_destroy(topology);

  hwloc_bitmap_free(cpuset);

  return 0;
}
