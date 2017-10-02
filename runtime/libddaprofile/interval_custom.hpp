#include <iostream>
#include <boost/icl/interval_set.hpp>

using namespace std;
using namespace boost::icl;

// Here is a typical class that may model intervals in your application.
class MyInterval
{
public:
    MyInterval(): _first(), _past(){}
    MyInterval(long lo, long up): _first(lo), _past(up){}
    long first()const{ return _first; }
    long past ()const{ return _past; }
private:
    long _first, _past;
};

namespace boost{ namespace icl 
{
// Class template interval_traits serves as adapter to register and customize your interval class
template<>
struct interval_traits< MyInterval >       //1.  Partially specialize interval_traits for 
{                                          //    your class MyInterval
                                           //2.  Define associated types
    typedef MyInterval     interval_type;  //2.1 MyInterval will be the interval_type
    typedef long           domain_type;    //2.2 The elements of the domain are ints 
    typedef std::less<long> domain_compare; //2.3 This is the way our element shall be ordered.
                                           //3.  Next we define the essential functions 
                                           //    of the specialisation
                                           //3.1 Construction of intervals
    static interval_type construct(const domain_type& lo, const domain_type& up) 
    { return interval_type(lo, up); }        
                                           //3.2 Selection of values 
    static domain_type lower(const interval_type& inter_val){ return inter_val.first(); };
    static domain_type upper(const interval_type& inter_val){ return inter_val.past(); };
};

template<>
struct interval_bound_type<MyInterval>     //4.  Finally we define the interval borders.
{                                          //    Choose between static_open         (lo..up)
    typedef interval_bound_type type;      //                   static_left_open    (lo..up]
    BOOST_STATIC_CONSTANT(bound_type, value = interval_bounds::static_right_open);//[lo..up)
};                                         //               and static_closed       [lo..up] 

}} // namespace boost icl


