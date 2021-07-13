#include <alice/alice.hpp>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/akers.hpp>
#include <mockturtle/algorithms/node_resynthesis/direct.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>

#include <stdio.h>
#include <fstream>

#include <sys/stat.h>
#include <stdlib.h>
#include "kahypar_config.hpp"
#ifdef ENABLE_GALOIS
#include <utah/BiPart.h>
#endif

namespace alice
{
  class partitioning_command : public alice::command{

  public:
    using aig_names = mockturtle::names_view<mockturtle::aig_network>;
    using aig_ntk = std::shared_ptr<aig_names>;
    using part_man_aig = oracle::partition_manager<aig_names>;
    using part_man_aig_ntk = std::shared_ptr<part_man_aig>;

    using mig_names = mockturtle::names_view<mockturtle::mig_network>;
    using mig_ntk = std::shared_ptr<mig_names>;
    using part_man_mig = oracle::partition_manager<mig_names>;
    using part_man_mig_ntk = std::shared_ptr<part_man_mig>;

    explicit partitioning_command( const environment::ptr& env )
      : command( env, "Partitions current network using k-means hypergraph partitioner" ) {

      opts.add_option( "--num,num", num_partitions, "Number of desired partitions" )->required();
      opts.add_option( "--config_direc,-c", config_direc, "Path to the configuration file for KaHyPar." );
      opts.add_option("--file,-f", part_file, "External file containing partition information");
      add_flag("--mig,-m", "Partitions stored MIG network (AIG network is default)");
#ifdef ENABLE_GALOIS
      add_flag("--bipart,-g", "Run hypergraph partitionining using BiPart from the Galois system");
#endif
    }

  protected:
    std::vector<int> read_file(string filename) {
      std::vector<int> output;
      std::ifstream ifs;

      ifs.open(filename);
      if(ifs.is_open()){
        while(ifs.good()){
          std::string part;
          getline(ifs, part);
          output.push_back(std::stoi(part));
        }
        ifs.close();
        return output;
      }
      else{
        std::cout << "Unable to open partition data file\n";
        throw exception();
      }
    }

    template <typename gen_ntk, typename gen_names, typename part_man_gen, typename part_man_gen_ntk, typename node_type>
    void process_network(string type_name) {
      auto ntk = *store<gen_ntk>().current();
      if(part_file != ""){
        std::cout << "Partitioning stored " << type_name << " network using external file\n";
        std::map<node_type, int> part_data;
        std::vector<int> parts = read_file(part_file);
        if(parts.size() != ntk.size()){
          std::cout << "Partition file contains the incorrect number of nodes\n";
          exit(1);
        }
        for (int i = 0; i < parts.size(); i++) {
          part_data[ntk.index_to_node(i)] = parts[i];
        }
        oracle::partition_manager<gen_names> partitions(ntk, part_data, num_partitions);
        store<part_man_gen_ntk>().extend() = std::make_shared<part_man_gen>( partitions );
      }
      else{
#ifdef ENABLE_GALOIS
        if(is_set("bipart")){
          std::cout << "Partitioning stored " << type_name << " network using Galois BiPart\n";
          oracle::hypergraph<gen_names> t(ntk);
          uint32_t num_vertices = 0;

          t.get_hypergraph(ntk);
          std::vector<std::vector<uint32_t>> hedges = t.get_hyperedges();
          num_vertices = t.get_num_vertices();

          int num_threads = 14;
          scheduleMode mode = PP;
          std::map<int, int> bipart = biparting(hedges, num_vertices, num_partitions, num_threads, mode);
          std::map<node_type, int> part_data;
          ntk.foreach_node([&](auto node){
            part_data[node] = bipart[node];
          });

          oracle::partition_manager<gen_names> partitions(ntk, part_data, num_partitions);
          store<part_man_gen_ntk>().extend() = std::make_shared<part_man_gen>( partitions );
        }
        else
#endif
          {
            std::cout << "Partitioning stored " << type_name << " network using KaHyPar\n";
            if(config_direc == ""){
              config_direc = make_temp_config();
            }
            oracle::partition_manager<gen_names> partitions(ntk, num_partitions, config_direc);
            store<part_man_gen_ntk>().extend() = std::make_shared<part_man_gen>( partitions );
          }
      }
    }
    void execute(){
      mockturtle::mig_npn_resynthesis resyn_mig;
      mockturtle::xag_npn_resynthesis<mockturtle::aig_network> resyn_aig;

      if(is_set("mig")){
        if(!store<mig_ntk>().empty()) {
          process_network<mig_ntk, mig_names, part_man_mig, part_man_mig_ntk, mockturtle::mig_network::node>("MIG");
        }
        else{
          std::cout << "MIG network not stored\n";
        }
      }
      else{
        if(!store<aig_ntk>().empty()) {
          process_network<aig_ntk, aig_names, part_man_aig, part_man_aig_ntk, mockturtle::aig_network::node>("AIG");
        }
        else{
          std::cout << "AIG network not stored\n";
        }
      }
    }
  private:
    int num_partitions{};
    std::string config_direc = "";
    std::string part_file = "";
  };

  ALICE_ADD_COMMAND(partitioning, "Partitioning");
}
