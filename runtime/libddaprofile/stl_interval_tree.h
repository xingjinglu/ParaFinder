// Interval RB tree implementation -*- C++ -*-

//  Added by lxj.

// Copyright (C) 2001-2014 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/*
 *
 * Copyright (c) 1996,1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 */

/** @file bits/stl_interval_tree.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{map,set}
 */

#ifndef _STL_INTERVAL_TREE_H
#define _STL_INTERVAL_TREE_H 1

//#pragma GCC system_header
#if 1
#include <bits/stl_algobase.h>
#include <bits/allocator.h>
#include <bits/stl_function.h>
#include <bits/cpp_type_traits.h>
#include <ext/alloc_traits.h>
#include<set>
#include<stack>
#endif
#if 0
#include <stl_algobase.h>
#include <allocator.h>
#include <stl_function.h>
#include <cpp_type_traits.h>
#include <alloc_traits.h>
#include<iostream>
#endif

#define GCC_VERSION (__GNUC__ * 10000 \
    + __GNUC_MINOR__ * 100 \
    + __GNUC_PATCHLEVEL__)

//#if GCC_VERSION >= 40900
//#include<ext/aligned_buffer.h>
#if __cplusplus > 201103L
#include <aligned_buffer.h>
#endif

#define _GLIBCXX_FORWARD_OLD(_Tp, __val) (__val)  

//# define _GLIBCXX_VISIBILITY(V) __attribute__ ((__visibility__ (#V)))  

namespace std  _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION


// Red-black tree class, designed for use in implementing STL
// associative containers (set, multiset, map, and multimap). The
// insertion and deletion algorithms are based on those in Cormen,
// Leiserson, and Rivest, Introduction to Algorithms (MIT Press,
// 1990), except that
//
// (1) the header cell is maintained with links not only to the root
// but also to the leftmost node of the tree, to enable constant
// time begin(), and to the rightmost node of the tree, to enable
// linear time performance when used with the generic set algorithms
// (set_union, etc.)
// 
// (2) when a node being deleted has two children its successor node
// is relinked into its place, rather than copied, so that the only
// iterators invalidated are those referring to the deleted node.

// defined in namespace of  std in stl_tree.h
//
enum _Interval_rb_tree_color { _S_interval_red = false, _S_interval_black = true };

struct _Interval_rb_tree_node_base
{
  typedef _Interval_rb_tree_node_base* _Base_ptr;
  typedef const _Interval_rb_tree_node_base* _Const_Base_ptr;

  _Interval_rb_tree_color	_M_color;
  _Base_ptr		_M_parent;
  _Base_ptr		_M_left;
  _Base_ptr		_M_right;

  static _Base_ptr
    _S_minimum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_left != 0) __x = __x->_M_left;
      return __x;
    }

  static _Const_Base_ptr
    _S_minimum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_left != 0) __x = __x->_M_left;
      return __x;
    }

  static _Base_ptr
    _S_maximum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_right != 0) __x = __x->_M_right;
      return __x;
    }

  static _Const_Base_ptr
    _S_maximum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_right != 0) __x = __x->_M_right;
      return __x;
    }
};

template<typename _Val>
struct _Interval_rb_tree_node : public _Interval_rb_tree_node_base
{
  typedef _Interval_rb_tree_node<_Val>* _Link_type;

#if __cplusplus <= 201103L
  _Val _M_value_field;
  _Val _M_low, _M_up; // Interval: [Beg, End]
  _Val _M_max; 
  int _M_num; // Elements number (equal: (1, 2, 3, 4, 2, 2, 3, 4, 4, 4) ==> [1, 4] = 10).

  _Val* _M_valptr()
  { return std::__addressof(_M_value_field); }

  const _Val* _M_valptr() const
  { return std::__addressof(_M_value_field); }

  _Val * _M_maxptr() 
  { return std::__addressof(_M_max); }

  const _Val * _M_maxptr() const
  { return std::__addressof(_M_max); }

  _Val * _M_lowptr() 
  { return std::__addressof(_M_low); }

  const _Val * _M_lowptr() const
  { return std::__addressof(_M_low); }

  _Val * _M_upptr() 
  { return std::__addressof(_M_up); }

  const _Val * _M_upptr() const
  { return std::__addressof(_M_up); }

#else
  __gnu_cxx::__aligned_buffer<_Val> _M_storage;

  _Val*
    _M_valptr()
    { return _M_storage._M_ptr(); }

  const _Val*
    _M_valptr() const
    { return _M_storage._M_ptr(); }
#endif
};

_GLIBCXX_PURE _Interval_rb_tree_node_base*
_Interval_rb_tree_increment(_Interval_rb_tree_node_base* __x) throw ();

_GLIBCXX_PURE const _Interval_rb_tree_node_base*
_Interval_rb_tree_increment(const _Interval_rb_tree_node_base* __x) throw ();

_GLIBCXX_PURE _Interval_rb_tree_node_base*
_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ();

_GLIBCXX_PURE const _Interval_rb_tree_node_base*
_Interval_rb_tree_decrement(const _Interval_rb_tree_node_base* __x) throw ();

template<typename _Tp>
struct _Interval_rb_tree_iterator
{
  typedef _Tp  value_type;
  typedef _Tp& reference;
  typedef _Tp* pointer;

  typedef bidirectional_iterator_tag iterator_category;
  typedef ptrdiff_t                  difference_type;

  typedef _Interval_rb_tree_iterator<_Tp>        _Self;
  typedef _Interval_rb_tree_node_base::_Base_ptr _Base_ptr;
  typedef _Interval_rb_tree_node<_Tp>*           _Link_type;

  _Interval_rb_tree_iterator() _GLIBCXX_NOEXCEPT
    : _M_node() { }

  explicit
    _Interval_rb_tree_iterator(_Link_type __x) _GLIBCXX_NOEXCEPT
    : _M_node(__x) { }

  reference
    operator*() const _GLIBCXX_NOEXCEPT
    { return *static_cast<_Link_type>(_M_node)->_M_valptr(); }

  pointer
    operator->() const _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type> (_M_node)->_M_valptr(); }

  _Self&
    operator++() _GLIBCXX_NOEXCEPT
    {
      _M_node = _Interval_rb_tree_increment(_M_node);
      return *this;
    }

  _Self
    operator++(int) _GLIBCXX_NOEXCEPT
    {
      _Self __tmp = *this;
      _M_node = _Interval_rb_tree_increment(_M_node);
      return __tmp;
    }

  _Self&
    operator--() _GLIBCXX_NOEXCEPT
    {
      _M_node = _Interval_rb_tree_decrement(_M_node);
      return *this;
    }

  _Self
    operator--(int) _GLIBCXX_NOEXCEPT
    {
      _Self __tmp = *this;
      _M_node = _Interval_rb_tree_decrement(_M_node);
      return __tmp;
    }

  bool
    operator==(const _Self& __x) const _GLIBCXX_NOEXCEPT
    { return _M_node == __x._M_node; }

  bool
    operator!=(const _Self& __x) const _GLIBCXX_NOEXCEPT
    { return _M_node != __x._M_node; }

  std::pair<value_type, value_type>
    interval(){
      value_type low = *(static_cast<_Link_type> (_M_node)->_M_lowptr());
      value_type up = *(static_cast<_Link_type> (_M_node)->_M_upptr());
    //std::cout<<"return low = "<<low <<" up = "<<up<<std::endl;
      return std::pair<value_type, value_type>(low, up);
    } 
  int countele(){
    return static_cast<_Link_type>(_M_node)->_M_num;
  }

  _Base_ptr _M_node;
};

template<typename _Tp>
struct _Interval_rb_tree_const_iterator
{
  typedef _Tp        value_type;
  typedef const _Tp& reference;
  typedef const _Tp* pointer;

  typedef _Interval_rb_tree_iterator<_Tp> iterator;

  typedef bidirectional_iterator_tag iterator_category;
  typedef ptrdiff_t                  difference_type;

  typedef _Interval_rb_tree_const_iterator<_Tp>        _Self;
  typedef _Interval_rb_tree_node_base::_Const_Base_ptr _Base_ptr;
  typedef const _Interval_rb_tree_node<_Tp>*           _Link_type;

  _Interval_rb_tree_const_iterator() _GLIBCXX_NOEXCEPT
    : _M_node() { }

  explicit
    _Interval_rb_tree_const_iterator(_Link_type __x) _GLIBCXX_NOEXCEPT
    : _M_node(__x) { }

  _Interval_rb_tree_const_iterator(const iterator& __it) _GLIBCXX_NOEXCEPT
    : _M_node(__it._M_node) { }

  iterator
    _M_const_cast() const _GLIBCXX_NOEXCEPT
    { return iterator(static_cast<typename iterator::_Link_type>
        (const_cast<typename iterator::_Base_ptr>(_M_node))); }

  reference
    operator*() const _GLIBCXX_NOEXCEPT
    { return *static_cast<_Link_type>(_M_node)->_M_valptr(); }

  pointer
    operator->() const _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type>(_M_node)->_M_valptr(); }

  _Self&
    operator++() _GLIBCXX_NOEXCEPT
    {
      _M_node = _Interval_rb_tree_increment(_M_node);
      return *this;
    }

  _Self
    operator++(int) _GLIBCXX_NOEXCEPT
    {
      _Self __tmp = *this;
      _M_node = _Interval_rb_tree_increment(_M_node);
      return __tmp;
    }

  _Self&
    operator--() _GLIBCXX_NOEXCEPT
    {
      _M_node = _Interval_rb_tree_decrement(_M_node);
      return *this;
    }

  _Self
    operator--(int) _GLIBCXX_NOEXCEPT
    {
      _Self __tmp = *this;
      _M_node = _Interval_rb_tree_decrement(_M_node);
      return __tmp;
    }

  bool
    operator==(const _Self& __x) const _GLIBCXX_NOEXCEPT
    { return _M_node == __x._M_node; }

  bool
    operator!=(const _Self& __x) const _GLIBCXX_NOEXCEPT
    { return _M_node != __x._M_node; }


  std::pair<value_type, value_type>
    interval() const
    {
      value_type low = *(static_cast<_Link_type> (_M_node)->_M_lowptr());
      value_type up = *(static_cast<_Link_type> (_M_node)->_M_upptr());

    //std::cout<<"return const low = "<<low <<" up = "<<up<<std::endl;
      return std::pair<value_type, value_type>(low, up);
    } 

  int countele()const
  {
    return static_cast<_Link_type>(_M_node)->_M_num;
  }
  _Base_ptr _M_node;
};

template<typename _Val>
inline bool
operator==(const _Interval_rb_tree_iterator<_Val>& __x,
    const _Interval_rb_tree_const_iterator<_Val>& __y) _GLIBCXX_NOEXCEPT
{ return __x._M_node == __y._M_node; }

template<typename _Val>
inline bool
operator!=(const _Interval_rb_tree_iterator<_Val>& __x,
    const _Interval_rb_tree_const_iterator<_Val>& __y) _GLIBCXX_NOEXCEPT
{ return __x._M_node != __y._M_node; }

void
_Interval_rb_tree_insert_and_rebalance(const bool __insert_left,
    _Interval_rb_tree_node_base* __x,
    _Interval_rb_tree_node_base* __p,
    _Interval_rb_tree_node_base& __header) throw ();

_Interval_rb_tree_node_base*
_Interval_rb_tree_rebalance_for_erase(_Interval_rb_tree_node_base* const __z,
    _Interval_rb_tree_node_base& __header) throw ();


template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc = allocator<_Val> >
  class _Interval_rb_tree
{
  typedef typename __gnu_cxx::__alloc_traits<_Alloc>::template
    rebind<_Interval_rb_tree_node<_Val> >::other _Node_allocator;

  typedef __gnu_cxx::__alloc_traits<_Node_allocator> _Alloc_traits;

  // 20140620
  _Key _stride = 8;  //int
  _Key _border_stride = 16;

  protected:
  typedef _Interval_rb_tree_node_base* 		_Base_ptr;
  typedef const _Interval_rb_tree_node_base* 	_Const_Base_ptr;

  public:
  typedef _Key 				key_type;
  typedef _Val 				value_type;
  typedef value_type* 			pointer;
  typedef const value_type* 		const_pointer;
  typedef value_type& 			reference;
  typedef const value_type& 		const_reference;
  typedef _Interval_rb_tree_node<_Val>* 		_Link_type;
  typedef const _Interval_rb_tree_node<_Val>*	_Const_Link_type;
  typedef size_t 				size_type;
  typedef ptrdiff_t 			difference_type;
  typedef _Alloc 				allocator_type;

  // 20140813
  std::stack<_Link_type> _to_erase;

  _Node_allocator&
    _M_get_Node_allocator() _GLIBCXX_NOEXCEPT
    { return *static_cast<_Node_allocator*>(&this->_M_impl); }

  const _Node_allocator&
    _M_get_Node_allocator() const _GLIBCXX_NOEXCEPT
    { return *static_cast<const _Node_allocator*>(&this->_M_impl); }

  allocator_type
    get_allocator() const _GLIBCXX_NOEXCEPT
    { return allocator_type(_M_get_Node_allocator()); }

  protected:
  _Link_type
    _M_get_node()
    { return _Alloc_traits::allocate(_M_get_Node_allocator(), 1); }

  void
    _M_put_node(_Link_type __p) _GLIBCXX_NOEXCEPT
    { _Alloc_traits::deallocate(_M_get_Node_allocator(), __p, 1); }

#if __cplusplus <= 201103L
  _Link_type
    _M_create_node(const value_type& __x)
    {
      _Link_type __tmp = _M_get_node();
      __try
      { get_allocator().construct(__tmp->_M_valptr(), __x); }
      __catch(...)
      {
        _M_put_node(__tmp);
        __throw_exception_again;
      }
      return __tmp;
    }

  void
    _M_destroy_node(_Link_type __p)
    {
      get_allocator().destroy(__p->_M_valptr());
      _M_put_node(__p);
    }
#else
  template<typename... _Args>
    _Link_type
    _M_create_node(_Args&&... __args)
    {
      _Link_type __tmp = _M_get_node();
      __try
      {
        ::new(__tmp) _Interval_rb_tree_node<_Val>;
        _Alloc_traits::construct(_M_get_Node_allocator(),
            __tmp->_M_valptr(),
            std::forward<_Args>(__args)...);
      }
      __catch(...)
      {
        _M_put_node(__tmp);
        __throw_exception_again;
      }
      return __tmp;
    }

  void
    _M_destroy_node(_Link_type __p) noexcept
    {
      _Alloc_traits::destroy(_M_get_Node_allocator(), __p->_M_valptr());
      __p->~_Interval_rb_tree_node<_Val>();
      _M_put_node(__p);
    }
#endif

  // clone interval_node.
  _Link_type
    _M_clone_node(_Const_Link_type __x)
    {
      _Link_type __tmp = _M_create_node(*__x->_M_valptr());
      __tmp->_M_low = __x->_M_low;
      __tmp->_M_up = __x->_M_up;
      __tmp->_M_max = __x->_M_max;

      __tmp->_M_color = __x->_M_color;
      __tmp->_M_left = 0;
      __tmp->_M_right = 0;
      return __tmp;
    }

  protected:
  template<typename _Key_compare, 
    bool _Is_pod_comparator = __is_pod(_Key_compare)>
      struct _Interval_rb_tree_impl : public _Node_allocator
    {
      _Key_compare		_M_key_compare;
      _Interval_rb_tree_node_base 	_M_header;
      size_type 		_M_node_count; // Keeps track of size of tree.

      _Interval_rb_tree_impl()
        : _Node_allocator(), _M_key_compare(), _M_header(),
        _M_node_count(0)
      { _M_initialize(); }

      _Interval_rb_tree_impl(const _Key_compare& __comp, const _Node_allocator& __a)
        : _Node_allocator(__a), _M_key_compare(__comp), _M_header(),
        _M_node_count(0)
      { _M_initialize(); }

#if __cplusplus > 201103L
      _Interval_rb_tree_impl(const _Key_compare& __comp, _Node_allocator&& __a)
        : _Node_allocator(std::move(__a)), _M_key_compare(__comp),
        _M_header(), _M_node_count(0)
      { _M_initialize(); }
#endif

      private:
      void
        _M_initialize()
        {
          this->_M_header._M_color = _S_interval_red;
          this->_M_header._M_parent = 0;
          this->_M_header._M_left = &this->_M_header;
          this->_M_header._M_right = &this->_M_header;
        }	    
    };

  _Interval_rb_tree_impl<_Compare> _M_impl;

  protected:
  _Base_ptr&
    _M_root() _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_parent; }

  _Const_Base_ptr
    _M_root() const _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_parent; }

  _Base_ptr&
    _M_leftmost() _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_left; }

  _Const_Base_ptr
    _M_leftmost() const _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_left; }

  _Base_ptr&
    _M_rightmost() _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_right; }

  _Const_Base_ptr
    _M_rightmost() const _GLIBCXX_NOEXCEPT
    { return this->_M_impl._M_header._M_right; }

  _Link_type
    _M_begin() _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type>(this->_M_impl._M_header._M_parent); }

  _Const_Link_type
    _M_begin() const _GLIBCXX_NOEXCEPT
    {
      return static_cast<_Const_Link_type>
        (this->_M_impl._M_header._M_parent);
    }

  _Link_type
    _M_end() _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type>(&this->_M_impl._M_header); }

  _Const_Link_type
    _M_end() const _GLIBCXX_NOEXCEPT
    { return static_cast<_Const_Link_type>(&this->_M_impl._M_header); }

  static const_reference
    _S_value(_Const_Link_type __x)
    { return *__x->_M_valptr(); }

  static const _Key&
    _S_key(_Const_Link_type __x)
    { return _KeyOfValue()(_S_low(__x)); }
    //{ return _KeyOfValue()(_S_value(__x)); }


  static const _Key& _S_low(_Const_Link_type __x)
  { return _KeyOfValue()( *__x->_M_lowptr() ); } // why not? __x->_M_low;

  static const _Key& _S_up(_Const_Link_type __x)
  { return _KeyOfValue()( *__x->_M_upptr() ); } 

  static const _Key& _S_max(_Const_Link_type __x)
  { return _KeyOfValue()( *__x->_M_maxptr() ); } 



  static _Link_type
    _S_left(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type>(__x->_M_left); }

  static _Const_Link_type
    _S_left(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return static_cast<_Const_Link_type>(__x->_M_left); }

  static _Link_type
    _S_right(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return static_cast<_Link_type>(__x->_M_right); }

  static _Const_Link_type
    _S_right(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return static_cast<_Const_Link_type>(__x->_M_right); }

  static const_reference
    _S_value(_Const_Base_ptr __x)
    { return *static_cast<_Const_Link_type>(__x)->_M_valptr(); }

  static const _Key&
    _S_key(_Const_Base_ptr __x)
    { return _KeyOfValue()(_S_value(__x)); }

  static _Base_ptr
    _S_minimum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return _Interval_rb_tree_node_base::_S_minimum(__x); }

  static _Const_Base_ptr
    _S_minimum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return _Interval_rb_tree_node_base::_S_minimum(__x); }

  static _Base_ptr
    _S_maximum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return _Interval_rb_tree_node_base::_S_maximum(__x); }

  static _Const_Base_ptr
    _S_maximum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    { return _Interval_rb_tree_node_base::_S_maximum(__x); }

  public:
  typedef _Interval_rb_tree_iterator<value_type>       iterator;
  typedef _Interval_rb_tree_const_iterator<value_type> const_iterator;

  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  private:
  pair<_Base_ptr, _Base_ptr>
    _M_get_insert_unique_pos(const key_type& __k);

  pair<_Base_ptr, _Base_ptr>
    _M_get_insert_interval_unique_pos(const key_type& __k, const key_type& __end);
  
  void _M_erase_interval_node(const_iterator __position);
  //void _M_join_adjacent_nodes( _Link_type& pos, bool joinlow, bool joinup);

 _Link_type 
    _M_join_right_nodes(_Link_type left, _Link_type right)
    {
      _Link_type __parent;
      key_type __low1, __low2, __up1, __up2;
      __low1 = left->_M_low;
      __up1 = left->_M_up;

      while( right != 0 ){
        __low2 = right->_M_low;
        __up2 = right->_M_up;
        if( (__low1 < __up2 + _border_stride) && (__low2 < __up1 + _border_stride)  ){
          if(  __low1 < __low2 ){
            right->_M_low = __low1;
          }
          if( __up2 < __up1  ){
            right->_M_up = __up1;
          } 
          //right->_M_num += left->_M_num;

          // update _M_max value lazily. 
          #if 1
          __parent = right;
          while( __parent  ){
            if ( __parent->_M_max < __up1 ){
              __parent->_M_max = __up1;
              __parent = static_cast<_Link_type>(__parent->_M_parent);
            }
            else
              break;
          }
         #endif
          //_M_erase_interval_node(const_iterator(static_cast<_Const_Link_type>(left)));
          _to_erase.push(left);

          right = _M_join_adjacent_nodes(right, 0, 0); 
          return right;
        }
       
        // Left-first search.
        if( _S_left( right ) &&  ( __low1 < (_S_left(right))->_M_max + _border_stride ) ){
          right = _S_left( right);
        } 
        else{
          right = _S_right( right );
        }

      }

      return left;
  }

 _Link_type 
   _M_join_adjacent_nodes( _Link_type& pos, bool joinlow, bool joinup )
   {
     _Link_type __next; // 
     _Link_type __parent;
     key_type  __low1, __low2, __up1, __up2;

     __low1 = pos->_M_low;
     __up1 = pos->_M_up;
     //__next = _M_begin();
     std::stack<_Link_type> __RightNode; // For right tree.

     // Merge with parents. __low1 < parent->up; check parent->low < up1 ?
     // bug 20140812
     #if 0
     __parent = static_cast<_Link_type>(pos->_M_parent);
     while( __parent && (__parent->_M_low < ( __up1 + _border_stride) )){
      #if 0
       if(  __parent->_M_low < __low1 ){
          __low1 = __parent->_M_low;
       }
      #endif
       if( __up1 < __parent->_M_up  ){
         __up1 = __parent->_M_up;
       }
       _M_erase_interval_node(const_iterator(static_cast<_Const_Link_type>(__parent)));
       __parent = static_cast<_Link_type>(pos->_M_parent);
     }

     pos->_M_low = __low1;
     pos->_M_up = __up1;
    #endif

#if 1
     //if( _S_left( pos ) &&  _M_impl._M_key_compare( __low1, (_S_left(pos))->_M_max + _border_stride) )
     if( _S_left( pos ) &&  ( __low1 < (_S_left(pos))->_M_max + _border_stride) ){
       __next = _S_left(pos);
       if( _S_right( pos ) &&  ( __low1 < (_S_right(pos))->_M_max + _border_stride) )
         __RightNode.push(_S_right(pos)); 
     }
     else
       __next = _S_right(pos);
#endif

     while( __next != 0  ){
       __low2 = __next->_M_low;
       __up2 = __next->_M_up;

       // Update _M_max of interval-search path[pos, __next] if there are overlapping.  
#if 1
       //if(  _M_impl._M_key_compare( __next->_M_max, __up1)  )
       //  __next->_M_max = __up1;
#endif

       // overlap. opt?
       if( (__low1 < __up2 + _border_stride) && (__low2 < __up1 + _border_stride)  ){
         if(  __low1 < __low2 ){
           __next->_M_low = __low1;
         }
         if( __up2 < __up1  ){
           __next->_M_up = __up1;
         } 
         //__next->_M_num += pos->_M_num;

         // update _M_max value lazily. 
         __parent = __next;
         while( __parent  ){
           if ( __parent->_M_max < __up1 ){
             __parent->_M_max = __up1;
             __parent = static_cast<_Link_type>(__parent->_M_parent);
           }
           else
             break;
         }

         //std::cout<<"to del: __y_low = "<< pos->_M_low <<", __y_up = "<< pos->_M_up << std::endl;
         //_M_erase_interval_node(const_iterator(static_cast<_Const_Link_type> (pos)));
         _to_erase.push(pos);
         //std::cout<<"new: __next_low = "<< __next->_M_low <<", __next_up = "<< __next->_M_up << std::endl;

         // Recursive.
         pos = __next;
       __low1 = pos->_M_low;
       __up1 = pos->_M_up;
       }


       // Check from subtree of pos.
       if( _S_left(__next) &&  ( __low1 < (_S_left(__next))->_M_max + _border_stride ) ){
         if( _S_right( __next ) &&  ( __low1 < (_S_right(__next))->_M_max + _border_stride) )
           __RightNode.push(_S_right(__next)); 
         __next = _S_left(__next);
       } 
       else{
         __next = _S_right(__next);
       }

     } // end while(

     // 20140812 
     _Link_type __right, __left = pos; 
     #if 1
     while( !__RightNode.empty() ){
       __right = __RightNode.top(); 
       __RightNode.pop(); 
       __left =  _M_join_right_nodes(__left, __right);
     }
    #endif

     return __left; // New node.
   }

  pair<_Base_ptr, _Base_ptr>
    _M_get_insert_equal_pos(const key_type& __k);

  pair<_Base_ptr, _Base_ptr>
    _M_get_insert_hint_unique_pos(const_iterator __pos,
        const key_type& __k);

  pair<_Base_ptr, _Base_ptr>
    _M_get_insert_hint_equal_pos(const_iterator __pos,
        const key_type& __k);

#if __cplusplus > 201103L
  template<typename _Arg>
    iterator
    _M_insert_(_Base_ptr __x, _Base_ptr __y, _Arg&& __v);

  iterator
    _M_insert_node(_Base_ptr __x, _Base_ptr __y, _Link_type __z);

  template<typename _Arg>
    iterator
    _M_insert_lower(_Base_ptr __y, _Arg&& __v);

  template<typename _Arg>
    iterator
    _M_insert_equal_lower(_Arg&& __x);

  iterator
    _M_insert_lower_node(_Base_ptr __p, _Link_type __z);

  iterator
    _M_insert_equal_lower_node(_Link_type __z);
#else
  iterator
    _M_insert_(_Base_ptr __x, _Base_ptr __y,
        const value_type& __v);
  iterator
    _M_insert_interval_(_Base_ptr __x, _Base_ptr __y,
        const value_type& __v, const value_type& __up);

  // _GLIBCXX_RESOLVE_LIB_DEFECTS
  // 233. Insertion hints in associative containers.
  iterator
    _M_insert_lower(_Base_ptr __y, const value_type& __v);

  iterator
    _M_insert_equal_lower(const value_type& __x);
#endif

  _Link_type
    _M_copy(_Const_Link_type __x, _Link_type __p);

  void
    _M_erase(_Link_type __x);

  iterator
    _M_lower_bound(_Link_type __x, _Link_type __y,
        const _Key& __k);

  const_iterator
    _M_lower_bound(_Const_Link_type __x, _Const_Link_type __y,
        const _Key& __k) const;

  iterator
    _M_upper_bound(_Link_type __x, _Link_type __y,
        const _Key& __k);

  const_iterator
    _M_upper_bound(_Const_Link_type __x, _Const_Link_type __y,
        const _Key& __k) const;

  public:
  // allocation/deallocation
  _Interval_rb_tree() { }

  _Interval_rb_tree(_Key stride) { 
    _stride = stride; 
    _border_stride = stride*2; 
    //std::cout<<"_stride = " << _stride << std::endl;
  }

  _Interval_rb_tree(const _Compare& __comp,
      const allocator_type& __a = allocator_type())
    : _M_impl(__comp, _Node_allocator(__a)) { }

  _Interval_rb_tree(const _Interval_rb_tree& __x)
    : _M_impl(__x._M_impl._M_key_compare,
        _Alloc_traits::_S_select_on_copy(__x._M_get_Node_allocator()))
  {
    if (__x._M_root() != 0)
    {
      _M_root() = _M_copy(__x._M_begin(), _M_end());
      _M_leftmost() = _S_minimum(_M_root());
      _M_rightmost() = _S_maximum(_M_root());
      _M_impl._M_node_count = __x._M_impl._M_node_count;
    }
  }

#if __cplusplus > 201103L
  _Interval_rb_tree(const allocator_type& __a)
    : _M_impl(_Compare(), _Node_allocator(__a))
  { }

  _Interval_rb_tree(const _Interval_rb_tree& __x, const allocator_type& __a)
    : _M_impl(__x._M_impl._M_key_compare, _Node_allocator(__a))
  {
    if (__x._M_root() != 0)
    {
      _M_root() = _M_copy(__x._M_begin(), _M_end());
      _M_leftmost() = _S_minimum(_M_root());
      _M_rightmost() = _S_maximum(_M_root());
      _M_impl._M_node_count = __x._M_impl._M_node_count;
    }
  }

  _Interval_rb_tree(_Interval_rb_tree&& __x)
    : _M_impl(__x._M_impl._M_key_compare, __x._M_get_Node_allocator())
  {
    if (__x._M_root() != 0)
      _M_move_data(__x, std::true_type());
  }

  _Interval_rb_tree(_Interval_rb_tree&& __x, const allocator_type& __a)
    : _Interval_rb_tree(std::move(__x), _Node_allocator(__a))
  { }

  _Interval_rb_tree(_Interval_rb_tree&& __x, _Node_allocator&& __a);
#endif

  ~_Interval_rb_tree() _GLIBCXX_NOEXCEPT
  { _M_erase(_M_begin()); }

  _Interval_rb_tree&
    operator=(const _Interval_rb_tree& __x);

  // Accessors.
  _Compare
    key_comp() const
    { return _M_impl._M_key_compare; }

  iterator
    begin() _GLIBCXX_NOEXCEPT
    { 
      return iterator(static_cast<_Link_type>
          (this->_M_impl._M_header._M_left));
    }

  const_iterator
    begin() const _GLIBCXX_NOEXCEPT
    { 
      return const_iterator(static_cast<_Const_Link_type>
          (this->_M_impl._M_header._M_left));
    }

  iterator
    end() _GLIBCXX_NOEXCEPT
    { return iterator(static_cast<_Link_type>(&this->_M_impl._M_header)); }

  const_iterator
    end() const _GLIBCXX_NOEXCEPT
    { 
      return const_iterator(static_cast<_Const_Link_type>
          (&this->_M_impl._M_header));
    }

  reverse_iterator
    rbegin() _GLIBCXX_NOEXCEPT
    { return reverse_iterator(end()); }

  const_reverse_iterator
    rbegin() const _GLIBCXX_NOEXCEPT
    { return const_reverse_iterator(end()); }

  reverse_iterator
    rend() _GLIBCXX_NOEXCEPT
    { return reverse_iterator(begin()); }

  const_reverse_iterator
    rend() const _GLIBCXX_NOEXCEPT
    { return const_reverse_iterator(begin()); }

  bool
    empty() const _GLIBCXX_NOEXCEPT
    { return _M_impl._M_node_count == 0; }

  size_type
    size() const _GLIBCXX_NOEXCEPT 
    { return _M_impl._M_node_count; }

  size_type
    max_size() const _GLIBCXX_NOEXCEPT
    { return _Alloc_traits::max_size(_M_get_Node_allocator()); }

  void
#if __cplusplus > 201103L
    swap(_Interval_rb_tree& __t) noexcept(_Alloc_traits::_S_nothrow_swap());
#else
  swap(_Interval_rb_tree& __t);
#endif

  // Insert/erase.
#if __cplusplus > 201103L
  template<typename _Arg>
    pair<iterator, bool>
    _M_insert_unique(_Arg&& __x);

  template<typename _Arg>
    iterator
    _M_insert_equal(_Arg&& __x);

  template<typename _Arg>
    iterator
    _M_insert_unique_(const_iterator __position, _Arg&& __x);

  template<typename _Arg>
    iterator
    _M_insert_equal_(const_iterator __position, _Arg&& __x);

  template<typename... _Args>
    pair<iterator, bool>
    _M_emplace_unique(_Args&&... __args);

  template<typename... _Args>
    iterator
    _M_emplace_equal(_Args&&... __args);

  template<typename... _Args>
    iterator
    _M_emplace_hint_unique(const_iterator __pos, _Args&&... __args);

  template<typename... _Args>
    iterator
    _M_emplace_hint_equal(const_iterator __pos, _Args&&... __args);
#else
  pair<iterator, bool>
    _M_insert_unique(const value_type& __x);

  pair<iterator, bool>
    _M_insert_interval_unique(const value_type& __beg, const value_type& __end );

  iterator
    _M_insert_equal(const value_type& __x);

  iterator
    _M_insert_unique_(const_iterator __position, const value_type& __x);

  iterator
    _M_insert_equal_(const_iterator __position, const value_type& __x);
#endif

  template<typename _InputIterator>
    void
    _M_insert_unique(_InputIterator __first, _InputIterator __last);

  template<typename _InputIterator>
    void
    _M_insert_equal(_InputIterator __first, _InputIterator __last);

  private:
  void
    _M_erase_aux(const_iterator __position);

  void
    _M_erase_aux(const_iterator __first, const_iterator __last);

  public:
#if __cplusplus > 201103L
  // _GLIBCXX_RESOLVE_LIB_DEFECTS
  // DR 130. Associative erase should return an iterator.
  _GLIBCXX_ABI_TAG_CXX11
    iterator
    erase(const_iterator __position)
    {
      const_iterator __result = __position;
      ++__result;
      _M_erase_aux(__position);
      return __result._M_const_cast();
    }

  // LWG 2059.
  _GLIBCXX_ABI_TAG_CXX11
    iterator
    erase(iterator __position)
    {
      iterator __result = __position;
      ++__result;
      _M_erase_aux(__position);
      return __result;
    }
#else
  void
    erase(iterator __position)
    { _M_erase_aux(__position); }

  void
    erase(const_iterator __position)
    { _M_erase_aux(__position); }
#endif
  size_type
    erase(const key_type& __x);

#if __cplusplus > 201103L
  // _GLIBCXX_RESOLVE_LIB_DEFECTS
  // DR 130. Associative erase should return an iterator.
  _GLIBCXX_ABI_TAG_CXX11
    iterator
    erase(const_iterator __first, const_iterator __last)
    {
      _M_erase_aux(__first, __last);
      return __last._M_const_cast();
    }
#else
  void
    erase(iterator __first, iterator __last)
    { _M_erase_aux(__first, __last); }

  void
    erase(const_iterator __first, const_iterator __last)
    { _M_erase_aux(__first, __last); }
#endif
  void
    erase(const key_type* __first, const key_type* __last);

  void
    clear() _GLIBCXX_NOEXCEPT
    {
      _M_erase(_M_begin());
      _M_leftmost() = _M_end();
      _M_root() = 0;
      _M_rightmost() = _M_end();
      _M_impl._M_node_count = 0;
    }

  // Set operations.
  iterator
    find(const key_type& __k);

  const_iterator
    find(const key_type& __k) const;

  size_type
    count(const key_type& __k) const;

  iterator
    lower_bound(const key_type& __k)
    { return _M_lower_bound(_M_begin(), _M_end(), __k); }

  const_iterator
    lower_bound(const key_type& __k) const
    { return _M_lower_bound(_M_begin(), _M_end(), __k); }

  iterator
    upper_bound(const key_type& __k)
    { return _M_upper_bound(_M_begin(), _M_end(), __k); }

  const_iterator
    upper_bound(const key_type& __k) const
    { return _M_upper_bound(_M_begin(), _M_end(), __k); }

  pair<iterator, iterator>
    equal_range(const key_type& __k);

  pair<const_iterator, const_iterator>
    equal_range(const key_type& __k) const;

  // Debugging.
  bool
    __rb_verify() const;

  // 20140620
  _Interval_rb_tree_node_base*
_Interval_rb_tree_rebalance_for_erase(_Interval_rb_tree_node_base* const __z, 
    _Interval_rb_tree_node_base& __header) throw ();

  void 
_Interval_rb_tree_insert_and_rebalance(const bool          __insert_left,
    _Interval_rb_tree_node_base* __x,
    _Interval_rb_tree_node_base* __p,
    _Interval_rb_tree_node_base& __header) throw ();

 // _Interval_rb_tree_node_base*
//local_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ();
  
//  _Interval_rb_tree_node_base*
//local_Interval_rb_tree_increment(_Interval_rb_tree_node_base* __x) throw ();


//  _Interval_rb_tree_node_base*
//_Interval_rb_tree_increment(_Interval_rb_tree_node_base* __x) throw ();


// const _Interval_rb_tree_node_base*
//_Interval_rb_tree_increment(const _Interval_rb_tree_node_base* __x) throw ();



//  _Interval_rb_tree_node_base*
//_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ();

//  const _Interval_rb_tree_node_base*
//_Interval_rb_tree_decrement(const _Interval_rb_tree_node_base* __x) throw ();


void
local_Interval_rb_tree_rotate_left(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root);


void
_Interval_rb_tree_rotate_left(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root);

void
local_Interval_rb_tree_rotate_right(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root);

void
_Interval_rb_tree_rotate_right(_Interval_rb_tree_node_base* const __x, _Interval_rb_tree_node_base*& __root);


//  _Interval_rb_tree_node_base*
//local_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ();



#if __cplusplus > 201103L
  bool
    _M_move_assign(_Interval_rb_tree&);

  private:
  // Move elements from container with equal allocator.
  void
    _M_move_data(_Interval_rb_tree&, std::true_type);

  // Move elements from container with possibly non-equal allocator,
  // which might result in a copy not a move.
  void
    _M_move_data(_Interval_rb_tree&, std::false_type);
#endif
};

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator==(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{
  return __x.size() == __y.size()
    && std::equal(__x.begin(), __x.end(), __y.begin());
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator<(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{
  return std::lexicographical_compare(__x.begin(), __x.end(), 
      __y.begin(), __y.end());
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator!=(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{ return !(__x == __y); }

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator>(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{ return __y < __x; }

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator<=(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{ return !(__y < __x); }

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline bool
operator>=(const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    const _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{ return !(__x < __y); }

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  inline void
swap(_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __x,
    _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __y)
{ __x.swap(__y); }

#if __cplusplus > 201103L
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  _Interval_rb_tree(_Interval_rb_tree&& __x, _Node_allocator&& __a)
: _M_impl(__x._M_impl._M_key_compare, std::move(__a))
{
  using __eq = integral_constant<bool, _Alloc_traits::_S_always_equal()>;
  if (__x._M_root() != 0)
    _M_move_data(__x, __eq());
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_move_data(_Interval_rb_tree& __x, std::true_type)
{
  _M_root() = __x._M_root();
  _M_leftmost() = __x._M_leftmost();
  _M_rightmost() = __x._M_rightmost();
  _M_root()->_M_parent = _M_end();

  __x._M_root() = 0;
  __x._M_leftmost() = __x._M_end();
  __x._M_rightmost() = __x._M_end();

  this->_M_impl._M_node_count = __x._M_impl._M_node_count;
  __x._M_impl._M_node_count = 0;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_move_data(_Interval_rb_tree& __x, std::false_type)
{
  if (_M_get_Node_allocator() == __x._M_get_Node_allocator())
    _M_move_data(__x, std::true_type());
  else
  {
    _M_root() = _M_copy(__x._M_begin(), _M_end());
    _M_leftmost() = _S_minimum(_M_root());
    _M_rightmost() = _S_maximum(_M_root());
    _M_impl._M_node_count = __x._M_impl._M_node_count;
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  bool
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_move_assign(_Interval_rb_tree& __x)
{
  if (_Alloc_traits::_S_propagate_on_move_assign()
      || _Alloc_traits::_S_always_equal()
      || _M_get_Node_allocator() == __x._M_get_Node_allocator())
  {
    clear();
    if (__x._M_root() != 0)
      _M_move_data(__x, std::true_type());
    std::__alloc_on_move(_M_get_Node_allocator(),
        __x._M_get_Node_allocator());
    return true;
  }
  return false;
}
#endif

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>&
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
operator=(const _Interval_rb_tree& __x)
{
  if (this != &__x)
  {
    // Note that _Key may be a constant type.
    clear();
    this->_stride = __x._stride;
    this->_border_stride = __x._border_stride;
#if __cplusplus > 201103L
    if (_Alloc_traits::_S_propagate_on_copy_assign())
    {
      auto& __this_alloc = this->_M_get_Node_allocator();
      auto& __that_alloc = __x._M_get_Node_allocator();
      if (!_Alloc_traits::_S_always_equal()
          && __this_alloc != __that_alloc)
      {
        std::__alloc_on_copy(__this_alloc, __that_alloc);
      }
    }
#endif
    _M_impl._M_key_compare = __x._M_impl._M_key_compare;
    if (__x._M_root() != 0)
    {
      _M_root() = _M_copy(__x._M_begin(), _M_end());
      _M_leftmost() = _S_minimum(_M_root());
      _M_rightmost() = _S_maximum(_M_root());
      _M_impl._M_node_count = __x._M_impl._M_node_count;
    }
  }
  return *this;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_(_Base_ptr __x, _Base_ptr __p, _Arg&& __v)
#else
_M_insert_(_Base_ptr __x, _Base_ptr __p, const _Val& __v)
#endif
{
  bool __insert_left = (__x != 0 || __p == _M_end()
      || _M_impl._M_key_compare(_KeyOfValue()(__v),
        _S_key(__p)));

  _Link_type __z = _M_create_node(_GLIBCXX_FORWARD_OLD(_Arg, __v));
  __z->_M_low = __v;
  __z->_M_up = __v;
  __z->_M_max = __v;
 // __z->_M_num = 1;

  _Interval_rb_tree_insert_and_rebalance(__insert_left, __z, __p,
      this->_M_impl._M_header);
  ++_M_impl._M_node_count;
  return iterator(__z);
}

// lxj Only for cplusplus < 201103L
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_insert_interval_(_Base_ptr __x, _Base_ptr __p, const _Val& __v, const _Val& __up2)
{
  bool __insert_left = (__x != 0 || __p == _M_end()
      || _M_impl._M_key_compare(_KeyOfValue()(__v),
        _S_key(__p)));

  _Link_type __z = _M_create_node(_GLIBCXX_FORWARD_OLD(_Arg, __v));
  __z->_M_low = __v;
  __z->_M_up = __up2;
  __z->_M_max = __up2;
  //__z->_M_num = 1;

  //  bug? update node's max value.
  _Interval_rb_tree_insert_and_rebalance(__insert_left, __z, __p,
      this->_M_impl._M_header);
  ++_M_impl._M_node_count;
  return iterator(__z);
}




template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_lower(_Base_ptr __p, _Arg&& __v)
#else
_M_insert_lower(_Base_ptr __p, const _Val& __v)
#endif
{
  bool __insert_left = (__p == _M_end()
      || !_M_impl._M_key_compare(_S_key(__p),
        _KeyOfValue()(__v)));

  _Link_type __z = _M_create_node(_GLIBCXX_FORWARD_OLD(_Arg, __v));

  _Interval_rb_tree_insert_and_rebalance(__insert_left, __z, __p,
      this->_M_impl._M_header);
  ++_M_impl._M_node_count;
  return iterator(__z);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_equal_lower(_Arg&& __v)
#else
_M_insert_equal_lower(const _Val& __v)
#endif
{
  _Link_type __x = _M_begin();
  _Link_type __y = _M_end();
  while (__x != 0)
  {
    __y = __x;
    __x = !_M_impl._M_key_compare(_S_key(__x), _KeyOfValue()(__v)) ?
      _S_left(__x) : _S_right(__x);
  }
  return _M_insert_lower(__y, _GLIBCXX_FORWARD_OLD(_Arg, __v));
}

template<typename _Key, typename _Val, typename _KoV,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KoV, _Compare, _Alloc>::_Link_type
  _Interval_rb_tree<_Key, _Val, _KoV, _Compare, _Alloc>::
_M_copy(_Const_Link_type __x, _Link_type __p)
{
  // Structural copy.  __x and __p must be non-null.
  _Link_type __top = _M_clone_node(__x);
  __top->_M_parent = __p;

  __try
  {
    if (__x->_M_right)
      __top->_M_right = _M_copy(_S_right(__x), __top);
    __p = __top;
    __x = _S_left(__x);

    while (__x != 0)
    {
      _Link_type __y = _M_clone_node(__x);
      __p->_M_left = __y;
      __y->_M_parent = __p;
      if (__x->_M_right)
        __y->_M_right = _M_copy(_S_right(__x), __y);
      __p = __y;
      __x = _S_left(__x);
    }
  }
  __catch(...)
  {
    _M_erase(__top);
    __throw_exception_again;
  }
  return __top;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_erase(_Link_type __x)
{
  // Erase without rebalancing.
  while (__x != 0)
  {
    _M_erase(_S_right(__x));
    _Link_type __y = _S_left(__x);
    _M_destroy_node(__x);
    __x = __y;
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_lower_bound(_Link_type __x, _Link_type __y,
    const _Key& __k)
{
  while (__x != 0)
    if (!_M_impl._M_key_compare(_S_key(__x), __k))
      __y = __x, __x = _S_left(__x);
    else
      __x = _S_right(__x);
  return iterator(__y);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::const_iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  _M_lower_bound(_Const_Link_type __x, _Const_Link_type __y,
      const _Key& __k) const
{
  while (__x != 0)
    if (!_M_impl._M_key_compare(_S_key(__x), __k))
      __y = __x, __x = _S_left(__x);
    else
      __x = _S_right(__x);
  return const_iterator(__y);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_upper_bound(_Link_type __x, _Link_type __y,
    const _Key& __k)
{
  while (__x != 0)
    if (_M_impl._M_key_compare(__k, _S_key(__x)))
      __y = __x, __x = _S_left(__x);
    else
      __x = _S_right(__x);
  return iterator(__y);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::const_iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  _M_upper_bound(_Const_Link_type __x, _Const_Link_type __y,
      const _Key& __k) const
{
  while (__x != 0)
    if (_M_impl._M_key_compare(__k, _S_key(__x)))
      __y = __x, __x = _S_left(__x);
    else
      __x = _S_right(__x);
  return const_iterator(__y);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
equal_range(const _Key& __k)
{
  _Link_type __x = _M_begin();
  _Link_type __y = _M_end();
  while (__x != 0)
  {
    if (_M_impl._M_key_compare(_S_key(__x), __k))
      __x = _S_right(__x);
    else if (_M_impl._M_key_compare(__k, _S_key(__x)))
      __y = __x, __x = _S_left(__x);
    else
    {
      _Link_type __xu(__x), __yu(__y);
      __y = __x, __x = _S_left(__x);
      __xu = _S_right(__xu);
      return pair<iterator,
             iterator>(_M_lower_bound(__x, __y, __k),
                 _M_upper_bound(__xu, __yu, __k));
    }
  }
  return pair<iterator, iterator>(iterator(__y),
      iterator(__y));
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::const_iterator,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::const_iterator>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  equal_range(const _Key& __k) const
{
  _Const_Link_type __x = _M_begin();
  _Const_Link_type __y = _M_end();
  while (__x != 0)
  {
    if (_M_impl._M_key_compare(_S_key(__x), __k))
      __x = _S_right(__x);
    else if (_M_impl._M_key_compare(__k, _S_key(__x)))
      __y = __x, __x = _S_left(__x);
    else
    {
      _Const_Link_type __xu(__x), __yu(__y);
      __y = __x, __x = _S_left(__x);
      __xu = _S_right(__xu);
      return pair<const_iterator,
             const_iterator>(_M_lower_bound(__x, __y, __k),
                 _M_upper_bound(__xu, __yu, __k));
    }
  }
  return pair<const_iterator, const_iterator>(const_iterator(__y),
      const_iterator(__y));
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
swap(_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>& __t)
#if __cplusplus > 201103L
noexcept(_Alloc_traits::_S_nothrow_swap())
#endif
{
  if (_M_root() == 0)
  {
    if (__t._M_root() != 0)
    {
      _M_root() = __t._M_root();
      _M_leftmost() = __t._M_leftmost();
      _M_rightmost() = __t._M_rightmost();
      _M_root()->_M_parent = _M_end();

      __t._M_root() = 0;
      __t._M_leftmost() = __t._M_end();
      __t._M_rightmost() = __t._M_end();
    }
  }
  else if (__t._M_root() == 0)
  {
    __t._M_root() = _M_root();
    __t._M_leftmost() = _M_leftmost();
    __t._M_rightmost() = _M_rightmost();
    __t._M_root()->_M_parent = __t._M_end();

    _M_root() = 0;
    _M_leftmost() = _M_end();
    _M_rightmost() = _M_end();
  }
  else
  {
    std::swap(_M_root(),__t._M_root());
    std::swap(_M_leftmost(),__t._M_leftmost());
    std::swap(_M_rightmost(),__t._M_rightmost());

    _M_root()->_M_parent = _M_end();
    __t._M_root()->_M_parent = __t._M_end();
  }
  // No need to swap header's color as it does not change.
  std::swap(this->_M_impl._M_node_count, __t._M_impl._M_node_count);
  std::swap(this->_M_impl._M_key_compare, __t._M_impl._M_key_compare);

  _Alloc_traits::_S_on_swap(_M_get_Node_allocator(),
      __t._M_get_Node_allocator());
}

//
//
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_get_insert_unique_pos(const key_type& __k)
{
  typedef pair<_Base_ptr, _Base_ptr> _Res;
  _Link_type __x = _M_begin(); // _M_header->parent: root or 0
  _Link_type __y = _M_end(); // _M_header
  _Link_type __del;
  bool __comp = true;
  bool __insert = true;
  key_type  __low, __up;
  while (__x != 0)
  {
    __y = __x;

   __low = _S_low(__x);
   __up = _S_up(__x);

   //if( _M_impl._M_key_compare(__x->_M_max, __k ) )
   if( __x->_M_max < __k  )
      __x->_M_max = __k;

    // 1) __low-1 <= ___k <= __up+1  lxj
   if( (__low < __k + _border_stride ) && (__k <  __up + _border_stride) ){
   //if( _M_impl._M_key_compare(__low, __k + _border_stride ) && _M_impl._M_key_compare(__k,  __up + _border_stride) ){

     //if( _M_impl._M_key_compare(__k, __low ) )
     if( __k < __low  )
       __x->_M_low = __k;
     if( __up < __k  )
      __x-> _M_up = __k;

      //__x->_M_num++;
     _M_join_adjacent_nodes(__x, 0, 0 );
    
     while(!_to_erase.empty() ){
       __del = _to_erase.top();
       _M_erase_interval_node(const_iterator(static_cast<_Const_Link_type>(__del)));
       _to_erase.pop();
    }
     return _Res(__x, 0);
   }


  //if( _S_left(__x) && _M_impl._M_key_compare( __k, (_S_left(__x))->_M_max ) )
  __comp = __k < __low ; 
  __x = __comp ? _S_left(__x) : _S_right(__x);
#if 0
  if( _S_left(__x) && ( __k < (_S_left(__x))->_M_low ) )
  //if( _S_left(__x) && ( __k < (_S_left(__x))->_M_max ) )
      __x = _S_left(__x);
  else
     __x = _S_right(__x);

#endif
  }

  iterator __j = iterator(__y); // __y is the parent of insert_pos.
  if (__comp)
  {
    if (__j == begin()) // left-most node
      return _Res(__x, __y);  // first-node:(0, _M_header)
    else   // right-subtree, twice rotation.
      --__j;
  }
  // (__j.val < __k || __k < __j.val) == 0 ==> Not insert.
  //if (_M_impl._M_key_compare(_S_key(__j._M_node), __k)) // __j._val < __k
  if (_S_key(__j._M_node) < __k) // __j._val < __k
    return _Res(__x, __y);
  return _Res(__j._M_node, 0);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_erase_interval_node(const_iterator __position)
{
  _Link_type __y =
    static_cast<_Link_type>(_Interval_rb_tree_rebalance_for_erase
        (const_cast<_Base_ptr>(__position._M_node),
         this->_M_impl._M_header));
  _M_destroy_node(__y);
  --_M_impl._M_node_count;
}




  
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_get_insert_interval_unique_pos(const key_type& __low2, const key_type& __up2 )
{
  typedef pair<_Base_ptr, _Base_ptr> _Res;
  _Link_type __x = _M_begin(); // _M_header->parent: root or 0
  _Link_type __y = _M_end(); // _M_header
  _Link_type __del;
  bool __comp = true;
  key_type __low1, __up1;
  while (__x != 0)
  {
    __y = __x;
    __low1 = __x->_M_low; 
    __up1 = __x->_M_up; 


    //if(  _M_impl._M_key_compare( __x->_M_max, __up2) )
    if(   __x->_M_max < __up2 )
      __x->_M_max = __up2;

    // 1) Overlapping.
    //if( _M_impl._M_key_compare(__low2, __up1+_border_stride ) && _M_impl._M_key_compare(__low1, __up2 + _border_stride )  )
    if( (__low2 < __up1+_border_stride ) && (__low1 < __up2 + _border_stride )  ){
      // 1.1) __low2 < __low1, low1 = low2 
      //if(  _M_impl._M_key_compare( __low2, __low1 ) )
      if(   __low2 < __low1  ){
        __x->_M_low = __low2;
      }
      // 1.2) __up1 < __up2
      //if(  _M_impl._M_key_compare( __up1, __up2 ) )
      if(   __up1 < __up2  ){
        __x->_M_up = __up2;
      }
      //__x->_M_num += (__up2 - __low2)/_stride;
      _M_join_adjacent_nodes(__x, 0, 0 );

      while( !_to_erase.empty() ){
        __del = _to_erase.top();
        _M_erase_interval_node(const_iterator(static_cast<_Const_Link_type>(__del)));
        _to_erase.pop();
      }
      return _Res(0, 0);
    }
  #if 1
    //__comp = _M_impl._M_key_compare(__low2,  _S_key(__x));
    //__comp = (__low2 <  _S_key(__x));
    __comp = (__low2 <  __x->_M_low );
    __x = __comp ? _S_left(__x) : _S_right(__x);
  #endif
  }

  iterator __j = iterator(__y); // __y is the parent of insert_pos.
  // __x == _S_left(__y)
  if (__comp)
  {
    if (__j == begin()) // left-most node
      return _Res(__x, __y);  // first-node:(0, _M_header)
    else   // right-subtree, twice rotation.
      --__j;
  }
  // __x is the right-subtree's left-most node or __x == _S_right(__y)  
  //if (_M_impl._M_key_compare(_S_key(__j._M_node), __low2)) // __j._val < __k
  if ((_S_key(__j._M_node)< __low2)) // __j._val < __k
    return _Res(__x, __y);
  return _Res(__j._M_node, 0);
}



template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_get_insert_equal_pos(const key_type& __k)
{
  typedef pair<_Base_ptr, _Base_ptr> _Res;
  _Link_type __x = _M_begin();
  _Link_type __y = _M_end();
  while (__x != 0)
  {
    __y = __x;
   __x = _M_impl._M_key_compare(__k, _S_key(__x)) ?
      _S_left(__x) : _S_right(__x);
  }
  return _Res(__x, __y);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator, bool>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_unique(_Arg&& __v)
#else
_M_insert_unique(const _Val& __v)
#endif
{
  typedef pair<iterator, bool> _Res;
  pair<_Base_ptr, _Base_ptr> __res
    = _M_get_insert_unique_pos(_KeyOfValue()(__v));

  if (__res.second)
    return _Res(_M_insert_(__res.first, __res.second,
          _GLIBCXX_FORWARD_OLD(_Arg, __v)),
        true);

  //std::cout<<"Not insert \n";
  return _Res(iterator(static_cast<_Link_type>(__res.first)), false);
}


template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator, bool>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  // Only for cplusplus < 201103L 
_M_insert_interval_unique(const _Val& __beg, const _Val& __end)
{
  typedef pair<iterator, bool> _Res;
  pair<_Base_ptr, _Base_ptr> __res
    = _M_get_insert_interval_unique_pos(_KeyOfValue()(__beg), _KeyOfValue()(__end));

  if (__res.second)
    return _Res(_M_insert_interval_(__res.first, __res.second,
          _GLIBCXX_FORWARD_OLD(_Arg, __beg), _GLIBCXX_FORWARD_OLD(_Arg, __end) ),
        true);

  //std::cout<<"Not insert \n";
  return _Res(iterator(static_cast<_Link_type>(__res.first)), false);
}



template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_equal(_Arg&& __v)
#else
_M_insert_equal(const _Val& __v)
#endif
{
  pair<_Base_ptr, _Base_ptr> __res
    = _M_get_insert_equal_pos(_KeyOfValue()(__v));
  return _M_insert_(__res.first, __res.second, _GLIBCXX_FORWARD_OLD(_Arg, __v));
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_get_insert_hint_unique_pos(const_iterator __position,
    const key_type& __k)
{
  iterator __pos = __position._M_const_cast();
  typedef pair<_Base_ptr, _Base_ptr> _Res;

  // end()
  if (__pos._M_node == _M_end())
  {
    if (size() > 0
        && _M_impl._M_key_compare(_S_key(_M_rightmost()), __k))
      return _Res(0, _M_rightmost());
    else
      return _M_get_insert_unique_pos(__k);
  }
  else if (_M_impl._M_key_compare(__k, _S_key(__pos._M_node)))
  {
    // First, try before...
    iterator __before = __pos;
    if (__pos._M_node == _M_leftmost()) // begin()
      return _Res(_M_leftmost(), _M_leftmost());
    else if (_M_impl._M_key_compare(_S_key((--__before)._M_node), __k))
    {
      if (_S_right(__before._M_node) == 0)
        return _Res(0, __before._M_node);
      else
        return _Res(__pos._M_node, __pos._M_node);
    }
    else
      return _M_get_insert_unique_pos(__k);
  }
  else if (_M_impl._M_key_compare(_S_key(__pos._M_node), __k))
  {
    // ... then try after.
    iterator __after = __pos;
    if (__pos._M_node == _M_rightmost())
      return _Res(0, _M_rightmost());
    else if (_M_impl._M_key_compare(__k, _S_key((++__after)._M_node)))
    {
      if (_S_right(__pos._M_node) == 0)
        return _Res(0, __pos._M_node);
      else
        return _Res(__after._M_node, __after._M_node);
    }
    else
      return _M_get_insert_unique_pos(__k);
  }
  else
    // Equivalent keys.
    return _Res(__pos._M_node, 0);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_unique_(const_iterator __position, _Arg&& __v)
#else
_M_insert_unique_(const_iterator __position, const _Val& __v)
#endif
{
  pair<_Base_ptr, _Base_ptr> __res
    = _M_get_insert_hint_unique_pos(__position, _KeyOfValue()(__v));

  if (__res.second)
    return _M_insert_(__res.first, __res.second,
        _GLIBCXX_FORWARD_OLD(_Arg, __v));
  return iterator(static_cast<_Link_type>(__res.first));
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr,
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::_Base_ptr>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_get_insert_hint_equal_pos(const_iterator __position, const key_type& __k)
{
  iterator __pos = __position._M_const_cast();
  typedef pair<_Base_ptr, _Base_ptr> _Res;

  // end()
  if (__pos._M_node == _M_end())
  {
    if (size() > 0
        && !_M_impl._M_key_compare(__k, _S_key(_M_rightmost())))
      return _Res(0, _M_rightmost());
    else
      return _M_get_insert_equal_pos(__k);
  }
  else if (!_M_impl._M_key_compare(_S_key(__pos._M_node), __k))
  {
    // First, try before...
    iterator __before = __pos;
    if (__pos._M_node == _M_leftmost()) // begin()
      return _Res(_M_leftmost(), _M_leftmost());
    else if (!_M_impl._M_key_compare(__k, _S_key((--__before)._M_node)))
    {
      if (_S_right(__before._M_node) == 0)
        return _Res(0, __before._M_node);
      else
        return _Res(__pos._M_node, __pos._M_node);
    }
    else
      return _M_get_insert_equal_pos(__k);
  }
  else
  {
    // ... then try after.  
    iterator __after = __pos;
    if (__pos._M_node == _M_rightmost())
      return _Res(0, _M_rightmost());
    else if (!_M_impl._M_key_compare(_S_key((++__after)._M_node), __k))
    {
      if (_S_right(__pos._M_node) == 0)
        return _Res(0, __pos._M_node);
      else
        return _Res(__after._M_node, __after._M_node);
    }
    else
      return _Res(0, 0);
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
#if __cplusplus > 201103L
  template<typename _Arg>
#endif
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
#if __cplusplus > 201103L
_M_insert_equal_(const_iterator __position, _Arg&& __v)
#else
_M_insert_equal_(const_iterator __position, const _Val& __v)
#endif
{
  pair<_Base_ptr, _Base_ptr> __res
    = _M_get_insert_hint_equal_pos(__position, _KeyOfValue()(__v));

  if (__res.second)
    return _M_insert_(__res.first, __res.second,
        _GLIBCXX_FORWARD_OLD(_Arg, __v));

  return _M_insert_equal_lower(_GLIBCXX_FORWARD_OLD(_Arg, __v));
}

#if __cplusplus > 201103L
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_insert_node(_Base_ptr __x, _Base_ptr __p, _Link_type __z)
{
  bool __insert_left = (__x != 0 || __p == _M_end()
      || _M_impl._M_key_compare(_S_key(__z),
        _S_key(__p)));

  _Interval_rb_tree_insert_and_rebalance(__insert_left, __z, __p,
      this->_M_impl._M_header);
  ++_M_impl._M_node_count;
  return iterator(__z);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_insert_lower_node(_Base_ptr __p, _Link_type __z)
{
  bool __insert_left = (__p == _M_end()
      || !_M_impl._M_key_compare(_S_key(__p),
        _S_key(__z)));

  _Interval_rb_tree_insert_and_rebalance(__insert_left, __z, __p,
      this->_M_impl._M_header);
  ++_M_impl._M_node_count;
  return iterator(__z);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_insert_equal_lower_node(_Link_type __z)
{
  _Link_type __x = _M_begin();
  _Link_type __y = _M_end();
  while (__x != 0)
  {
    __y = __x;
    __x = !_M_impl._M_key_compare(_S_key(__x), _S_key(__z)) ?
      _S_left(__x) : _S_right(__x);
  }
  return _M_insert_lower_node(__y, __z);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  template<typename... _Args>
  pair<typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator, bool>
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_emplace_unique(_Args&&... __args)
{
  _Link_type __z = _M_create_node(std::forward<_Args>(__args)...);

  __try
  {
    typedef pair<iterator, bool> _Res;
    auto __res = _M_get_insert_unique_pos(_S_key(__z));
    if (__res.second)
      return _Res(_M_insert_node(__res.first, __res.second, __z), true);

    _M_destroy_node(__z);
    return _Res(iterator(static_cast<_Link_type>(__res.first)), false);
  }
  __catch(...)
  {
    _M_destroy_node(__z);
    __throw_exception_again;
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  template<typename... _Args>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_emplace_equal(_Args&&... __args)
{
  _Link_type __z = _M_create_node(std::forward<_Args>(__args)...);

  __try
  {
    auto __res = _M_get_insert_equal_pos(_S_key(__z));
    return _M_insert_node(__res.first, __res.second, __z);
  }
  __catch(...)
  {
    _M_destroy_node(__z);
    __throw_exception_again;
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  template<typename... _Args>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_emplace_hint_unique(const_iterator __pos, _Args&&... __args)
{
  _Link_type __z = _M_create_node(std::forward<_Args>(__args)...);

  __try
  {
    auto __res = _M_get_insert_hint_unique_pos(__pos, _S_key(__z));

    if (__res.second)
      return _M_insert_node(__res.first, __res.second, __z);

    _M_destroy_node(__z);
    return iterator(static_cast<_Link_type>(__res.first));
  }
  __catch(...)
  {
    _M_destroy_node(__z);
    __throw_exception_again;
  }
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  template<typename... _Args>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_emplace_hint_equal(const_iterator __pos, _Args&&... __args)
{
  _Link_type __z = _M_create_node(std::forward<_Args>(__args)...);

  __try
  {
    auto __res = _M_get_insert_hint_equal_pos(__pos, _S_key(__z));

    if (__res.second)
      return _M_insert_node(__res.first, __res.second, __z);

    return _M_insert_equal_lower_node(__z);
  }
  __catch(...)
  {
    _M_destroy_node(__z);
    __throw_exception_again;
  }
}
#endif

template<typename _Key, typename _Val, typename _KoV,
  typename _Cmp, typename _Alloc>
  template<class _II>
  void
  _Interval_rb_tree<_Key, _Val, _KoV, _Cmp, _Alloc>::
_M_insert_unique(_II __first, _II __last)
{
  for (; __first != __last; ++__first)
    _M_insert_unique_(end(), *__first);
}

template<typename _Key, typename _Val, typename _KoV,
  typename _Cmp, typename _Alloc>
  template<class _II>
  void
  _Interval_rb_tree<_Key, _Val, _KoV, _Cmp, _Alloc>::
_M_insert_equal(_II __first, _II __last)
{
  for (; __first != __last; ++__first)
    _M_insert_equal_(end(), *__first);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_erase_aux(const_iterator __position)
{
  _Link_type __y =
    static_cast<_Link_type>(_Interval_rb_tree_rebalance_for_erase
        (const_cast<_Base_ptr>(__position._M_node),
         this->_M_impl._M_header));
  _M_destroy_node(__y);
  --_M_impl._M_node_count;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_M_erase_aux(const_iterator __first, const_iterator __last)
{
  if (__first == begin() && __last == end())
    clear();
  else
    while (__first != __last)
      erase(__first++);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::size_type
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
erase(const _Key& __x)
{
  pair<iterator, iterator> __p = equal_range(__x);
  const size_type __old_size = size();
  erase(__p.first, __p.second);
  return __old_size - size();
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
erase(const _Key* __first, const _Key* __last)
{
  while (__first != __last)
    erase(*__first++);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
find(const _Key& __k)
{
  iterator __j = _M_lower_bound(_M_begin(), _M_end(), __k);
  return (__j == end()
      || _M_impl._M_key_compare(__k,
        _S_key(__j._M_node))) ? end() : __j;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue,
  _Compare, _Alloc>::const_iterator
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  find(const _Key& __k) const
{
  const_iterator __j = _M_lower_bound(_M_begin(), _M_end(), __k);
  return (__j == end()
      || _M_impl._M_key_compare(__k, 
        _S_key(__j._M_node))) ? end() : __j;
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  typename _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::size_type
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
  count(const _Key& __k) const
{
  pair<const_iterator, const_iterator> __p = equal_range(__k);
  const size_type __n = std::distance(__p.first, __p.second);
  return __n;
}

_GLIBCXX_PURE unsigned int
_Interval_rb_tree_black_count(const _Interval_rb_tree_node_base* __node,
    const _Interval_rb_tree_node_base* __root) throw ();

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  bool
  _Interval_rb_tree<_Key,_Val,_KeyOfValue,_Compare,_Alloc>::__rb_verify() const
{
  if (_M_impl._M_node_count == 0 || begin() == end())
    return _M_impl._M_node_count == 0 && begin() == end()
      && this->_M_impl._M_header._M_left == _M_end()
      && this->_M_impl._M_header._M_right == _M_end();

  unsigned int __len = _Interval_rb_tree_black_count(_M_leftmost(), _M_root());
  for (const_iterator __it = begin(); __it != end(); ++__it)
  {
    _Const_Link_type __x = static_cast<_Const_Link_type>(__it._M_node);
    _Const_Link_type __L = _S_left(__x);
    _Const_Link_type __R = _S_right(__x);

    if (__x->_M_color == _S_interval_red)
      if ((__L && __L->_M_color == _S_interval_red)
          || (__R && __R->_M_color == _S_interval_red))
        return false;

    if (__L && _M_impl._M_key_compare(_S_key(__x), _S_key(__L)))
      return false;
    if (__R && _M_impl._M_key_compare(_S_key(__R), _S_key(__x)))
      return false;

    if (!__L && !__R && _Interval_rb_tree_black_count(__x, _M_root()) != __len)
      return false;
  }

  if (_M_leftmost() != _Interval_rb_tree_node_base::_S_minimum(_M_root()))
    return false;
  if (_M_rightmost() != _Interval_rb_tree_node_base::_S_maximum(_M_root()))
    return false;
  return true;
}

// Copied from src/c++98/tree.cc.  lxj
//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
  _Interval_rb_tree_node_base*
  //_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
local_Interval_rb_tree_increment(_Interval_rb_tree_node_base* __x) throw ()
{
  if (__x->_M_right != 0) 
  {
    __x = __x->_M_right;
    while (__x->_M_left != 0)
      __x = __x->_M_left;
  }
  else 
  {
    _Interval_rb_tree_node_base* __y = __x->_M_parent;
    while (__x == __y->_M_right) 
    {
      __x = __y;
      __y = __y->_M_parent;
    }
    if (__x->_M_right != __y)
      __x = __y;
  }
  return __x;
}

//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
  _Interval_rb_tree_node_base*
  //_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_increment(_Interval_rb_tree_node_base* __x) throw ()
{
  return local_Interval_rb_tree_increment(__x);
}

//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
 const _Interval_rb_tree_node_base*
 // _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_increment(const _Interval_rb_tree_node_base* __x) throw ()
{
  return local_Interval_rb_tree_increment(const_cast<_Interval_rb_tree_node_base*>(__x));
}

//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
  _Interval_rb_tree_node_base*
  //_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
local_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ()
{
  if (__x->_M_color == _S_interval_red 
      && __x->_M_parent->_M_parent == __x)
    __x = __x->_M_right;
  else if (__x->_M_left != 0) 
  {
    _Interval_rb_tree_node_base* __y = __x->_M_left;
    while (__y->_M_right != 0)
      __y = __y->_M_right;
    __x = __y;
  }
  else 
  {
    _Interval_rb_tree_node_base* __y = __x->_M_parent;
    while (__x == __y->_M_left) 
    {
      __x = __y;
      __y = __y->_M_parent;
    }
    __x = __y;
  }
  return __x;
}

//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
  _Interval_rb_tree_node_base*
//  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_decrement(_Interval_rb_tree_node_base* __x) throw ()
{
  return local_Interval_rb_tree_decrement(__x);
}

//template<typename _Key, typename _Val, typename _KeyOfValue,
 // typename _Compare, typename _Alloc>
  const _Interval_rb_tree_node_base*
  //_Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_decrement(const _Interval_rb_tree_node_base* __x) throw ()
{
  return local_Interval_rb_tree_decrement(const_cast<_Interval_rb_tree_node_base*>(__x));
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
local_Interval_rb_tree_rotate_left(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root)
{
  _Interval_rb_tree_node_base* const __y = __x->_M_right;

  _Interval_rb_tree_node<long> * __sx ,*__sy; 
  __sx = static_cast<_Interval_rb_tree_node<long> *>( __x);

  __x->_M_right = __y->_M_left;
  if (__y->_M_left !=0){
    __y->_M_left->_M_parent = __x;
  }
  __y->_M_parent = __x->_M_parent;

  // update recompute __x->_M_max
  // if __y has left subtree
  if(!__x->_M_left && !__x->_M_right )
    __sx->_M_max = __sx->_M_up;
  else{
  // if __y has left subtree
    if( __x->_M_right ){
      __sy = static_cast<_Interval_rb_tree_node<long> *>(__x->_M_right);
      if( __sx->_M_up < __sy->_M_max )
        __sx->_M_max = __sy->_M_max;
      else
        __sx->_M_max = __sx->_M_up;
    }
    // bug? __y has not  left-subtree, so __x has no right subtree.
    else if(__x->_M_left )
    {
      __sy = static_cast<_Interval_rb_tree_node<long> *>(__x->_M_left);
      if(  __sx->_M_max < __sy->_M_max )
        //if(  __sx->_M_max < __sy->_M_max )
        __sx->_M_max = __sy->_M_max;
    }
  }

  // update __y->_M_max
  __sy = static_cast<_Interval_rb_tree_node<long> *>(__y);
  if(  __sy->_M_max < __sx->_M_max )
    __sy->_M_max = __sx->_M_max;



  if (__x == __root)
    __root = __y;
  else if (__x == __x->_M_parent->_M_left)
    __x->_M_parent->_M_left = __y;
  else
    __x->_M_parent->_M_right = __y;
  __y->_M_left = __x;
  __x->_M_parent = __y;
}

/* Static keyword was missing on _Interval_rb_tree_rotate_left.
   Export the symbol for backward compatibility until
   next ABI change.  */
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_rotate_left(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root)
{
  local_Interval_rb_tree_rotate_left (__x, __root);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
local_Interval_rb_tree_rotate_right(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root)
{
  _Interval_rb_tree_node_base* const __y = __x->_M_left;

  // update __y-> _M_max.  because __y->max <= __x->max exists, so we just
  _Interval_rb_tree_node<long> * __sx, *__sy; // bug?? not general.
  __sx = static_cast<_Interval_rb_tree_node<long>*>(__x);
  __sy = static_cast<_Interval_rb_tree_node<long>*>(__y);
  __sy->_M_max = __sx->_M_max;

  __x->_M_left = __y->_M_right; // y->right is x->left
  if (__y->_M_right != 0)
    __y->_M_right->_M_parent = __x;
  __y->_M_parent = __x->_M_parent; // 

  if (__x == __root)
    __root = __y;
  else if (__x == __x->_M_parent->_M_right)
    __x->_M_parent->_M_right = __y;
  else
    __x->_M_parent->_M_left = __y;
  __y->_M_right = __x;
  __x->_M_parent = __y;
}

/* Static keyword was missing on _Interval_rb_tree_rotate_right
   Export the symbol for backward compatibility until
   next ABI change.  */
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_rotate_right(_Interval_rb_tree_node_base* const __x, 
    _Interval_rb_tree_node_base*& __root)
{
  local_Interval_rb_tree_rotate_right (__x, __root);
}

template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
void
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_insert_and_rebalance(const bool          __insert_left,
    _Interval_rb_tree_node_base* __x,
    _Interval_rb_tree_node_base* __p,
    _Interval_rb_tree_node_base& __header) throw ()
{
  _Interval_rb_tree_node_base *& __root = __header._M_parent;

  // Initialize fields in new node to insert.
  __x->_M_parent = __p;
  __x->_M_left = 0;
  __x->_M_right = 0;
  __x->_M_color = _S_interval_red;

  // Insert.
  // Make new node child of parent and maintain root, leftmost and
  // rightmost nodes.
  // N.B. First node is always inserted left.
  if (__insert_left)
  {
    __p->_M_left = __x; // also makes leftmost = __x when __p == &__header

    if (__p == &__header)
    {
      __header._M_parent = __x;
      __header._M_right = __x;
    }
    else if (__p == __header._M_left)
      __header._M_left = __x; // maintain leftmost pointing to min node
  }
  else
  {
    __p->_M_right = __x;

    if (__p == __header._M_right)
      __header._M_right = __x; // maintain rightmost pointing to max node
  }
  // Rebalance.
  while (__x != __root 
      && __x->_M_parent->_M_color == _S_interval_red) 
  {
    _Interval_rb_tree_node_base* const __xpp = __x->_M_parent->_M_parent;

    if (__x->_M_parent == __xpp->_M_left) 
    {
      _Interval_rb_tree_node_base* const __y = __xpp->_M_right;
      if (__y && __y->_M_color == _S_interval_red) 
      {
        __x->_M_parent->_M_color = _S_interval_black;
        __y->_M_color = _S_interval_black;
        __xpp->_M_color = _S_interval_red;
        __x = __xpp;
      }
      else 
      {
        if (__x == __x->_M_parent->_M_right) 
        {
          __x = __x->_M_parent;
          local_Interval_rb_tree_rotate_left(__x, __root);
        }
        __x->_M_parent->_M_color = _S_interval_black;
        __xpp->_M_color = _S_interval_red;
        local_Interval_rb_tree_rotate_right(__xpp, __root);
      }
    }
    else 
    {
      _Interval_rb_tree_node_base* const __y = __xpp->_M_left;
      if (__y && __y->_M_color == _S_interval_red) 
      {
        __x->_M_parent->_M_color = _S_interval_black;
        __y->_M_color = _S_interval_black;
        __xpp->_M_color = _S_interval_red;
        __x = __xpp;
      }
      else 
      {
        if (__x == __x->_M_parent->_M_left) 
        {
          __x = __x->_M_parent;
          local_Interval_rb_tree_rotate_right(__x, __root);
        }
        __x->_M_parent->_M_color = _S_interval_black;
        __xpp->_M_color = _S_interval_red;
        local_Interval_rb_tree_rotate_left(__xpp, __root);
      }
    }
  }
  __root->_M_color = _S_interval_black;
}

// bug? To update _M_max. 
template<typename _Key, typename _Val, typename _KeyOfValue,
  typename _Compare, typename _Alloc>
  _Interval_rb_tree_node_base*
  _Interval_rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
_Interval_rb_tree_rebalance_for_erase(_Interval_rb_tree_node_base* const __z, 
    _Interval_rb_tree_node_base& __header) throw ()
{
  _Interval_rb_tree_node_base *& __root = __header._M_parent;
  _Interval_rb_tree_node_base *& __leftmost = __header._M_left;
  _Interval_rb_tree_node_base *& __rightmost = __header._M_right;
  _Interval_rb_tree_node_base* __y = __z;
  _Interval_rb_tree_node_base* __x = 0;
  _Interval_rb_tree_node_base* __x_parent = 0;

  if (__y->_M_left == 0)     // __z has at most one non-null child. y == z.
    __x = __y->_M_right;     // __x might be null.
  else
    if (__y->_M_right == 0)  // __z has exactly one non-null child. y == z.
      __x = __y->_M_left;    // __x is not null.
    else 
    {
      // __z has two non-null children.  Set __y to
      __y = __y->_M_right;   //   __z's successor.  __x might be null.
      while (__y->_M_left != 0)
        __y = __y->_M_left;
      __x = __y->_M_right;
    }
  if (__y != __z) 
  {
    // relink y in place of z.  y is z's successor
    __z->_M_left->_M_parent = __y; 
    __y->_M_left = __z->_M_left;
    if (__y != __z->_M_right) 
    {
      __x_parent = __y->_M_parent;
      if (__x) __x->_M_parent = __y->_M_parent;
      __y->_M_parent->_M_left = __x;   // __y must be a child of _M_left
      __y->_M_right = __z->_M_right;
      __z->_M_right->_M_parent = __y;
    }
    else
      __x_parent = __y;  
    if (__root == __z)
      __root = __y;
    else if (__z->_M_parent->_M_left == __z)
      __z->_M_parent->_M_left = __y;
    else 
      __z->_M_parent->_M_right = __y;
    __y->_M_parent = __z->_M_parent;
    std::swap(__y->_M_color, __z->_M_color);
    __y = __z;
    // __y now points to node to be actually deleted
  }
  else 
  {                        // __y == __z
    __x_parent = __y->_M_parent;
    if (__x) 
      __x->_M_parent = __y->_M_parent;   
    if (__root == __z)
      __root = __x;
    else 
      if (__z->_M_parent->_M_left == __z)
        __z->_M_parent->_M_left = __x;
      else
        __z->_M_parent->_M_right = __x;
    if (__leftmost == __z) 
    {
      if (__z->_M_right == 0)        // __z->_M_left must be null also
        __leftmost = __z->_M_parent;
      // makes __leftmost == _M_header if __z == __root
      else
        __leftmost = _Interval_rb_tree_node_base::_S_minimum(__x);
    }
    if (__rightmost == __z)  
    {
      if (__z->_M_left == 0)         // __z->_M_right must be null also
        __rightmost = __z->_M_parent;  
      // makes __rightmost == _M_header if __z == __root
      else                      // __x == __z->_M_left
        __rightmost = _Interval_rb_tree_node_base::_S_maximum(__x);
    }
  }
  if (__y->_M_color != _S_interval_red) 
  { 
    while (__x != __root && (__x == 0 || __x->_M_color == _S_interval_black))
      if (__x == __x_parent->_M_left) 
      {
        _Interval_rb_tree_node_base* __w = __x_parent->_M_right;
        if (__w->_M_color == _S_interval_red) 
        {
          __w->_M_color = _S_interval_black;
          __x_parent->_M_color = _S_interval_red;
          local_Interval_rb_tree_rotate_left(__x_parent, __root);
          __w = __x_parent->_M_right;
        }
        if ((__w->_M_left == 0 || 
              __w->_M_left->_M_color == _S_interval_black) &&
            (__w->_M_right == 0 || 
             __w->_M_right->_M_color == _S_interval_black)) 
        {
          __w->_M_color = _S_interval_red;
          __x = __x_parent;
          __x_parent = __x_parent->_M_parent;
        } 
        else 
        {
          if (__w->_M_right == 0 
              || __w->_M_right->_M_color == _S_interval_black) 
          {
            __w->_M_left->_M_color = _S_interval_black;
            __w->_M_color = _S_interval_red;
            local_Interval_rb_tree_rotate_right(__w, __root);
            __w = __x_parent->_M_right;
          }
          __w->_M_color = __x_parent->_M_color;
          __x_parent->_M_color = _S_interval_black;
          if (__w->_M_right) 
            __w->_M_right->_M_color = _S_interval_black;
          local_Interval_rb_tree_rotate_left(__x_parent, __root);
          break;
        }
      } 
      else 
      {   
        // same as above, with _M_right <-> _M_left.
        _Interval_rb_tree_node_base* __w = __x_parent->_M_left;
        if (__w->_M_color == _S_interval_red) 
        {
          __w->_M_color = _S_interval_black;
          __x_parent->_M_color = _S_interval_red;
          local_Interval_rb_tree_rotate_right(__x_parent, __root);
          __w = __x_parent->_M_left;
        }
        if ((__w->_M_right == 0 || 
              __w->_M_right->_M_color == _S_interval_black) &&
            (__w->_M_left == 0 || 
             __w->_M_left->_M_color == _S_interval_black)) 
        {
          __w->_M_color = _S_interval_red;
          __x = __x_parent;
          __x_parent = __x_parent->_M_parent;
        } 
        else 
        {
          if (__w->_M_left == 0 || __w->_M_left->_M_color == _S_interval_black) 
          {
            __w->_M_right->_M_color = _S_interval_black;
            __w->_M_color = _S_interval_red;
            local_Interval_rb_tree_rotate_left(__w, __root);
            __w = __x_parent->_M_left;
          }
          __w->_M_color = __x_parent->_M_color;
          __x_parent->_M_color = _S_interval_black;
          if (__w->_M_left) 
            __w->_M_left->_M_color = _S_interval_black;
          local_Interval_rb_tree_rotate_right(__x_parent, __root);
          break;
        }
      }
    if (__x) __x->_M_color = _S_interval_black;
  }
  return __y;
}

  unsigned int
_Interval_rb_tree_black_count(const _Interval_rb_tree_node_base* __node,
    const _Interval_rb_tree_node_base* __root) throw ()
{
  if (__node == 0)
    return 0;
  unsigned int __sum = 0;
  do 
  {
    if (__node->_M_color == _S_interval_black) 
      ++__sum;
    if (__node == __root) 
      break;
    __node = __node->_M_parent;
  } 
  while (1);
  return __sum;
}








_GLIBCXX_END_NAMESPACE_VERSION
} // namespace

#endif
