#include <kitty/kitty.hpp>
#include <mockturtle/mockturtle.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <fmt/format.h>
#include <stdio.h>
#include <stdlib.h>

namespace mockturtle{
    class mig_script2{
    public:
        mockturtle::mig_network run(mockturtle::mig_network& mig){
            
            mockturtle::mig_npn_resynthesis resyn;
            mockturtle::akers_resynthesis<mockturtle::mig_network> rf_resyn;
            mockturtle::cut_rewriting_params ps;
            mockturtle::refactoring_params rp;

            ps.cut_enumeration_ps.cut_size = 4;
            rp.allow_zero_gain = false;

            mockturtle::depth_view mig_depth{mig};
            
            //b
            mockturtle::balancing(mig_depth);
            mig = mockturtle::cleanup_dangling(mig);

            //rw
            mockturtle::cut_rewriting(mig, resyn, ps);
            // std::cout << "done cut rewriting\n";
            mig = mockturtle::cleanup_dangling(mig);
            // std::cout << "2\n";

            //rf
            mockturtle::refactoring(mig, rf_resyn, rp);
            mig = mockturtle::cleanup_dangling(mig);

            mockturtle::depth_view mig_depth1{mig};

            //b
            mockturtle::balancing(mig_depth1);
            mig = mockturtle::cleanup_dangling(mig);

            //rw
            mockturtle::cut_rewriting(mig, resyn, ps);
            mig = mockturtle::cleanup_dangling(mig);
            // std::cout << "5\n";

            ps.allow_zero_gain = true;

            //rwz
            mockturtle::cut_rewriting(mig, resyn, ps);
            mig = mockturtle::cleanup_dangling(mig);
            // std::cout << "6\n";

            mockturtle::depth_view mig_depth2{mig};
            
            //b
            mockturtle::balancing(mig_depth2);
            mig = mockturtle::cleanup_dangling(mig);

            //rfz
            rp.allow_zero_gain = true;
            // std::cout << "activate zero gain\n";
            mockturtle::refactoring(mig, rf_resyn, rp);
            // std::cout << "refactored\n";
            mig = mockturtle::cleanup_dangling(mig);
            // std::cout << "8\n";

            //rwz
            mockturtle::cut_rewriting(mig, resyn, ps);
            mig = mockturtle::cleanup_dangling(mig);
            // std::cout << "9\n";

            mockturtle::depth_view mig_depth3{mig};
            
            //b
            mockturtle::balancing(mig_depth3);
            mig = mockturtle::cleanup_dangling(mig);

            return mig;
        }
    };
}