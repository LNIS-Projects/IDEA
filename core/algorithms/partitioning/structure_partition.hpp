#pragma once
#include <vector>
#include <libkahypar.h>
#include <mockturtle/mockturtle.hpp>
namespace oracle
{
  template<typename Ntk>
  class structure_partition
  {
  public:
    double sap_fixed(std::vector<kahypar_partition_id_t>& assigned, uint32_t max_size, uint32_t max_inflections) {
      ntk.clear_values();
      std::vector<std::vector<typename Ntk::node>> parts = extract_parts(max_inflections);
      return bin_pack(assigned, parts, max_size);
    }

    int32_t distance_from_pi(typename Ntk::node pi, typename Ntk::node node) {
      ptrdiff_t idx = find(ntk._storage->inputs.begin(), ntk._storage->inputs.end(), ntk.node_to_index(pi)) - ntk._storage->inputs.begin();
      return distances_pi[idx][ntk.node_to_index(node)];
    }

    int32_t distance_from_pi(typename Ntk::signal in, typename Ntk::signal out) {
      return distance_from_pi(ntk.get_node(in), ntk.get_node(out));
    }

    structure_partition(Ntk& ntk): ntk(ntk) {
      distances_pi.resize(ntk.num_pis(), std::vector(ntk.size(), -1));
      //calculate_distance_from_pi();
    }
  private:
    void calculate_distance_from_pi() {
      mockturtle::topo_view<Ntk> topo{ntk};
      topo.foreach_pi([this, topo](auto pi, auto pi_i) {
        distances_pi[pi_i][pi] = 0;
        topo.foreach_node([this, topo, pi, pi_i](auto node) {
          topo.foreach_fanin(node, [this, topo, pi, pi_i, node](auto fin) {
            uint32_t n_i = topo.node_to_index(node);
            // std::cout << po_i << " " << po << " " << n_i << " " << f_i << " " << this->distances_pi[po_i][f_i] << " " << this->distances_pi[po_i][n_i];
            auto x = this->distances_pi[pi_i][fin.index];
            if (x >= 0) {
              this->distances_pi[pi_i][n_i] = std::max(x+1, this->distances_pi[pi_i][n_i]); //TODO weights
            }

            // std::cout << " " << this->distances_pi[po_i][n_i] << std::endl;
          });
        });
      });
    }

    std::tuple<typename Ntk::signal, std::vector<typename Ntk::node>, int32_t> deepest_po(){
      typename Ntk::signal max_po;
      int32_t max_depth = -1;
      std::vector<typename Ntk::node> max_cone;

      ntk.foreach_po([this, &max_depth, &max_po, &max_cone](auto po) {
        //        std::cout << "PO " << po.index << std::endl;
        ntk.incr_trav_id();
        uint32_t trav_id = ntk.trav_id();
        ntk.clear_visited();
        std::vector<typename Ntk::node> cone;
        typename Ntk::node pon = ntk.get_node(po);
        int32_t depth = collect_cone(cone, pon, trav_id);
        if (depth > max_depth) {
          max_po = po;
          max_depth = depth;
          max_cone = cone;
        }
      });
      return std::make_tuple(max_po, max_cone, max_depth);
    }

    int32_t collect_cone(std::vector<typename Ntk::node>& cone, typename Ntk::node node, uint32_t trav_id) {
      if (ntk.visited(node) || ntk.value(node) != 0) {
        return 0;
      }
      cone.push_back(node);
      ntk.set_visited(node, trav_id);

      int32_t depth = 0;
      ntk.foreach_fanin(node, [this, &cone, &depth, trav_id](auto fin){
        int32_t p = collect_cone(cone, ntk.get_node(fin), trav_id) + 1;
        if (p > depth) depth = p;
      });
      return depth;
    }

    std::vector<std::vector<typename Ntk::node>> extract_parts(size_t max_inflections) {
      std::vector<std::vector<typename Ntk::node>> parts;
      uint32_t inflections = 0;
      int32_t second_last_max_depth = -2;
      int32_t last_max_depth = -1;
      while (true) {

        auto [po, cone, max_depth] = deepest_po();
        std::cout << max_depth << std::endl;
        if (cone.size() == 0 || max_depth <= 0) {
          break;
        }
        // typename Ntk::node po_n = ntk.get_node(po);

        for (auto node = cone.begin(); node != cone.end(); node++) {
          ntk.set_value(*node, 1);
        }

        if (max_depth != last_max_depth && last_max_depth == second_last_max_depth) {
          inflections++;
        }
        if (inflections >= max_inflections) {
          break;
        }

        parts.push_back(cone);

        second_last_max_depth = last_max_depth;
        last_max_depth = max_depth;
      }

      return parts;
    }

    uint32_t bin_pack(std::vector<kahypar_partition_id_t>& assigned, std::vector<std::vector<typename Ntk::node>> parts, size_t bin_size){
      uint32_t max_bin = 0;
      int32_t partition = 0;
      std::vector<typename Ntk::node> bin;
      for (auto part: parts) {
        for (auto node: part) {
          bin.push_back(node);
        }
        if (bin.size() >= bin_size) {
          for (auto node: bin) {
            assigned[ntk.node_to_index(node)] = partition;
          }
          partition++;
          if (bin.size() > max_bin) {
            max_bin = bin.size();
          }
          bin.clear();
        }
      }
      if (bin.size() >= 0) {
        if (bin.size() > max_bin) {
          max_bin = bin.size();
         }
        for (auto node: bin) {
          assigned[ntk.node_to_index(node)] = partition;
        }
      }
      return max_bin;
    }

    Ntk& ntk;
    std::vector<std::vector<int32_t>> distances_pi;
  };
}
