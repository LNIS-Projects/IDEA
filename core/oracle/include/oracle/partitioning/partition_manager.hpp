/* mockturtle: C++ logic network library
 * Copyright (C) 2018  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file window_view.hpp
  \brief Implements an isolated view on a window in a network
  \author Heinz Riener
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <set>
#include <cassert>

#include <mockturtle/traits.hpp>
#include "partition_view.hpp"
#include "hyperg.hpp"
#include <mockturtle/networks/detail/foreach.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <libkahypar.h>

namespace oracle
{

  /*! \brief Partitions circuit using multi-level hypergraph partitioner
   *
   */
  template<typename Ntk>
  class partition_manager : public Ntk
  {
  public:
    using storage = typename Ntk::storage;
    using node = typename Ntk::node;
    using signal = typename Ntk::signal;

  public:
    partition_manager(){}

    partition_manager(Ntk const& ntk, std::map<node, int> partition, int part_num){
      // std::cout << "number of parts = " << part_num << "\n";
      num_partitions = part_num;
      for(int i = 0; i < part_num; ++i)
        _part_scope.push_back(std::set<node>());

      ntk.foreach_node( [&](auto curr_node){
        //get rid of circuit PIs
        // std::cout << "Node = " << curr_node << "\n";
        if (ntk.is_pi(curr_node) ) {
          // std::cout << "PI\n";
          // std::cout << "partition[curr_node] = " << partition[curr_node] << "\n";
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pis.insert(std::pair<int, node>(partition[curr_node], curr_node));
        }

        if (ntk.is_ro(curr_node) && !ntk.is_constant(curr_node)) {
          // std::cout << "RO\n";
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pis.insert(std::pair<int, node>(partition[curr_node], curr_node));
          if(ntk.is_po(curr_node) && !ntk.is_constant(curr_node)){
            _part_pos.insert(std::pair<int, node>(partition[curr_node], curr_node));
          }
        }
        //get rid of circuit POs
        else if (ntk.is_po(curr_node) && !ntk.is_constant(curr_node)) {
          // std::cout << "PO\n";
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pos.insert(std::pair<int, node>(partition[curr_node], curr_node));
        }
        else if (!ntk.is_constant(curr_node)) {
          // std::cout << "not constant\n";
          _part_scope[partition[curr_node]].insert(curr_node);
        }

        //look to partition inputs (those that are not circuit PIs)
        if (!ntk.is_pi(curr_node) && !ntk.is_ro(curr_node)){
          // std::cout << "not PI AND not RO\n";
          ntk.foreach_fanin(curr_node, [&](auto const &conn, auto j) {
            // std::cout << "fanin = " << conn.index << "\n";
            if (partition[ntk.index_to_node(conn.index)] != partition[curr_node] && !ntk.is_constant(ntk.index_to_node(conn.index))) {
              _part_scope[partition[curr_node]].insert(curr_node);
              _part_pis.insert(std::pair<int, node>(partition[curr_node], ntk.index_to_node(conn.index)));
              _part_pos.insert(std::pair<int, node>(partition[ntk.index_to_node(conn.index)],ntk.index_to_node(conn.index)));
              
            }
          });
        }
      });
      // std::cout << "mapped partitions\n";
      for(int i = 0; i < part_num; i++){
        partitionInputs[i] = create_part_inputs(i);
        typename std::set<node>::iterator it;
        partitionOutputs[i] = create_part_outputs(i);
        update_io(i);
      }
    }

    partition_manager(Ntk const& ntk, std::vector<std::set<node>> scope, std::unordered_map<int, std::set<node>> inputs, 
      std::unordered_map<int, std::set<node>> outputs, std::unordered_map<int, std::set<node>> regs, std::unordered_map<int, std::set<node>> regs_in, int part_num){

      num_partitions = part_num;
      _part_scope = scope;
      partitionInputs = inputs;
      partitionOutputs = outputs;
      partitionReg = regs;
      partitionRegIn = regs_in;
    }

    partition_manager( Ntk const& ntk, int part_num, std::string config_direc="../../core/test.ini" ) : Ntk( ntk )
    {

      static_assert( mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type" );
      static_assert( mockturtle::has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
      static_assert( mockturtle::has_visited_v<Ntk>, "Ntk does not implement the visited method" );
      static_assert( mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
      static_assert( mockturtle::has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
      static_assert( mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
      static_assert( mockturtle::has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );

      // std::cout << "HERE\n";
      num_partitions = part_num;

      for(int i = 0; i<part_num; ++i)
        _part_scope.push_back(std::set<node>());
      
      if(part_num == 1){
        ntk.foreach_pi( [&](auto pi){
          _part_scope[0].insert(ntk.index_to_node(pi));
          _part_pis.insert(std::pair<int, node>(0, ntk.index_to_node(pi)));
        });
        ntk.foreach_po( [&](auto po){
          _part_scope[0].insert(ntk.index_to_node(po.index));
          _part_pos.insert(std::pair<int, node>(0, ntk.index_to_node(po.index)));

        });
        ntk.foreach_gate( [&](auto curr_node){
          _part_scope[0].insert(curr_node);
          _part_nodes[curr_node] = 0;
        });

        for(int i = 0; i < part_num; i++){
          partitionInputs[i] = create_part_inputs(i);
          partitionOutputs[i] = create_part_outputs(i);
          partitionReg[i] = create_part_latches(i);
          partitionRegIn[i] = create_part_latches_in(i);
        }
      }

      else{
        uint32_t kahyp_num_hyperedges = 0;
        uint32_t kahyp_num_vertices = 0;
        uint32_t kahyp_num_indeces_hyper = 0;
        unsigned long kahyp_num_sets = 0;
        std::vector<uint32_t> kahypar_connections;
        std::vector<unsigned long> kahyp_set_indeces;

        /******************
        Generate HyperGraph
        ******************/

        oracle::hypergraph<Ntk> t(ntk);
        t.get_hypergraph(ntk);
        t.return_hyperedges(kahypar_connections);
        kahyp_num_hyperedges = t.get_num_edges();
        kahyp_num_vertices = t.get_num_vertices();
        kahyp_num_indeces_hyper = t.get_num_indeces();
        kahyp_num_sets = t.get_num_sets();
        t.get_indeces(kahyp_set_indeces);
        t.dump();

        /******************
        Partition with kahypar
        ******************/
        //configures kahypar
        kahypar_context_t* context = kahypar_context_new();
        kahypar_configure_context_from_file(context, config_direc.c_str());

        //set number of hyperedges and vertices. These variables are defined by the hyperG command
        const kahypar_hyperedge_id_t num_hyperedges = kahyp_num_hyperedges;
        const kahypar_hypernode_id_t num_vertices = kahyp_num_vertices;

        //set all edges to have the same weight
        std::unique_ptr<kahypar_hyperedge_weight_t[]> hyperedge_weights = std::make_unique<kahypar_hyperedge_weight_t[]>(kahyp_num_vertices);

        for( int i = 0; i < kahyp_num_vertices; i++ )
          hyperedge_weights[i] = 2;

        //vector with indeces where each set starts
        std::unique_ptr<size_t[]> hyperedge_indices = std::make_unique<size_t[]>(kahyp_num_sets+1);

        for ( int j = 0; j < kahyp_num_sets+1; j++){
          hyperedge_indices[j] = kahyp_set_indeces[j];
        }

        std::unique_ptr<kahypar_hyperedge_id_t[]> hyperedges = std::make_unique<kahypar_hyperedge_id_t[]>(kahyp_num_indeces_hyper);

        for ( int i = 0; i < kahyp_num_indeces_hyper; i++){
          hyperedges[i] = kahypar_connections[i];
        }

        const double imbalance = 0.5;
        const kahypar_partition_id_t k = part_num;

        kahypar_hyperedge_weight_t objective = 0;

        std::vector<kahypar_partition_id_t> partition(num_vertices, -1);

        kahypar_partition(num_vertices, num_hyperedges,
                          imbalance, k, nullptr, hyperedge_weights.get(),
                          hyperedge_indices.get(), hyperedges.get(),
                          &objective, context, partition.data());

        for(auto i=1; i <= ntk.num_pis(); i++){
          if(i<=ntk.num_pis()-ntk.num_latches()){
            //std::cout << "ADDIIING PI " << std::endl;
            _part_pis.insert(std::pair<int, node>(partition[i], ntk.index_to_node(i)));
          }
          else {
            _part_pis.insert(std::pair<int, node>(partition[i], ntk.index_to_node(i)));
            _part_ros.insert(std::pair<int, node>(partition[i], ntk.index_to_node(i)));
          }
        }
        
        ntk.foreach_node( [&](auto curr_node){
          if (!ntk.is_constant(curr_node)) {
            _part_scope[partition[ntk.node_to_index(curr_node)]].insert(curr_node);
          }

          //look to partition inputs (those that are not circuit PIs)
          if (!ntk.is_pi(curr_node) && !ntk.is_ro(curr_node)){
            ntk.foreach_fanin(curr_node, [&](auto const &conn, auto j) {
              if (partition[conn.index] != partition[ntk.node_to_index(curr_node)] && !ntk.is_constant(ntk.index_to_node(conn.index))) {
                _part_scope[partition[ntk.node_to_index(curr_node)]].insert(curr_node);
                _part_pis.insert(std::pair<int, node>(partition[ntk.node_to_index(curr_node)], ntk.index_to_node(conn.index)));
                _part_pos.insert(std::pair<int, node>(partition[conn.index],ntk.index_to_node(conn.index)));
                
              }
            });
          }
          // std::cout << "Node " << curr_node << " partition " << partition[ntk.node_to_index(curr_node)] << "\n";
        });

        for(auto i=0; i < ntk.num_pos(); i++){
          if(i<ntk.num_pos()-ntk.num_latches() && !ntk.is_constant(ntk.index_to_node(ntk._storage->outputs[i].index))){
            _part_pos.insert(std::pair<int, node>(partition[ntk._storage->outputs[i].index], ntk.index_to_node(ntk._storage->outputs[i].index)));
          }
          else {
			if(!ntk.is_constant(ntk.index_to_node(ntk._storage->outputs[i].index))){
            	_part_ris.insert(std::pair<int, node>(partition[ntk._storage->outputs[i].index], ntk.index_to_node(ntk._storage->outputs[i].index)));
            }
		  }
        }

        for(int i = 0; i < part_num; i++){
          partitionInputs[i] = create_part_inputs(i);
          partitionReg[i] = create_part_latches(i);
          typename std::set<node>::iterator it;
          partitionRegIn[i] = create_part_latches_in(i);
          partitionOutputs[i] = create_part_outputs(i);
          update_io(i);
        }
        kahypar_context_free(context);
      }
      
    }

  private:

    /***************************************************
    Utility functions to be moved later
    ***************************************************/

    // Helper function to flip the bit
    char flip(char c){
      return (c == '0') ? '1' : '0';
    }

    int get_output_index(Ntk const& ntk, int nodeIdx){

      assert(ntk.is_po(nodeIdx));

      for(int i = 0; i < ntk._storage->outputs.size(); i++){
        if(ntk._storage->outputs.at(i).index == nodeIdx){
          return i;
        }
      }
    }//get_output_index()

    std::vector<int> get_output_indeces(Ntk const& ntk, int nodeIdx){

        assert(ntk.is_po(nodeIdx));
        std::vector<int> indeces;
        for(int i = 0; i < ntk._storage->outputs.size(); i++){
            if(ntk._storage->outputs.at(i).index == nodeIdx){
                indeces.push_back(i);
            }
        }
        return indeces;
    }//get_output_indeces()

    //Simple BFS Traversal to optain the depth of an output's logic cone before the truth table is built
    void BFS_traversal(Ntk const& ntk, node output, int partition){
      std::queue<int> net_queue;
      std::map<int, bool> visited;
      std::set<int> inputs;
      int size = 0;
      //Set all nodes to be unvisited
      ntk.foreach_node( [&]( auto node ) {
        visited[ntk.node_to_index(node)] = false;
      });
      int outputIdx = ntk.node_to_index(output);
      net_queue.push(outputIdx);
      visited[outputIdx] = true;

      while(!net_queue.empty()){

        int curr_node = net_queue.front();
        net_queue.pop();
        auto node = ntk.index_to_node(curr_node);

        //Make sure that the BFS traversal does not go past the inputs of the partition
        if(partitionInputs[partition].find(curr_node) == partitionInputs[partition].end()){

          for(int i = 0; i < ntk._storage->nodes[node].children.size(); i++){

            int childIdx = ntk._storage->nodes[node].children[i].index;
            bool is_valid = true;
            //Make sure a valid child index is found
            if(childIdx < 0){
              is_valid = false;
            }

            if(!visited[childIdx]){

              if(is_valid){

                net_queue.push(childIdx);
                visited[childIdx] = true;
                size++;
              }
            }
          }
        }
        else{
          inputs.insert(curr_node);
        }
      }
      cone_size[output] = size;
      logic_cone_inputs[output] = inputs;
    }//BFS_traversal()

    int computeLevel( Ntk const& ntk, node curr_node, int partition ) {
      //if node not visited
      if(ntk._storage->nodes[curr_node].data[1].h1==0 && !ntk.is_constant(curr_node))  {
        //set node as visited
        ntk._storage->nodes[curr_node].data[1].h1=1;
        //if is input
        if (partitionInputs[partition].find(curr_node) != partitionInputs[partition].end()) {
		  return 0;
        }

        auto inIdx2 = ntk._storage->nodes[curr_node].children[1].data;
		
		if (inIdx2 & 1)
          inIdx2 = inIdx2 - 1;

        //calculate input node index
        auto inNode1 = inIdx2 >> 1;
        int levelNode1 = computeLevel(ntk, inNode1, partition);

        auto inIdx = ntk._storage->nodes[curr_node].children[0].data;
        if (inIdx & 1)
          inIdx = inIdx - 1;

        //calculate input node index
        auto inNode0 = inIdx >> 1;
        int levelNode0 = computeLevel(ntk, inNode0, partition);

        int level = 1 + std::max(levelNode0, levelNode1);
        return level;
      }
    }

    std::string to_binary(int dec){

      std::string bin;
      while(dec != 0){
        bin = (dec % 2 == 0 ? "0":"1") + bin;
        dec /= 2;
      }
      return bin;
    }

    std::string graytoBinary(std::string gray){
      std::string binary = "";

      // MSB of binary code is same as gray code
      binary += gray[0];

      // Compute remaining bits
      for (int i = 1; i < gray.length(); i++) {
        // If current bit is 0, concatenate
        // previous bit
        if (gray[i] == '0')
          binary += binary[i - 1];

          // Else, concatenate invert of
          // previous bit
        else
          binary += flip(binary[i - 1]);
      }

      return binary;
    }

    // Function to convert binary to decimal
    int binaryToDecimal(int n){

      int num = n;
      int dec_value = 0;

      // Initializing base value to 1, i.e 2^0
      int base = 1;

      int temp = num;
      while (temp)
      {
        int last_digit = temp % 10;
        temp = temp/10;

        dec_value += last_digit*base;

        base = base*2;
      }

      return dec_value;
    }

    /***************************************************/

    void tt_build(Ntk const& ntk, int partition, node curr_node, node root){
      int nodeIdx = ntk.node_to_index(curr_node);
      if(logic_cone_inputs[root].find(nodeIdx) != logic_cone_inputs[root].end() || _part_scope[partition].find(curr_node) == _part_scope[partition].end()){
        
        if(logic_cone_inputs[root].find(root) != logic_cone_inputs[root].end()){
          auto output = ntk._storage->outputs.at(get_output_index(ntk,root));
          if(output.data & 1){
            tt_map[nodeIdx] = ~tt_map[nodeIdx];
          }
        }
        return;
      }
            
      std::vector<signal> children;
      ntk.foreach_fanin(curr_node, [&]( auto const& child, auto i){
        children.push_back(child);
      });
      int child1Idx = ntk._storage->nodes[nodeIdx].children[0].index;
      int child2Idx = ntk._storage->nodes[nodeIdx].children[1].index;
            
      for(auto child : children){
        tt_build(ntk, partition, ntk.get_node(child), root);
      }

      if(!ntk.is_constant(nodeIdx) && logic_cone_inputs[root].find(nodeIdx) == logic_cone_inputs[root].end() ){
        
        std::vector<kitty::dynamic_truth_table> child_tts;
        for(auto child : children){
          child_tts.push_back(tt_map[child.index]);
        }

        ntk.foreach_fanin( curr_node, [&]( auto const& conn, auto i ) {
          int childIdx = conn.index;
          if ( ntk.is_complemented( conn )) {
            child_tts.at(i) = ~tt_map[childIdx];
          }

          if(ntk.is_po(childIdx) && logic_cone_inputs[root].find(childIdx) != logic_cone_inputs[root].end()){
            auto output = ntk._storage->outputs.at(get_output_index(ntk,childIdx));
            if(output.data & 1){
              child_tts.at(i) = ~child_tts.at(i);
            }
          }
        });

        kitty::dynamic_truth_table tt;
        if(ntk.fanin_size(curr_node) == 3){
          tt = kitty::ternary_majority(child_tts.at(0), child_tts.at(1), child_tts.at(2));
        }
        else{
          tt = kitty::binary_and(child_tts.at(0), child_tts.at(1));;
        } 
        tt_map[nodeIdx] = tt;
      }
            
      if(ntk.is_po(nodeIdx) && nodeIdx == root){
        auto output = ntk._storage->outputs.at(get_output_index(ntk,nodeIdx));
        if(output.data & 1){
          tt_map[nodeIdx] = ~tt_map[nodeIdx];
        }
      }
    }

  public:
    oracle::partition_view<Ntk> create_part( Ntk const& ntk, int part ){ 
      oracle::partition_view<Ntk> partition(ntk, partitionInputs[part], partitionOutputs[part], partitionReg[part], partitionRegIn[part], false);
      return partition;
    }

    template<class NtkPart, class NtkOpt>
    void synchronize_part(oracle::partition_view<NtkPart> part, NtkOpt const& opt, Ntk &ntk){
      int orig_ntk_size = ntk.size();
      mockturtle::node_map<signal, NtkOpt> old_to_new( opt );
      std::vector<signal> pis;

      part.foreach_pi( [&]( auto node ) {
        pis.push_back(part.make_signal(node));
      });

      mockturtle::topo_view part_top{part};
      mockturtle::topo_view opt_top{opt};

      int pi_idx = 0;
      std::set<signal> visited_pis;
      opt_top.foreach_node( [&]( auto node ) {
        if(opt_top.is_po(node)){
        }
        if ( opt.is_constant( node ) || opt.is_pi( node ) || opt.is_ro( node ))
          return;
        // std::cout << "Node = " << node << "\n";
        /* collect children */
        std::vector<signal> children;
        opt.foreach_fanin( node, [&]( auto child, auto ) {
          const auto f = old_to_new[child];
          if(opt.is_pi(opt.get_node(child)) || opt.is_ro(opt.get_node(child))){
            f = pis.at(child.index - 1);
          }
          if ( opt.is_complemented( child ) )
          {
            children.push_back( ntk.create_not( f ) );
          }
          else
          {
            children.push_back( f );
          }
        } );

        old_to_new[node] = ntk.clone_node( opt, node, children );
      });

      for(int i = 0; i < opt._storage->outputs.size(); i++){
        auto opt_node = opt.get_node(opt._storage->outputs.at(i));
        auto opt_out = old_to_new[opt._storage->outputs.at(i)];
        auto part_out = part._roots.at(i);
        if(opt.is_complemented(opt._storage->outputs[i])){
          opt_out.data += 1;

        }

        if(!opt.is_constant(opt_node) && !opt.is_pi(opt_node) && !opt.is_ro(opt_node)){
          output_substitutions[ntk.get_node(part_out)] = opt_out;
        }
      }
    }

    void generate_truth_tables(Ntk const& ntk){
      for(int i = 0; i < num_partitions; i++){                  
        typename std::set<node>::iterator it;
        for(it = partitionOutputs[i].begin(); it != partitionOutputs[i].end(); ++it){
          auto curr_output = *it;   
          BFS_traversal(ntk, curr_output, i); 
          if(ntk.is_constant(curr_output)){
            std::cout << "CONSTANT\n";
          }
          else if(logic_cone_inputs[curr_output].size() <= 16 && !ntk.is_constant(curr_output)){
            int idx = 0;
            std::set<int>::iterator input_it;
            for(input_it = logic_cone_inputs[curr_output].begin(); input_it != logic_cone_inputs[curr_output].end(); ++input_it){
              int nodeIdx = *input_it;
              kitty::dynamic_truth_table tt( logic_cone_inputs[curr_output].size() );
              kitty::create_nth_var(tt, idx);
                                  
              tt_map[nodeIdx] = tt;
              idx++;
            }

            tt_build(ntk, i, curr_output, curr_output);
            output_tt[curr_output] = tt_map[curr_output];
            ntk.foreach_node( [&]( auto node ) {
              int index = ntk.node_to_index(node);
              ntk._storage->nodes[index].data[1].h1 = 0;
            });        
          }
          else{
            std::cout << "Logic Cone too big at " << logic_cone_inputs[curr_output].size() << " inputs\n";
          }
        }
      }
    }

    std::vector<float> get_km_image( Ntk const& ntk, int partition, node output ){

      std::vector<float> default_image;
      BFS_traversal(ntk, output, partition);
      int num_inputs = logic_cone_inputs[output].size();
      ntk.foreach_node( [&]( auto node ) {
        int index = ntk.node_to_index(node);
        ntk._storage->nodes[index].data[1].h1 = 0;
      });

      std::string tt = kitty::to_binary(output_tt[output]);
      char* tt_binary = malloc(sizeof(char) * (tt.length() + 1));
      strcpy(tt_binary, tt.c_str());

      std::vector<std::string> onset_indeces;
      int indx = 0;
      for(int k = tt.length() - 1; k >= 0; k--){
        int bit = (int)tt_binary[k] - 48;
        if(bit == 1){
          onset_indeces.push_back(to_binary(indx));
        }
        indx++;
      }
      for(int k = 0; k < onset_indeces.size(); k++){
        while(onset_indeces.at(k).length() != logic_cone_inputs[output].size()){
          onset_indeces.at(k).insert(0, "0");
        }
        std::reverse(onset_indeces.at(k).begin(), onset_indeces.at(k).end());
      }
      int columns = num_inputs / 2;
      int rows;
      if(num_inputs <= 16 && num_inputs >= 2){
        if(num_inputs % 2 != 0){
          rows = columns + 1;
        }
        else{
          rows = columns;
        }

        int row_num = pow(2, rows);
        int col_num = pow(2, columns);
        char **k_map = malloc(sizeof(char *) * col_num);
        for(int y = 0; y < col_num; y++)
          k_map[y] = malloc(sizeof(char) * row_num);

        for(int y = 0; y < col_num; y++){
          for(int x = 0; x < row_num; x++){
            k_map[y][x] = 0;
          }
        }
        for(int k = 0; k < onset_indeces.size(); k++){

          std::string row_index_gray = onset_indeces.at(k).substr(0, rows);
          std::string col_index_gray = onset_indeces.at(k).substr(rows, onset_indeces.at(k).size() - 1);
          std::string row_index_bin = graytoBinary(row_index_gray);
          std::string col_index_bin = graytoBinary(col_index_gray);
          int row_index = std::stoi(row_index_bin,nullptr,2);
          int col_index = std::stoi(col_index_bin,nullptr,2);
          k_map[col_index][row_index] = 2;

        }
        if(num_inputs < 16){
          int padded_row = 256;
          int padded_col = 256;
          char **k_map_pad = malloc(sizeof(char *) * padded_col);
          for(int k = 0; k < padded_col; k++){
            k_map_pad[k] = malloc(sizeof(char) * padded_row);
          }

          for(int y = 0; y < padded_col; y++){
            for(int x = 0; x < padded_row; x++){
              k_map_pad[y][x] = 1;
            }
          }
          int row_offset = (padded_row - row_num);
          if(row_offset % 2 != 0){
            row_offset++;
          }
          int col_offset = (padded_col - col_num);
          if(col_offset % 2 != 0){
            col_offset++;
          }
          row_offset /= 2;
          col_offset /= 2;
          for(int y = 0; y < col_num; y++){
            for(int x = 0; x < row_num; x++){
              k_map_pad[y + col_offset][x + row_offset] = k_map[y][x];
            }
          }
          std::vector<float> data_1d(padded_row * padded_col);
          for(int y = 0; y < padded_col; y++){
            for(int x = 0; x < padded_row; x++){
              data_1d[x + y*padded_col] = (float)k_map_pad[y][x];
            }
          }
          return data_1d;
        }
        else{
          std::vector<float> data_1d(row_num * col_num);
          for(int y = 0; y < col_num; y++){
            for(int x = 0; x < row_num; x++){
              data_1d[x + y*col_num] = (float)k_map[y][x];
            }
          }
          return data_1d;
        }
      }
      return default_image;
    }

    void run_classification( Ntk const& ntk, std::string model_file ){

      int row_num = 256;
      int col_num = 256;
      int chann_num = 1;
      std::vector<std::string> labels = {"AIG", "MIG"};
      const auto model = fdeep::load_model(model_file);

      if(output_tt.empty()){
        generate_truth_tables(ntk);
      }

      for(int i = 0; i < num_partitions; i++){
        int aig_score = 0;
        int mig_score = 0;

        int partition = i;
        auto total_outputs = 0;
        auto total_depth = 0;
        auto weight = 1.3;
        auto weight_nodes = 1;
        auto average_nodes = 0;
        auto average_depth = 0;

        mockturtle::depth_view ntk_depth{ntk};

        typename std::set<node>::iterator it;
        for(it = partitionOutputs[i].begin(); it != partitionOutputs[i].end(); ++it){
          auto output = *it;
		  if(ntk.is_constant(output)) continue;  	
          	total_depth += computeLevel(ntk, output, partition);
          	total_outputs++;
        }
        if(total_outputs>0) {
           average_nodes = _num_nodes_cone / total_outputs;
           average_depth = total_depth / total_outputs;
        }

        for(it = partitionOutputs[i].begin(); it != partitionOutputs[i].end(); ++it){
          auto output = *it;
          _num_nodes_cone = 0;
          std::vector<float> image = get_km_image(ntk, partition, output);
          //std::cout << "received image of size = " << image.size() << "\n";
          if(image.size() > 0){
            const fdeep::shared_float_vec sv(fplus::make_shared_ref<fdeep::float_vec>(std::move(image)));
            fdeep::tensor5 input(fdeep::shape5(1, 1, row_num, col_num, chann_num), sv);
            const auto result = model.predict_class({input});
            //std::cout << "Result\n";
            //std::cout << labels.at(result) << "\n";

            weight = 1;
            weight_nodes = 1;

            if(result == 0){
              int num_inputs = logic_cone_inputs[output].size();

              int depth = computeLevel(ntk, output, partition);
              if(depth > average_depth && average_depth > 0 ){
                if(depth > average_depth + 1)
                  weight = 2;
                weight = 1.3;

                if(depth > average_depth + 2 && average_depth > 0  )
                  weight = 3;
              }

              if(_num_nodes_cone > average_nodes && average_nodes > 0  ) {
                weight_nodes = 1.5;
              }

              aig_score += ((weight_nodes*_num_nodes_cone)+(weight*depth));
            }

            else{
              int num_inputs = logic_cone_inputs[output].size();

              int depth = computeLevel(ntk, output, partition);

              if(depth > average_depth && average_depth > 0 ){
                if(depth > average_depth + 1 && average_depth > 0  )
                  weight = 2;
                if(depth > average_depth + 2 && average_depth > 0  )
                  weight = 3;
                weight = 1.3;
              }

              if(_num_nodes_cone > average_nodes && average_nodes > 0  ) {
                weight_nodes = 1.5;
              }

              mig_score += ( (weight_nodes*_num_nodes_cone)+(weight*depth));
            }
          }
          else{
            // std::cout << "Dealing with big cone with " << logic_cone_inputs[output].size() << " inputs" << std::endl;

            _num_nodes_cone = 0;
            int big_depth = computeLevel(ntk, output, partition);
            // std::cout << "computed depth: " << big_depth << "\n";
            // std::cout << "network depth: " << ntk_depth.depth() << "\n";
            if (big_depth > 0.4 * ntk_depth.depth())
              mig_score += ( (weight_nodes*_num_nodes_cone)+(3*big_depth));

            else aig_score += ( (weight_nodes*_num_nodes_cone)+(3*big_depth));

            // std::cout << "updated score\n";
          }
        }
        if(aig_score > mig_score){
          aig_parts.push_back(partition);
        }
        else{
          mig_parts.push_back(partition);
        }
      }
    }

    void write_karnaugh_maps( Ntk const& ntk, std::string directory ){

      if(output_tt.empty()){
        generate_truth_tables(ntk);
      }

      mkdir(directory.c_str(), 0777);
      for(int i = 0; i < num_partitions; i++){
        int partition = i;
        typename std::set<node>::iterator it;
        for(it = partitionOutputs[i].begin(); it != partitionOutputs[i].end(); ++it){
          auto output = *it;
          BFS_traversal(ntk, output, partition);
          int num_inputs = logic_cone_inputs[output].size();
          ntk.foreach_node( [&]( auto node ) {
            int index = ntk.node_to_index(node);
            ntk._storage->nodes[index].data[1].h1 = 0;
          });
          int logic_depth = computeLevel(ntk, output, partition);

          // std::string file_out = ntk._storage->net_name + "_kar_part_" + std::to_string(partition) + "_out_" +
          //                        std::to_string(output) + "_in_" + std::to_string(num_inputs) + "_lev_" + std::to_string(logic_depth) + ".txt";

          std::string file_out = "top_kar_part_" + std::to_string(partition) + "_out_" +
                                 std::to_string(output) + "_in_" + std::to_string(num_inputs) + "_lev_" + std::to_string(logic_depth) + ".txt";


          std::string tt = kitty::to_binary(output_tt[output]);
          char* tt_binary = malloc(sizeof(char) * (tt.length() + 1));
          strcpy(tt_binary, tt.c_str());

          std::vector<std::string> onset_indeces;

          int indx = 0;
          for(int k = tt.length() - 1; k >= 0; k--){
            int bit = (int)tt_binary[k] - 48;
            if(bit == 1){
              onset_indeces.push_back(to_binary(indx));
            }
            indx++;
          }
          for(int k = 0; k < onset_indeces.size(); k++){
            while(onset_indeces.at(k).length() != logic_cone_inputs[output].size()){
              onset_indeces.at(k).insert(0, "0");
            }
            std::reverse(onset_indeces.at(k).begin(), onset_indeces.at(k).end());
          }
          
          int columns = num_inputs / 2;
          int rows;
          if(num_inputs <= 16 && num_inputs >= 2){
            std::ofstream output_file(directory + file_out, std::ios::out | std::ios::binary | std::ios::trunc);
            if(num_inputs % 2 != 0){
              rows = columns + 1;
            }
            else{
              rows = columns;
            }

            int row_num = pow(2, rows);
            int col_num = pow(2, columns);
            char **k_map = malloc(sizeof(char *) * col_num);
            for(int y = 0; y < col_num; y++)
              k_map[y] = malloc(sizeof(char) * row_num);

            for(int y = 0; y < col_num; y++){
              for(int x = 0; x < row_num; x++){
                k_map[y][x] = 0;
              }
            }

            for(int k = 0; k < onset_indeces.size(); k++){

              std::string row_index_gray = onset_indeces.at(k).substr(0, rows);
              std::string col_index_gray = onset_indeces.at(k).substr(rows, onset_indeces.at(k).size() - 1);
              std::string row_index_bin = graytoBinary(row_index_gray);
              std::string col_index_bin = graytoBinary(col_index_gray);
              int row_index = std::stoi(row_index_bin,nullptr,2);
              int col_index = std::stoi(col_index_bin,nullptr,2);
              k_map[col_index][row_index] = 2;

            }

            if(num_inputs < 16){
              int padded_row = 256;
              int padded_col = 256;
              char **k_map_pad = malloc(sizeof(char *) * padded_col);
              for(int k = 0; k < padded_col; k++){
                k_map_pad[k] = malloc(sizeof(char) * padded_row);
              }

              for(int y = 0; y < padded_col; y++){
                for(int x = 0; x < padded_row; x++){
                  k_map_pad[y][x] = 1;
                }
              }
              int row_offset = (padded_row - row_num);
              if(row_offset % 2 != 0){
                row_offset++;
              }
              int col_offset = (padded_col - col_num);
              if(col_offset % 2 != 0){
                col_offset++;
              }
              row_offset /= 2;
              col_offset /= 2;
              for(int y = 0; y < col_num; y++){
                for(int x = 0; x < row_num; x++){
                  k_map_pad[y + col_offset][x + row_offset] = k_map[y][x];
                }
              }
              std::vector<char> data_1d(padded_row * padded_col);
              for(int y = 0; y < padded_col; y++){
                for(int x = 0; x < padded_row; x++){
                  data_1d[x + y*padded_col] = k_map_pad[y][x];
                }
              }

              output_file.write(data_1d.data(), data_1d.size()*sizeof(char));
            }
            else{
              std::vector<char> data_1d(row_num * col_num);
              for(int y = 0; y < col_num; y++){
                for(int x = 0; x < row_num; x++){
                  data_1d[x + y*col_num] = k_map[y][x];
                }
              }
              output_file.write(data_1d.data(), data_1d.size()*sizeof(char));
            }
            output_file.close();
          }

        }
      }
    }

    // Ntk create_ntk_from_part(Ntk const& ntk, int partition, node output){

    //   Ntk new_ntk;
    //   new_ntk._storage->net_name = ntk._storage->net_name + "_" + std::to_string(partition) + "_" + std::to_string(ntk.node_to_index(output));
    //   std::vector<node> index;

    //   //BFS Traversal of output
    //   std::queue<node> net_queue;
    //   std::map<node, bool> visited;
    //   //Set all nodes to be unvisited
    //   ntk.foreach_node( [&]( auto curr_node ) {
    //     visited[curr_node] = false;
    //   });

    //   net_queue.push(output);
    //   visited[output] = true;
            
    //   while(!net_queue.empty()){

    //     auto curr_node = net_queue.front();
    //     //make sure there are no duplicates added
    //     if(std::find(index.begin(),index.end(),curr_node) == index.end() && !ntk.is_constant(curr_node)){
    //       //Put inputs at the beginning of the index so they are added into the AIG first 
    //       if(partitionInputs[partition].find(curr_node) != partitionInputs[partition].end())
    //         index.insert(index.begin(), curr_node);
    //       else
    //         index.push_back(curr_node);
    //     }
    //     net_queue.pop();

    //     //Make sure that the BFS traversal does not go past the inputs of the partition
    //     if(partitionInputs[partition].find(curr_node) == partitionInputs[partition].end()){

    //       for(int i = 0; i < ntk._storage->nodes[curr_node].children.size(); i++){

    //         int childIdx = ntk._storage->nodes[curr_node].children[i].index;

    //         if(!visited[childIdx]){
    //           net_queue.push(childIdx);
    //           visited[childIdx] = true;
    //         }
    //       }               
                    
    //     }
    //   }
      
    //   for(int i = 0; i < index.size(); i++){
    //     auto curr_node = index.at(i);
    //     if(ntk.is_constant(curr_node)){
    //       new_ntk.get_constant( ntk.constant_value(curr_node) );
    //     }
    //     //outputs tied directly to output
    //     else if(partitionInputs[partition].find(curr_node) != partitionInputs[partition].end() && partitionOutputs[partition].find(curr_node) != partitionOutputs[partition].end()){

    //       auto pi = new_ntk.create_pi();

    //       if(ntk.is_complemented(ntk.make_signal(curr_node))){
    //         pi = ntk.create_not(pi);
    //       }

    //       new_ntk.create_po(pi); 
                    
    //     }
    //     //create pi
    //     else if(partitionInputs[partition].find(curr_node) != partitionInputs[partition].end()){
    //       auto pi = new_ntk.create_pi();
    //     }
    //     else{

    //       /* collect children */
    //       std::vector<signal> children;

    //       ntk.foreach_fanin( curr_node, [&]( auto child, auto ) {
    //         if(!ntk.is_constant(ntk.get_node(child))){
    //           typename std::vector<node>::iterator child_it;
    //           child_it = std::find (index.begin(), index.end(), ntk.get_node(child));
    //           int childIdx = std::distance(index.begin(), child_it);
    //           auto child_signal = ntk.make_signal(ntk.index_to_node(childIdx + 1));
    //           if ( ntk.is_complemented( child ) ){
    //             children.push_back( new_ntk.create_not( child_signal ) );
    //           }
    //           else{
    //             children.push_back( child_signal );
    //           }
    //         }
    //         else{
    //           if ( ntk.is_complemented( child ) ){
    //             children.push_back( new_ntk.create_not( child ) );
    //           }
    //           else{
    //             children.push_back( child );
    //           }
    //         }
    //       });
    //       auto gate = new_ntk.clone_node(ntk, curr_node, children);
                
    //       if(partitionOutputs[partition].find(curr_node) != partitionOutputs[partition].end()){       
    //         if(ntk.is_po(curr_node)){

    //           if(ntk.is_complemented(ntk.make_signal(curr_node))){
    //             gate = new_ntk.create_not(gate);
    //           }

    //           new_ntk.create_po(gate);
    //         }           
    //         else{
    //           new_ntk.create_po(gate);
    //         }
                        
    //       }
    //     }
    //   }

    //   return new_ntk;

    // }//create_aig_from_part()

    void connect_outputs(Ntk ntk){
      for(auto it = output_substitutions.begin(); it != output_substitutions.end(); ++it){
        ntk.substitute_node(it->first, it->second);
      }
    }

    std::set<node> create_part_outputs(int part_index){
      std::set<node> outputs;
      auto range = _part_pos.equal_range(part_index);
      for(auto i = range.first; i!=range.second;++i){
        if (std::find(outputs.begin(), outputs.end(), i->second) == outputs.end()) {
          outputs.insert(i->second);
        }
      }
      return outputs;
    }

    std::set<node> create_part_latches(int part_index){
        std::set<node> latches;
        auto range = _part_ros.equal_range(part_index);
        for(auto i = range.first; i!=range.second;++i){
          if (std::find(latches.begin(), latches.end(), i->second) == latches.end()) {
            latches.insert(i->second);
          }
        }
        return latches;
      }

     std::set<node> create_part_latches_in(int part_index){
      std::set<node> latches_in;
      auto range = _part_ris.equal_range(part_index);
      for(auto i = range.first; i!=range.second;++i){
        if (std::find(latches_in.begin(), latches_in.end(), i->second) == latches_in.end()) {
          latches_in.insert(i->second);
        }
      }
      return latches_in;
    }

    std::set<node> create_part_inputs(int part_index){
      std::set<node> inputs;
      auto range = _part_pis.equal_range(part_index);
      for(auto i = range.first; i!=range.second;++i){
        if (std::find(inputs.begin(), inputs.end(), i->second) == inputs.end()) {
          inputs.insert(i->second);
        }
      }
      return inputs;
    }

    void update_io(int part_index){
      typename std::set<node>::iterator it;
      for(it = partitionInputs[part_index].begin(); it != partitionInputs[part_index].end(); ++it){
        input_partition[*it].push_back(part_index);
      }
      for(it = partitionOutputs[part_index].begin(); it != partitionOutputs[part_index].end(); ++it){
        output_partition[*it].push_back(part_index);
      }
    }

    std::set<node> get_shared_io(int part_1, int part_2){
      std::set<node> part_1_inputs = partitionInputs[part_1];
      std::set<node> part_1_outputs = partitionOutputs[part_1];

      std::set<node> part_2_inputs = partitionInputs[part_2];
      std::set<node> part_2_outputs = partitionOutputs[part_2];

      std::set<node> shared_io;
      typename std::set<node>::iterator it_i;
      typename std::set<node>::iterator it_j;
      for(it_i = part_1_inputs.begin(); it_i != part_1_inputs.end(); ++it_i){
        for(it_j = part_2_outputs.begin(); it_j != part_2_outputs.end(); ++it_j){
          if(*it_i == *it_j){
            shared_io.insert(*it_i);
          }
        }
      }
      for(it_i = part_1_outputs.begin(); it_i != part_1_outputs.end(); ++it_i){
        for(it_j = part_2_inputs.begin(); it_j != part_2_inputs.end(); ++it_j){
          if(*it_i == *it_j){
            shared_io.insert(*it_i);
          }
        }
      }
      return shared_io;
    }

    std::vector<std::set<node>> combine_partitions(Ntk const& ntk, int part_1, int part_2){
      std::set<node> shared_io = get_shared_io(part_1, part_2);
      std::set<node> shared_history;

      std::set_union(shared_io.begin(), shared_io.end(),
                     combined_deleted_nodes[part_1].begin(), combined_deleted_nodes[part_1].end(),
                     std::inserter(shared_history, shared_history.end()));
      typename std::set<node>::iterator it;

      std::set<node> merged_inputs;
      std::set<node> merged_outputs;
      std::vector<std::set<node>> result_io;

      std::set_union(partitionInputs[part_1].begin(), partitionInputs[part_1].end(),
                     partitionInputs[part_2].begin(), partitionInputs[part_2].end(),
                     std::inserter(merged_inputs, merged_inputs.end()));

      std::set_union(partitionOutputs[part_1].begin(), partitionOutputs[part_1].end(),
                     partitionOutputs[part_2].begin(), partitionOutputs[part_2].end(),
                     std::inserter(merged_outputs, merged_outputs.end()));
      // std::cout << part_2 << " inputs = {";
      for(it = partitionInputs[part_2].begin(); it != partitionInputs[part_2].end(); ++it){
        // std::cout << *it << " ";
        for(int i = 0; i < input_partition[*it].size(); i++){
          if(input_partition[*it].at(i) = part_2){
            // std::cout << "in partition " << input_partition[*it].at(i) << "\n";
            input_partition[*it].at(i) = part_1;
          }
        }
        
      }
      // std::cout << "}\n";

      // std::cout << part_2 << " outputs = {";
      for(it = partitionOutputs[part_2].begin(); it != partitionOutputs[part_2].end(); ++it){
        // std::cout << *it << " ";
        if(_part_nodes[*it] == part_2)
          _part_nodes[*it] = part_1;
      }
      // std::cout << "}\n";

      merged_inputs.erase(ntk.index_to_node(0));
      for(it = shared_history.begin(); it != shared_history.end(); ++it){
        node shared_node = *it;
        // std::cout << "shared node = " << shared_node << "\n";
        if(!ntk.is_pi(shared_node)){
          // std::cout << "erasing " << shared_node << "\n";
          merged_inputs.erase(shared_node);
        }
        // std::cout << "shared node = " << shared_node << "\n";
        if(!ntk.is_po(shared_node)){
          // std::cout << "erasing " << shared_node << "\n";
          merged_outputs.erase(shared_node);
        }

        if(combined_deleted_nodes[part_1].find(shared_node) == combined_deleted_nodes[part_1].end() && 
          !ntk.is_pi(shared_node) && !ntk.is_po(shared_node)){

          combined_deleted_nodes[part_1].insert(shared_node);
        }
        
      }

      result_io.push_back(merged_inputs);
      result_io.push_back(merged_outputs);

      return result_io;
    }

    int get_part_num(){
      return num_partitions;
    }

    std::set<node> get_part_outputs(int partition){
      return partitionOutputs[partition];
    }

    void set_part_outputs(int partition, std::set<node> new_outputs){
      partitionOutputs[partition] = new_outputs;
    }

    std::set<node> get_part_inputs(int partition){
      return partitionInputs[partition];
    }

    void set_part_inputs(int partition, std::set<node> new_inputs){
      partitionInputs[partition] = new_inputs;
    }

    std::vector<std::set<node>> get_all_part_connections (){
      return _part_scope;
    }

    std::unordered_map<int, std::set<node>> get_all_partition_inputs(){
      return partitionInputs;
    }

    std::unordered_map<int, std::set<node>> get_all_partition_outputs(){
      return partitionOutputs;
    }

    std::unordered_map<int, std::set<node>> get_all_partition_regs(){
      return partitionReg;
    }

    std::unordered_map<int, std::set<node>> get_all_partition_regin(){
      return partitionRegIn;
    }

    std::set<node> get_part_context (int partition_num){
      return _part_scope[partition_num];
    }

    std::vector<int> get_aig_parts(){
      return aig_parts;
    }
    std::vector<int> get_mig_parts(){
      return mig_parts;
    }

    std::set<int> get_connected_parts( Ntk const& ntk, int partition_num ){
      std::set<int> conn_parts;
      typename std::set<node>::iterator it;
      // std::cout << "Partition " << partition_num << " Inputs:\n";
      for(it = partitionInputs[partition_num].begin(); it != partitionInputs[partition_num].end(); ++it){
        // std::cout << *it << "\n";
        for(int i = 0; i < output_partition[*it].size(); i++){
          if(output_partition[*it].at(i) != partition_num && !ntk.is_pi(*it)){
            // std::cout << "in partition = " << output_partition[*it].at(i) << "\n";
            conn_parts.insert(output_partition[*it].at(i));
          }
        }
      }
      // std::cout << "Partition " << partition_num << " Outputs:\n";
      for(it = partitionOutputs[partition_num].begin(); it != partitionOutputs[partition_num].end(); ++it){
        // std::cout << *it <<  "\n";
        for(int i = 0; i < input_partition[*it].size(); i++){
          if(input_partition[*it].at(i) != partition_num && !ntk.is_pi(*it)){
            // std::cout << "in partition " << input_partition[*it].at(i) << "\n";
            conn_parts.insert(input_partition[*it].at(i));
          }
        }
        // if(_part_nodes[*it] != partition_num && !ntk.is_pi(*it)){
          
        // }
      }

      return conn_parts;
    }

    std::vector<int> get_input_part(node curr_node){
      return input_partition[curr_node];
    }
    std::vector<int> get_output_part(node curr_node){
      return output_partition[curr_node];
    }

  private:
    int num_partitions = 0;

    std::unordered_map<node, int> _part_nodes;
    std::multimap<int, node> _part_pis;
    std::multimap<int, node> _part_pos;
    std::multimap<int, node> _part_ros;
    std::multimap<int, node> _part_ris;

    std::vector<std::set<node>> _part_scope;
    int _num_nodes_cone;

    std::unordered_map<int, std::set<node>> combined_deleted_nodes;

    std::vector<int> aig_parts;
    std::vector<int> mig_parts;

    std::unordered_map<int, std::set<int>> conn_parts;
    std::unordered_map<node, std::vector<int>> input_partition;
    std::unordered_map<node, std::vector<int>> output_partition;

    std::unordered_map<int, std::set<node>> partitionOutputs;
    std::unordered_map<int, std::set<node>> partitionInputs;
    std::unordered_map<int, std::set<node>> partitionReg;
    std::unordered_map<int, std::set<node>> partitionRegIn;

    std::unordered_map<node, signal> output_substitutions;

    std::map<int, int> output_cone_depth;
    std::unordered_map<node, std::set<int>> logic_cone_inputs;
    std::unordered_map<node, int> cone_size;

    std::map<int,kitty::dynamic_truth_table> tt_map;
    std::map<int,kitty::dynamic_truth_table> output_tt;

  };
} /* namespace oracle */
