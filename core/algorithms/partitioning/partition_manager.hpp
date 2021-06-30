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
#include <queue>

#include <mockturtle/traits.hpp>
#include "partition_view.hpp"
#include "structure_partition.hpp"
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

    partition_manager(Ntk& ntk, std::map<node, int> partition, int part_num) {
      num_partitions = part_num;
      for(int i = 0; i < part_num; ++i)
        _part_scope.push_back(std::set<node>());

      ntk.foreach_node( [&](auto curr_node){
        //get rid of circuit PIs
        if (ntk.is_pi(curr_node) ) {
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pis.insert(std::pair<int, node>(partition[curr_node], curr_node));
        }

        if (ntk.is_ro(curr_node) && !ntk.is_constant(curr_node)) {
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pis.insert(std::pair<int, node>(partition[curr_node], curr_node));
          if(ntk.is_po(curr_node) && !ntk.is_constant(curr_node)){
            _part_pos.insert(std::pair<int, node>(partition[curr_node], curr_node));
          }
        }
        //get rid of circuit POs
        else if (ntk.is_po(curr_node) && !ntk.is_constant(curr_node)) {
          _part_scope[partition[curr_node]].insert(curr_node);
          _part_pos.insert(std::pair<int, node>(partition[curr_node], curr_node));
        }
        else if (!ntk.is_constant(curr_node)) {
          _part_scope[partition[curr_node]].insert(curr_node);
        }

        //look to partition inputs (those that are not circuit PIs)
        if (!ntk.is_pi(curr_node) && !ntk.is_ro(curr_node)){
          ntk.foreach_fanin(curr_node, [&](auto const &conn, auto j) {
            if (partition[ntk.index_to_node(conn.index)] != partition[curr_node] && !ntk.is_constant(ntk.index_to_node(conn.index))) {
              _part_scope[partition[curr_node]].insert(curr_node);
              _part_pis.insert(std::pair<int, node>(partition[curr_node], ntk.index_to_node(conn.index)));
              _part_pos.insert(std::pair<int, node>(partition[ntk.index_to_node(conn.index)],ntk.index_to_node(conn.index)));

            }
          });
        }
      });
      for(int i = 0; i < part_num; i++){
        partitionInputs[i] = create_part_inputs(i);
        typename std::set<node>::iterator it;
        partitionOutputs[i] = create_part_outputs(i);
        update_io(i);
      }
    }

    partition_manager(Ntk& ntk, std::vector<std::set<node>> scope, std::unordered_map<int, std::set<node>> inputs,
      std::unordered_map<int, std::set<node>> outputs, std::unordered_map<int, std::set<node>> regs, std::unordered_map<int, std::set<node>> regs_in, int part_num){

      num_partitions = part_num;
      _part_scope = scope;
      partitionInputs = inputs;
      partitionOutputs = outputs;
      partitionReg = regs;
      partitionRegIn = regs_in;
    }

    partition_manager( Ntk& ntk, int part_num, std::string config_direc="../../core/test.ini", kahypar_hypernode_weight_t *hypernode_weights=nullptr, kahypar_hyperedge_weight_t *hyperedge_weights=nullptr, bool sap=false, double imbalance = 0.9) : Ntk( ntk )
    {
      static_assert( mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type" );
      static_assert( mockturtle::has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
      static_assert( mockturtle::has_visited_v<Ntk>, "Ntk does not implement the visited method" );
      static_assert( mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
      static_assert( mockturtle::has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
      static_assert( mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
      static_assert( mockturtle::has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );

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

        hypergraph<Ntk> t(ntk);
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
        if (hyperedge_weights == nullptr) {
          hyperedge_weights = new kahypar_hyperedge_weight_t[kahyp_num_vertices];
          for( int i = 0; i < kahyp_num_vertices; i++ ) {
            hyperedge_weights[i] = 2;
          }
        }

        //vector with indeces where each set starts
        std::unique_ptr<size_t[]> hyperedge_indices = std::make_unique<size_t[]>(kahyp_num_sets+1);

        for ( int j = 0; j < kahyp_num_sets+1; j++){
          hyperedge_indices[j] = kahyp_set_indeces[j];
        }

        std::unique_ptr<kahypar_hyperedge_id_t[]> hyperedges = std::make_unique<kahypar_hyperedge_id_t[]>(kahyp_num_indeces_hyper);

        for ( int i = 0; i < kahyp_num_indeces_hyper; i++){
          hyperedges[i] = kahypar_connections[i];
        }

        const kahypar_partition_id_t k = part_num;

        kahypar_hyperedge_weight_t objective = 0;

        std::vector<kahypar_partition_id_t> partition(num_vertices, -1);
        kahypar_hypergraph_t *hypergraph = kahypar_create_hypergraph(k,
                                                                     num_vertices,
                                                                     num_hyperedges,
                                                                     hyperedge_indices.get(),
                                                                     hyperedges.get(),
                                                                     hyperedge_weights,
                                                                     hypernode_weights);
        initial_partitions.resize(num_vertices, -1);
        if (sap) {
          std::vector<kahypar_partition_id_t> init_part(num_vertices, -1);
          structure_partition<Ntk> sap(ntk);
          uint32_t avg_part = ntk.size()/part_num;
          double max_bin = (double) sap.sap_fixed(init_part, avg_part, 3);
          kahypar_set_fixed_vertices(hypergraph, init_part.data());
          imbalance = std::max(imbalance, 1.5 * 1 - (double)max_bin/avg_part);
          std::cout << "Average partition size " << avg_part << std::endl;
          std::cout << "Max bin size " << max_bin << std::endl;
          std::cout << "Requested epsilon " << imbalance << std::endl;
          for (int i = 0; i < num_vertices; i++) {
            initial_partitions[i] = init_part[i];
          }
        }


        kahypar_partition_hypergraph(hypergraph, k, imbalance, &objective, context, partition.data());

        partition_results.resize(num_vertices, -1);
        for (int i = 0; i < num_vertices; i++) {
          partition_results[i] = partition[i];
        }

        for(auto i=1; i <= ntk.num_pis(); i++){
          if(i<=ntk.num_pis()-ntk.num_latches()){
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

    //Simple BFS Traversal to optain the depth of an output's logic cone before the truth table is built
    void BFS_traversal(Ntk& ntk, node output, int partition){
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

    void tt_build(Ntk& ntk, int partition, node curr_node, node root){
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
    partition_view<Ntk> create_part( Ntk& ntk, int part ){
      partition_view<Ntk> partition(ntk, partitionInputs[part], partitionOutputs[part], partitionReg[part], partitionRegIn[part], false);
      return partition;
    }

    template<class NtkPart, class NtkOpt>
    void synchronize_part(partition_view<NtkPart> part, NtkOpt& opt, Ntk &ntk){
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
        /* collect children */
        std::vector<signal> children;
        opt.foreach_fanin( node, [&]( auto child, auto ) {
          auto f = old_to_new[child];
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

    void generate_truth_tables(Ntk& ntk){

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

    std::vector<float> get_km_image( Ntk& ntk, int partition, node output ){

      std::vector<float> default_image;
      BFS_traversal(ntk, output, partition);
      int num_inputs = logic_cone_inputs[output].size();
      ntk.foreach_node( [&]( auto node ) {
        int index = ntk.node_to_index(node);
        ntk._storage->nodes[index].data[1].h1 = 0;
      });

      std::string tt = kitty::to_binary(output_tt[output]);
      char* tt_binary = (char*)malloc(sizeof(char) * (tt.length() + 1));
      strcpy(tt_binary, tt.c_str());

      std::vector<std::string> onset_indeces;
      int indx = 0;
      for(int k = tt.length() - 1; k >= 0; k--){
        int bit = (int)tt_binary[k] - 48;
        if(bit == 1){
          onset_indeces.push_back(oracle::to_binary(indx));
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
        char **k_map = (char**)malloc(sizeof(char *) * col_num);
        for(int y = 0; y < col_num; y++)
          k_map[y] = (char*)malloc(sizeof(char) * row_num);

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
          char **k_map_pad = (char**)malloc(sizeof(char *) * padded_col);
          for(int k = 0; k < padded_col; k++){
            k_map_pad[k] = (char*)malloc(sizeof(char) * padded_row);
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

    void run_classification( Ntk& ntk, std::string model_file ){

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
		      if(ntk.is_constant(output))
            continue;
        	total_depth += computeLevel(ntk, output, partitionInputs[partition]);
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
          if(image.size() > 0){
            const fdeep::shared_float_vec sv(fplus::make_shared_ref<fdeep::float_vec>(std::move(image)));
            fdeep::tensor5 input(fdeep::shape5(1, 1, row_num, col_num, chann_num), sv);
            const auto result = model.predict_class({input});

            weight = 1;
            weight_nodes = 1;

            if(result == 0){
              int num_inputs = logic_cone_inputs[output].size();

              int depth = computeLevel(ntk, output, partitionInputs[partition]);
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

              int depth = computeLevel(ntk, output, partitionInputs[partition]);

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
            _num_nodes_cone = 0;
            int big_depth = computeLevel(ntk, output, partitionInputs[partition]);
            if (big_depth > 0.4 * ntk_depth.depth())
              mig_score += ( (weight_nodes*_num_nodes_cone)+(3*big_depth));
            else
              aig_score += ( (weight_nodes*_num_nodes_cone)+(3*big_depth));
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

    void write_karnaugh_maps( Ntk& ntk, std::string directory ){

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
          int logic_depth = computeLevel(ntk, output, partitionInputs[partition]);

          std::string file_out = "top_kar_part_" + std::to_string(partition) + "_out_" +
                                 std::to_string(output) + "_in_" + std::to_string(num_inputs) + "_lev_" + std::to_string(logic_depth) + ".txt";


          std::string tt = kitty::to_binary(output_tt[output]);
          char* tt_binary = (char*)malloc(sizeof(char) * (tt.length() + 1));
          strcpy(tt_binary, tt.c_str());

          std::vector<std::string> onset_indeces;

          int indx = 0;
          for(int k = tt.length() - 1; k >= 0; k--){
            int bit = (int)tt_binary[k] - 48;
            if(bit == 1){
              onset_indeces.push_back(oracle::to_binary(indx));
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
            char **k_map = (char**)malloc(sizeof(char *) * col_num);
            for(int y = 0; y < col_num; y++)
              k_map[y] = (char*)malloc(sizeof(char) * row_num);

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
              char **k_map_pad = (char**)malloc(sizeof(char *) * padded_col);
              for(int k = 0; k < padded_col; k++){
                k_map_pad[k] = (char*)malloc(sizeof(char) * padded_row);
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

    void connect_outputs(Ntk ntk){
      // std::cout << "Number of output substitutions = " << output_substitutions.size() << "\n";
      for(auto it = output_substitutions.begin(); it != output_substitutions.end(); ++it){
        // std::cout << "substituting " << it->first << " with " << it->second.index << "\n";
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

    std::vector<std::set<node>> combine_partitions(Ntk& ntk, int part_1, int part_2){
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

      for(it = partitionInputs[part_2].begin(); it != partitionInputs[part_2].end(); ++it){

        for(int i = 0; i < input_partition[*it].size(); i++){
          if(input_partition[*it].at(i) == part_2){
            input_partition[*it].at(i) = part_1;
          }
        }
      }

      for(it = partitionOutputs[part_2].begin(); it != partitionOutputs[part_2].end(); ++it){
        if(_part_nodes[*it] == part_2)
          _part_nodes[*it] = part_1;
      }

      merged_inputs.erase(ntk.index_to_node(0));
      for(it = shared_history.begin(); it != shared_history.end(); ++it){
        node shared_node = *it;
        if(!ntk.is_pi(shared_node)){
          merged_inputs.erase(shared_node);
        }
        if(!ntk.is_po(shared_node)){
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

    std::set<int> get_connected_parts( Ntk& ntk, int partition_num ){
      std::set<int> conn_parts;
      typename std::set<node>::iterator it;
      for(it = partitionInputs[partition_num].begin(); it != partitionInputs[partition_num].end(); ++it){
        for(int i = 0; i < output_partition[*it].size(); i++){
          if(output_partition[*it].at(i) != partition_num && !ntk.is_pi(*it)){
            conn_parts.insert(output_partition[*it].at(i));
          }
        }
      }
      for(it = partitionOutputs[partition_num].begin(); it != partitionOutputs[partition_num].end(); ++it){
        for(int i = 0; i < input_partition[*it].size(); i++){
          if(input_partition[*it].at(i) != partition_num && !ntk.is_pi(*it)){
            conn_parts.insert(input_partition[*it].at(i));
          }
        }
      }
      return conn_parts;
    }

    std::vector<int> get_input_part(node curr_node){
      return input_partition[curr_node];
    }
    std::vector<int> get_output_part(node curr_node){
      return output_partition[curr_node];
    }

    std::vector<kahypar_partition_id_t> get_partitions() {
      return partition_results;
    }

    std::vector<kahypar_partition_id_t> get_initial_partitions() {
      return initial_partitions;
    }

    std::set<kahypar_partition_id_t> fixed_partitions() {
      std::set<kahypar_partition_id_t> ids;
      for (auto i = initial_partitions.begin(); i != initial_partitions.end(); i++) {
        if (*i >= 0) {
          ids.insert(*i);
        }
      }
      return ids;
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
    std::vector<kahypar_partition_id_t> initial_partitions;
    std::vector<kahypar_partition_id_t> partition_results;
  };
} /* namespace oracle */
