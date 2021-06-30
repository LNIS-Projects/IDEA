#include <iostream>
#include <gtest/gtest.h>
#include <queue>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/write_dot.hpp>
#include "algorithms/partitioning/structure_partition.hpp"

using network = mockturtle::aig_network;

network::signal add_symmetric(network *net, size_t depth) {
  if (depth == 0) {
    return net->create_pi();
  } else {
    return net->create_nand(add_symmetric(net, depth-1), add_symmetric(net, depth-1));
  }
}

network::signal add_chain(network *net, size_t depth) {
  network::signal a = net->create_pi();
  network::signal b = net->create_nand(a, net->create_pi());
  for (; depth > 0; depth--) {
    network::signal c = net->create_nand(a, b);
    a = b;
    b = c;
  }
  return b;
}

/*
 * verify distance calculations over ISCAS C17
 */
TEST(sap_test, sap_c17_distance_calculation_case)
{
// {
  network *net = new network;
  network::signal e_n1 = net->create_pi();
  network::signal e_n2 = net->create_pi();
  network::signal e_n3 = net->create_pi();
  network::signal e_n4 = net->create_pi();
  network::signal e_n5 = net->create_pi();
  network::signal e_w0 = net->create_nand(e_n1, e_n3);
  network::signal e_w1 = net->create_nand(e_n3, e_n4);
  network::signal e_w2 = net->create_nand(e_w1, e_n2);
  network::signal e_m1 = net->create_nand(e_w1, e_n5);

  network::signal e_s1_a0 = e_w0;
  network::signal e_s1_a1 = e_w2;
  network::signal e_s1_a2 = e_n5;
  network::signal e_s1_aw0 = net->create_nand(e_s1_a1, e_s1_a2);
  network::signal e_s1_aw1 = net->create_not(e_s1_a0);
  network::signal e_s1_aw2 = net->create_nand(e_s1_aw0, e_s1_aw1);
  network::signal e_s1_aw3 = net->create_nand(e_s1_aw1, net->create_nand(e_s1_a2, e_s1_a0));
  network::signal e_s1_b1 = net->create_nand(e_s1_aw2, e_s1_aw3);

  network::signal e_w3 = e_s1_b1;

  network::signal e_s2_a0 = e_w2;
  network::signal e_s2_a1 = e_w3;
  network::signal e_s2_a2 = e_w1;
  network::signal e_s2_aw0 = net->create_nand(e_s2_a1, e_s2_a2);
  network::signal e_s2_aw1 = net->create_not(e_s2_a0);
  network::signal e_s2_aw2 = net->create_nand(e_s2_aw0, e_s2_aw1);
  network::signal e_s2_aw3 = net->create_nand(e_s2_aw1, net->create_nand(e_s2_a2, e_s2_a0));
  network::signal e_s2_b1 = net->create_nand(e_s2_aw2, e_s2_aw3);

  network::signal e_m2 = e_s2_b1;

  uint32_t e_m1_po = net->create_po(e_m1);
  uint32_t e_m2_po = net->create_po(e_m2);
  mockturtle::write_dot(*net, "ntk.dot");

  std::vector<kahypar_partition_id_t> partition(net->size(), -1);
  oracle::structure_partition<network> sap(*net);

  ASSERT_EQ(sap.distance_from_pi(e_n1, e_n1), 0);
  ASSERT_EQ(sap.distance_from_pi(e_n1, e_n2), -1);
  ASSERT_EQ(sap.distance_from_pi(e_n1, e_w0), 1);
  ASSERT_EQ(sap.distance_from_pi(e_n1, e_m1), -1);
  ASSERT_EQ(sap.distance_from_pi(e_n1, e_m2), 7);
  ASSERT_EQ(sap.distance_from_pi(e_n3, e_m1), 2);
}

/*
 * Calculating a fully extracted network.
 */
TEST(sap_test, sap_cones_case)
{
  network *net = new network;
  network::signal pi0 = net->create_pi();
  network::signal pi1 = net->create_pi();
  network::signal pi2 = net->create_pi();
  network::signal pi3 = net->create_pi();
  network::signal w0 = net->create_nand(pi0, pi1);
  network::signal w1 = net->create_nand(pi1, pi2);
  network::signal w2 = net->create_nand(w0, w1);
  network::signal w3 = net->create_nand(pi3, w1);
  uint32_t po0 = net->create_po(w2);
  uint32_t po1 = net->create_po(w3);

  std::vector<kahypar_partition_id_t> partition(net->size(), -1);
  oracle::structure_partition<network> sap(*net);
  sap.sap_fixed(partition, 5, 1);
  ASSERT_EQ(partition[pi0.index], 0);
  ASSERT_EQ(partition[pi1.index], 0);
  ASSERT_EQ(partition[pi2.index], 0);
  ASSERT_EQ(partition[pi3.index], 1);
  ASSERT_EQ(partition[w0.index], 0);
  ASSERT_EQ(partition[w1.index], 0);
  ASSERT_EQ(partition[w2.index], 0);
  ASSERT_EQ(partition[w3.index], 1);
}

/*
 * Calculating halting on the second inflection.
 */
TEST(sap_test, sap_inflection_case)
{
  network *net = new network;
  network::signal w0 = add_symmetric(net, 5);
  network::signal w1 = add_symmetric(net, 4);
  network::signal w2 = add_symmetric(net, 4);
  network::signal w3 = add_symmetric(net, 3);
  network::signal w4 = add_symmetric(net, 3);
  network::signal w5 = add_symmetric(net, 2);
  network::signal w6 = add_symmetric(net, 2);
  network::signal w7 = add_symmetric(net, 1);
  uint32_t po0 = net->create_po(w0);
  uint32_t po1 = net->create_po(w1);
  uint32_t po2 = net->create_po(w2);
  uint32_t po3 = net->create_po(w3);
  uint32_t po4 = net->create_po(w4);
  uint32_t po5 = net->create_po(w5);
  uint32_t po6 = net->create_po(w6);
  uint32_t po7 = net->create_po(w7);

  std::vector<kahypar_partition_id_t> partition(net->size(), -1);
  oracle::structure_partition<network> sap(*net);

  sap.sap_fixed(partition, 1, 2); // Minimum size forces cone per bin during bin packing.
  ASSERT_EQ(partition[w0.index], 0);
  ASSERT_EQ(partition[w1.index], 1);
  ASSERT_EQ(partition[w2.index], 2);
  ASSERT_EQ(partition[w3.index], 3);
  ASSERT_EQ(partition[w4.index], 4);
  ASSERT_EQ(partition[w5.index], -1);
}

/*
 * Testing bin packing
 */
TEST(sap_test, sap_packing_case)
{
  network *net = new network;
  network::signal w0 = add_chain(net, 3);
  network::signal w1 = add_chain(net, 3);
  network::signal w2 = add_chain(net, 3);
  network::signal w3 = add_chain(net, 3);
  network::signal w4 = add_chain(net, 3);
  uint32_t po0 = net->create_po(w0);
  uint32_t po1 = net->create_po(w1);
  uint32_t po2 = net->create_po(w2);
  uint32_t po3 = net->create_po(w3);
  uint32_t po4 = net->create_po(w4);

  std::vector<kahypar_partition_id_t> partition(net->size(), -1);
  oracle::structure_partition<network> sap(*net);
  sap.sap_fixed(partition, 12, 2); // Minimum size forces cone per bin during bin packing.
  ASSERT_EQ(partition[w0.index], 0);
  ASSERT_EQ(partition[w1.index], 0);
  ASSERT_EQ(partition[w2.index], 1);
  ASSERT_EQ(partition[w3.index], 1);
  ASSERT_EQ(partition[w4.index], 2);
}
