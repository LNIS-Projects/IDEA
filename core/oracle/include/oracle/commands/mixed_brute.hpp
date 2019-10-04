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


namespace alice
{
class mixed_brute_command : public alice::command{

public:
  explicit mixed_brute_command( const environment::ptr& env )
    : command( env, "Optimize partitions with AIG based-optimizer." ){
    opts.add_option( "--num_parts,-p", num_parts, "Number of partitions to create" )->required();
  }

protected:
  void execute(){

    //read AIG to generate hypergraph
    if(!store<mockturtle::aig_network>().empty()) {
      auto ntk = store<mockturtle::aig_network>().current();
      std::cout << "AIG initial size = " << ntk.num_gates() << std::endl;
      mockturtle::depth_view depth{ntk};
      std::cout << "AIG initial size = " << ntk.num_gates() << " and depth = " << depth.depth() << "\n";

      oracle::partition_manager<mockturtle::aig_network> partitions_aig(ntk, num_parts);
      mockturtle::mig_npn_resynthesis resyn_mig;
      mockturtle::xag_npn_resynthesis<mockturtle::aig_network> resyn_aig;
      std::vector<int> aig_parts1;
      std::vector<int> mig_parts1;
                
      for(int i = 0; i < num_parts; i++){
        oracle::partition_view<mockturtle::aig_network> part_aig = partitions_aig.create_part(ntk, i);

        auto opt_aig = mockturtle::node_resynthesis<mockturtle::aig_network>( part_aig, resyn_aig );
        mockturtle::depth_view part_aig_depth{opt_aig};
        std::cout << "aig part size = " << opt_aig.num_gates() << " and depth = " << part_aig_depth.depth() << "\n";
        mockturtle::aig_script aigopt;
        opt_aig = aigopt.run(opt_aig);
        mockturtle::depth_view part_aig_opt_depth{opt_aig};
        int aig_opt_size = opt_aig.num_gates();
        int aig_opt_depth = part_aig_opt_depth.depth();
        std::cout << "optimized aig part size = " << aig_opt_size << " and depth = " << aig_opt_depth << "\n";

        auto opt_mig = mockturtle::node_resynthesis<mockturtle::mig_network>( part_aig, resyn_mig );
        mockturtle::depth_view part_mig_depth{opt_mig};
        std::cout << "mig part size = " << opt_mig.num_gates() << " and depth = " << part_mig_depth.depth() << "\n";
        mockturtle::mig_script migopt;
        opt_mig = migopt.run(opt_mig);
        mockturtle::depth_view part_mig_opt_depth{opt_mig};
        int mig_opt_size = opt_mig.num_gates();
        int mig_opt_depth = part_mig_opt_depth.depth();
        std::cout << "optimized mig part size = " << mig_opt_size << " and depth = " << mig_opt_depth << "\n";

        if((aig_opt_size * aig_opt_depth) <= (mig_opt_size * mig_opt_depth)){
          std::cout << "AIG wins\n";
          aig_parts1.push_back(i);
        }
        else{
          std::cout << "MIG wins\n";
          mig_parts1.push_back(i);
        }
      }

      //Deal with AIG partitions
      std::cout << "Total number of partitions for AIG 1 " << aig_parts1.size() << std::endl;
      std::cout << "Total number of partitions for MIG 1 " << mig_parts1.size() << std::endl;

      for (int i = 0; i < aig_parts1.size(); i++) {
        oracle::partition_view<mockturtle::aig_network> part_aig = partitions_aig.create_part(ntk, aig_parts1.at(i));

        std::cout << "\nPartition " << i << "\n";
        mockturtle::depth_view part_depth{part_aig};
        std::cout << "Partition size = " << part_aig.num_gates() << " and depth = " << part_depth.depth() << "\n";

        mockturtle::xag_npn_resynthesis<mockturtle::aig_network> resyn_aig;

        auto aig_opt = mockturtle::node_resynthesis<mockturtle::aig_network>( part_aig, resyn_aig );

        mockturtle::aig_script aigopt;
        auto aig = aigopt.run(aig_opt);

        mockturtle::depth_view part_aig_depth{aig};
        std::cout << "Post optimization part size = " << aig.num_gates() << " and depth = " << part_aig_depth.depth() << "\n";

        partitions_aig.synchronize_part(part_aig, aig, ntk);
      }

      partitions_aig.connect_outputs(ntk);
      auto ntk_final = mockturtle::cleanup_dangling(ntk);

      mockturtle::depth_view depth_final{ntk_final};

      std::cout << "Final AIG size = " << ntk_final.num_gates() << " and depth = " << depth_final.depth() << "\n";
      //mockturtle::write_verilog(ntk, filename);

      oracle::partition_manager<mockturtle::aig_network> tmp(ntk_final, num_parts);

      mockturtle::direct_resynthesis<mockturtle::mig_network> convert_mig;

      auto mig = mockturtle::node_resynthesis<mockturtle::mig_network>(ntk_final, convert_mig);
      std::cout << "Initial MIG size = " << mig.num_gates() << "\n";

      oracle::partition_manager<mockturtle::mig_network> partitions_mig(mig, num_parts);

      std::vector<int> aig_parts2;
      std::vector<int> mig_parts2;

      for(int i = 0; i < num_parts; i++){
        oracle::partition_view<mockturtle::mig_network> part_mig = partitions_mig.create_part(mig, i);

        auto opt_aig = mockturtle::node_resynthesis<mockturtle::aig_network>( part_mig, resyn_aig );
        mockturtle::depth_view part_aig_depth{opt_aig};
        std::cout << "aig part size = " << opt_aig.num_gates() << " and depth = " << part_aig_depth.depth() << "\n";
        mockturtle::aig_script aigopt;
        opt_aig = aigopt.run(opt_aig);
        mockturtle::depth_view part_aig_opt_depth{opt_aig};
        int aig_opt_size = opt_aig.num_gates();
        int aig_opt_depth = part_aig_opt_depth.depth();
        std::cout << "optimized aig part size = " << aig_opt_size << " and depth = " << aig_opt_depth << "\n";

        auto opt_mig = mockturtle::node_resynthesis<mockturtle::mig_network>( part_mig, resyn_mig );
        mockturtle::depth_view part_mig_depth{opt_mig};
        std::cout << "mig part size = " << opt_mig.num_gates() << " and depth = " << part_mig_depth.depth() << "\n";
        mockturtle::mig_script migopt;
        opt_mig = migopt.run(opt_mig);
        mockturtle::depth_view part_mig_opt_depth{opt_mig};
        int mig_opt_size = opt_mig.num_gates();
        int mig_opt_depth = part_mig_opt_depth.depth();
        std::cout << "optimized mig part size = " << mig_opt_size << " and depth = " << mig_opt_depth << "\n";

        if((aig_opt_size * aig_opt_depth) <= (mig_opt_size * mig_opt_depth)){
          std::cout << "AIG wins\n";
          aig_parts2.push_back(i);
        }
        else{
          std::cout << "MIG wins\n";
          mig_parts2.push_back(i);
        }
      }

      //Deal with AIG partitions
      std::cout << "Total number of partitions for AIG 2 " << aig_parts2.size() << std::endl;
      std::cout << "Total number of partitions for MIG 2 " << mig_parts2.size() << std::endl;
      for (int i = 0; i < mig_parts2.size(); i++) {
        oracle::partition_view<mockturtle::mig_network> part_mig = partitions_mig.create_part(mig, mig_parts2.at(i));
        mockturtle::mig_npn_resynthesis resyn_mig;

        std::cout << "\nPartition " << i << "\n";
        mockturtle::depth_view part_depth{part_mig};
        std::cout << "Partition size = " << part_mig.num_gates() << " and depth = " << part_depth.depth() << "\n";

        auto mig_opt = mockturtle::node_resynthesis<mockturtle::mig_network>(part_mig, resyn_mig);
        mockturtle::mig_script migopt;

        mig_opt = migopt.run(mig_opt);

        mockturtle::depth_view opt_mig_depth{mig_opt};

        std::cout << "Post optimization part size = " << mig_opt.num_gates() << " and depth = "
                  << opt_mig_depth.depth()
                  << "\n";

        partitions_mig.synchronize_part(part_mig, mig_opt, mig);
      }

      partitions_mig.connect_outputs(mig);
      mig = mockturtle::cleanup_dangling(mig);

      mockturtle::depth_view final_mig{mig};

      std::cout << "Total number of partitions for AIG 1 " << aig_parts1.size() << std::endl;
      std::cout << "Total number of partitions for MIG 1 " << mig_parts1.size() << std::endl;
      std::cout << "Total number of partitions for AIG 2 " << aig_parts2.size() << std::endl;
      std::cout << "Total number of partitions for MIG 2 " << mig_parts2.size() << std::endl;
      std::cout << "Final MIG size = " << mig.num_gates() << " and depth = " << final_mig.depth() << "\n";

      mockturtle::write_verilog(mig, "final_2steps.v");
    }
    else{
      std::cout << "There is no stored AIG network\n";
    }
  }

private:
  std::string filename{};
  int num_parts = 0;
};

ALICE_ADD_COMMAND(mixed_brute, "Optimization");
}
