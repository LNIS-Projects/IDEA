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
// #include <utah/BiPart.h>


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

          auto num_opt = opts.add_option( "--num,num", num_partitions, "Number of desired partitions" );
          opts.add_option( "--size", size_partitions, "Number of desired average nodes per partition." )->excludes(num_opt);
          opts.add_option( "--config_direc,-c", config_direc, "Path to the configuration file for KaHyPar." );
          opts.add_option("--file,-f", part_file, "External file containing partition information");
          opts.add_option("--output,-o", output_file, "External file to write the generated partitions to.");
          opts.add_option("--initial,-i", initial_file, "External file to write the initial partitions to.");
          opts.add_option("--node_weights,-n", node_weight_file, "External file containing node weights");
          opts.add_option("--edge_weights,-e", edge_weight_file, "External file containing edge weights");
          add_flag("--mig,-m", "Partitions stored MIG network (AIG network is default)");
          add_flag("--sap,-s", "Do the SAP thing.");
          opts.add_option("--epsilon", imbalance, "Hypergraph partitioning epsilon imbalance parameter.");
          // add_flag("--bipart,-g", "Run hypergraph partitionining using BiPart from the Galois system");
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
        // if(is_set("bipart")){
        //   std::cout << "Partitioning stored " << type_name << " network using Galois BiPart\n";
        //   oracle::hypergraph<gen_names> t(ntk);
        //   uint32_t num_vertices = 0;

        //   t.get_hypergraph(ntk);
        //   std::vector<std::vector<uint32_t>> hedges = t.get_hyperedges();
        //   num_vertices = t.get_num_vertices();

        //   int num_threads = 14;
        //   scheduleMode mode = PP;
        //   std::map<int, int> bipart = biparting(hedges, num_vertices, num_partitions, num_threads, mode);
        //   std::map<node_type, int> part_data;
        //   ntk.foreach_node([&](auto node){
        //     part_data[node] = bipart[node];
        //   });

        //   oracle::partition_manager<gen_names> partitions(ntk, part_data, num_partitions);
        //   store<part_man_gen_ntk>().extend() = std::make_shared<part_man_gen>( partitions );
        // }
        // else{
        std::cout << "Partitioning stored " << type_name << " network using KaHyPar\n";
        int *node_weights = nullptr;
        int *edge_weights = nullptr;
        if (edge_weight_file != "") {
          std::cout << "Reading edge weights from " << edge_weight_file << std::endl;
          std::vector<int> data = read_file(edge_weight_file);
          edge_weights = &data[0];
        }
        if (node_weight_file != "") {
          std::cout << "Reading node weights from " << node_weight_file << std::endl;
          std::vector<int> data = read_file(node_weight_file);
          if(data.size() != ntk.size()){
            std::cout << "Node weight file contains the incorrect number of nodes: got " << data.size() << " expected " << ntk.size() << std::endl;
            exit(1);
          }
          else {
            node_weights = &data[0];
          }
        }
        if (num_partitions == 0) {
          num_partitions = std::max(ntk.size() / size_partitions, 1u);
        }
        std::cout << "Using " << num_partitions << " partitions" << std::endl;
        std::cout << "Using KaHyPar configuration " << config_direc << std::endl;
        if (is_set("sap")) {
          std::cout << "Using SAP" << std::endl;
        }
        oracle::partition_manager<gen_names> partitions(ntk, num_partitions, config_direc, node_weights, edge_weights, is_set("sap"), imbalance);
        store<part_man_gen_ntk>().extend() = std::make_shared<part_man_gen>( partitions );
        if (output_file != "") {
          std::ofstream out;
          out.open(output_file);

          for (auto i = partitions.get_partitions().begin(); i != partitions.get_partitions().end(); i++) {
            out << *i << std::endl;
          }
          out.close();
        }

        if (initial_file != "") {
          std::ofstream out;
          out.open(initial_file);

          for (auto i = partitions.get_initial_partitions().begin(); i != partitions.get_initial_partitions().end(); i++) {
            out << *i << std::endl;
          }
          out.close();
        }

        //}
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
      uint32_t num_partitions = 0;
      uint32_t size_partitions = 2048;
      std::string config_direc = "/usr/local/share/lsoracle/test.ini";
      std::string part_file = "";
      std::string output_file = "";
      std::string initial_file = "";
      std::string edge_weight_file = "";
      std::string node_weight_file = "";
      double imbalance = 0.9;
  };

  ALICE_ADD_COMMAND(partitioning, "Partitioning");
}
