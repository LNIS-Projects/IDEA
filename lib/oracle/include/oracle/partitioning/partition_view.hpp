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
#include <unordered_set>
#include <cassert>

#include <mockturtle/traits.hpp>
#include <mockturtle/networks/detail/foreach.hpp>
#include <mockturtle/views/fanout_view.hpp>

namespace oracle
{

/*! \brief Implements an isolated view on a window in a network.
 *
 */

  template<typename Ntk>
  class partition_view : public Ntk {

    public:
      using storage = typename Ntk::storage;
      using node = typename Ntk::node;
      using signal = typename Ntk::signal;

    public:

      // partition_view(){
      //   add_node(get_node(get_constant(false)));
      // }

      explicit partition_view( Ntk const& ntk, std::set<node> const& leaves, std::set<node> const& pivots, bool auto_extend = true )
              : Ntk( ntk )
      {
        static_assert( mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type" );
        static_assert( mockturtle::has_set_visited_v<Ntk>, "Ntk does not implement the set_visited method" );
        static_assert( mockturtle::has_visited_v<Ntk>, "Ntk does not implement the visited method" );
        static_assert( mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
        static_assert( mockturtle::has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
        static_assert( mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
        static_assert( mockturtle::has_make_signal_v<Ntk>, "Ntk does not implement the make_signal method" );

        this->clear_visited();
        /* constants */
        add_node( this->get_node( this->get_constant( false ) ) );
        this->set_visited( this->get_node( this->get_constant( false ) ), 1 );
        if ( this->get_node( this->get_constant( true ) ) != this->get_node( this->get_constant( false ) ) ) {

          add_node( this->get_node( this->get_constant( true ) ) );
          this->set_visited( this->get_node( this->get_constant( true ) ), 1 );
          ++_num_constants;
        }

        /* primary inputs */
        for ( auto const& leaf : leaves ) {

          if ( this->visited( leaf ) == 1 )
            continue;

          add_node( leaf );
          this->set_visited( leaf, 1 );
          ++_num_leaves;
        }

        for ( auto const& p : pivots ) {
          // std::cout << "pivot = " << p << "\n";
          traverse( p );
        }

        for (auto n : pivots){

          auto sig = this->make_signal(n);
          if(ntk.is_complemented(sig)) {
            sig = this->create_not(sig);
          }
          _roots.push_back(sig);
        }

        /* restore visited */
        for ( auto const& n : _nodes ) {
          this->set_visited( n, 0 );
        }
      }

      inline auto size() const { return static_cast<uint32_t>( _nodes.size() ); }
      inline auto num_pis() const { return _num_leaves; }
      inline auto num_pos() const { return _roots.size(); }
      
      inline auto num_gates() const { 
        return _nodes.size() - _num_leaves - _num_constants;
      }

      inline auto node_to_index( const node& n ) const { return _node_to_index.at( n ); }
      inline auto index_to_node( uint32_t index ) const { return _nodes[index]; }

      inline bool is_pi( node const& pi ) const 
      {
        std::vector<signal> children;
        this->foreach_fanin( pi, [&]( const auto& f ) {
          children.push_back(f);
        });
        const auto beg = _nodes.begin() + _num_constants;
        // std::cout << "is_pi children size = " << children.size() << "\n";
        return std::find( beg, beg + _num_leaves, pi ) != beg + _num_leaves;// || children.size() == 0;
      }

      inline bool is_ci( node const& pi ) const 
      {
        std::vector<signal> children;
        this->foreach_fanin( pi, [&]( const auto& f ) {
          children.push_back(f);
        });
        const auto beg = _nodes.begin() + _num_constants;
        return std::find( beg, beg + _num_leaves, pi ) != beg + _num_leaves;// || children.size() == 0;
      }


      template<typename Fn>
      void foreach_pi( Fn&& fn ) const
      {
          mockturtle::detail::foreach_element( _nodes.begin() + _num_constants, _nodes.begin() + _num_constants + _num_leaves, fn );
      }

      template<typename Fn>
      void foreach_po( Fn&& fn ) const
      {
          mockturtle::detail::foreach_element( _roots.begin(), _roots.end(), fn );
      }

      template<typename Fn>
      void foreach_node( Fn&& fn ) const
      {
          mockturtle::detail::foreach_element( _nodes.begin(), _nodes.end(), fn );
      }

      template<typename Fn>
      void foreach_gate( Fn&& fn ) const
      {
          mockturtle::detail::foreach_element( _nodes.begin() + _num_constants + _num_leaves, _nodes.end(), fn );
      }

      uint32_t fanout_size( node const& n ) const
      {
          return _fanout_size.at( node_to_index(n) );
      }

      std::vector<signal> get_children( int nodeIdx ) const
      {
          return _children[nodeIdx];
      }

      std::vector<node> get_node_list(){
        return _nodes;
      }


  private:

    void add_node( node const& n )
    {
      // std::cout << "adding node " << n << "\n";
      _node_to_index[n] = _nodes.size();
      _nodes.push_back( n );
      _nodes_lut.insert(n);

      auto fanout_counter = 0;
      this->foreach_fanin( n, [&]( const auto& f ) {
        if ( _nodes_lut.find(this->get_node( f ) ) != _nodes_lut.end() ) {

          fanout_counter++;
        }
      });
      _fanout_size.push_back( fanout_counter );
    }

      void traverse( node const& n ) {
        // std::cout << "Node = " << n << "\n";
        if ( this->visited( n ) == 1 )
          return;

        this->foreach_fanin( n, [&]( const auto& f ) {
          // std::cout << "fanin of " << n << " = " << f.index << "\n";
          traverse( this->get_node( f ) );
        } );

        add_node( n );
        this->set_visited( n, 1 );
      }

      void update_fanin( Ntk const& ntk ){

        foreach_gate ([&]( auto node ){
          std::vector<signal> children;
          int nodeIdx = node_to_index(node);
          for(auto const& child : ntk._storage->nodes[node].children){
            auto orig_node = ntk.index_to_node(child.index);
            auto part_node = node_to_index(orig_node);
            auto sig = ntk.make_signal(part_node);
            if(child.data & 1){
              sig = ntk.create_not(sig);
            }
            children.push_back(sig);
          }
          auto s = signal(node, 0);
          _children[nodeIdx] = children;
        });
      }
    public:
      unsigned _num_constants{1};
      unsigned _num_leaves{0};
      std::vector<node> _nodes;
      std::unordered_map<int, uint32_t> _visited;
      std::unordered_map<int, std::vector<signal>> _children;
      std::unordered_map<node, uint32_t> _node_to_index;
      std::vector<signal> _outputs;
      std::vector<signal> _roots;
      std::vector<unsigned> _fanout_size;

      std::unordered_set<node>   _nodes_lut;
      std::unordered_set<signal> _roots_lut;
    };

  } /* namespace oracle */