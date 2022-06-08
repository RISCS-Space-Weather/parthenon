//========================================================================================
// Athena++ astrophysical MHD code
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code contributors
// Licensed under the 3-clause BSD License, see LICENSE file for details
//========================================================================================
// (C) (or copyright) 2020-2022. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================
#ifndef PARTHENON_ARRAY_GENERIC_HPP_
#define PARTHENON_ARRAY_GENERIC_HPP_

#include <string>
#include <utility>

namespace parthenon {

namespace impl {
template <bool...>
struct bool_pack;
template <bool... bs>
using all_true = std::is_same<bool_pack<bs..., true>, bool_pack<true, bs...>>;

template <class... Ts>
using are_all_integral = all_true<std::is_integral<Ts>::value...>;

template <class T>
bool all_greater_than(T) {
  return true;
}
template <class T, class Arg, class... Args>
bool all_greater_than(T val, Arg v, Args... args) {
  return (v > val) && all_greater_than(val, args...);
}

template <class... Ts>
using enable_if_all_integral = std::enable_if<are_all_integral<Ts...>::value>;
} // namespace impl

using namespace impl;

// API designed with Data = Kokkos::View<T******> in mind
template <typename Data>
class ParArrayGeneric {
 public:
  using index_pair_t = std::pair<size_t, size_t>;
  using base_t = Data;
  using HostMirror = ParArrayGeneric<typename Data::HostMirror>;
  using host_mirror_type = HostMirror;

  ParArrayGeneric() = default;
  __attribute__((nothrow)) virtual ~ParArrayGeneric() = default;
  __attribute__((nothrow)) ParArrayGeneric(const ParArrayGeneric<Data> &t) = default;
  __attribute__((nothrow)) ParArrayGeneric<Data> &
  operator=(const ParArrayGeneric<Data> &t) = default;
  __attribute__((nothrow)) ParArrayGeneric(ParArrayGeneric<Data> &&t) = default;
  __attribute__((nothrow)) ParArrayGeneric<Data> &
  operator=(ParArrayGeneric<Data> &&t) = default;
  
  KOKKOS_INLINE_FUNCTION
  explicit ParArrayGeneric(const Data &v) : data_(v) {}

  // Allow a ParArrayGeneric to be cast to any compatible Kokkos view
  template <class... Ts>
  operator ParArrayGeneric<Kokkos::View<Ts...>>() const {
    return ParArrayGeneric<Kokkos::View<Ts...>>(data_);
  }

  template <class... Ts>
  operator Kokkos::View<Ts...>() const {
    return data_;
  }

  template <class... Args, class = typename enable_if_all_integral<Args...>::type>
  ParArrayGeneric(const std::string &label, Args... args)
      : ParArrayGeneric(label, std::make_index_sequence<Data::rank - sizeof...(Args)>{},
                        args...) {
    assert(all_greater_than(0, args...));
    static_assert(Data::rank - sizeof...(Args) >= 0);
  }

  template <class... Args, class = typename enable_if_all_integral<Args...>::type>
  void NewParArrayND(Args... args, const std::string &label = "ParArrayND") {
    assert(all_greater_than(0, args...));
    static_assert(Data::rank - sizeof...(Args) >= 0);
    NewParArrayND(std::make_index_sequence<Data::rank - sizeof...(Args)>{}, args...,
                  label);
  }

  template <class... Args, class = typename enable_if_all_integral<Args...>::type>
  KOKKOS_FORCEINLINE_FUNCTION auto Get(Args... args) const {
    static_assert(Data::rank - sizeof...(Args) >= 0);
    return Get(std::make_index_sequence<Data::rank - sizeof...(Args)>{}, args...);
  }

  // call me as Get<D>();
  template <std::size_t N>
  KOKKOS_INLINE_FUNCTION auto Get() const {
    return Get_TemplateVersion_impl(std::make_index_sequence<Data::rank - N>{});
  }

  template <class... Args, class = typename enable_if_all_integral<Args...>::type>
  void Resize(Args... args) {
    static_assert(Data::rank - sizeof...(Args) >= 0);
    Resize(std::make_index_sequence<Data::rank - sizeof...(Args)>{}, args...);
  }

  template <class... Args, class = typename enable_if_all_integral<Args...>::type>
  KOKKOS_FORCEINLINE_FUNCTION auto &operator()(Args... args) const {
    static_assert(Data::rank - sizeof...(Args) >= 0);
    return _operator_impl(std::make_index_sequence<Data::rank - sizeof...(Args)>{},
                          args...);
  }

  // function to get the label
  std::string label() const { return data_.label(); }

  // functions to get array dimensions
  KOKKOS_INLINE_FUNCTION int GetDim(const int i) const {
    assert(0 < i && i <= Data::rank);
    return data_.extent_int(Data::rank - i);
  }
  template <class INTEGRAL_T>
  KOKKOS_INLINE_FUNCTION auto extent(const INTEGRAL_T i) const {
    return data_.extent(i);
  }

  template <class INTEGRAL_T>
  KOKKOS_INLINE_FUNCTION auto extent_int(const INTEGRAL_T i) const {
    return data_.extent_int(i);
  }

  KOKKOS_INLINE_FUNCTION auto size() const { return data_.size(); }

  // a function to get the total size of the array
  KOKKOS_INLINE_FUNCTION int GetSize() const {
    return data_.size();
    // TODO(LFR) : Make sure there is no inconsistency here
    // return GetDim(1) * GetDim(2) * GetDim(3) * GetDim(4) * GetDim(5) * GetDim(6);
  }

  template <typename MemSpace>
  auto GetMirror(MemSpace const &memspace) {
    auto mirror = Kokkos::create_mirror_view(memspace, data_);
    return ParArrayGeneric<decltype(mirror)>(mirror);
  }
  auto GetHostMirror() { return GetMirror(Kokkos::HostSpace()); }
  auto GetDeviceMirror() { return GetMirror(Kokkos::DefaultExecutionSpace()); }

  template <typename Other>
  void DeepCopy(const Other &src) {
    Kokkos::deep_copy(data_, src.data_);
  }

  template <typename MemSpace>
  auto GetMirrorAndCopy(MemSpace const &memspace) {
    auto mirror = Kokkos::create_mirror_view_and_copy(memspace, data_);
    return ParArrayGeneric<decltype(mirror)>(mirror);
  }
  auto GetHostMirrorAndCopy() { return GetMirrorAndCopy(Kokkos::HostSpace()); }

  template <typename... Args>
  KOKKOS_INLINE_FUNCTION auto Slice(Args... args) const {
    auto v = Kokkos::subview(data_, std::forward<Args>(args)...);
    return ParArrayGeneric<decltype(v)>(v);
  }

  // AthenaArray.InitWithShallowSlice(src,dim,indx,nvar)
  // translates into auto dest = src.SliceD<dim>(std::make_pair(indx,indx+nvar))
  template <std::size_t N = Data::rank>
  auto SliceD(index_pair_t slc) const {
    static_assert(N <= Data::rank);
    static_assert(N > 0);
    return SliceD(std::make_index_sequence<Data::rank - N>{},
                  std::make_index_sequence<N - 1>{}, slc);
  }

  // AthenaArray.InitWithShallowSlice(src,dim,indx,nvar)
  // translates into auto dest = src.SliceD<dim>(indx,nvar)
  template <std::size_t N = Data::rank>
  auto SliceD(const int indx, const int nvar) {
    return SliceD<N>(std::make_pair(indx, indx + nvar));
  }

  // Reset size to 0
  // Note: Copies of this array won't be affected
  void Reset() { data_ = Data(); }
  
  KOKKOS_INLINE_FUNCTION
  bool IsAllocated() const { return data_.size() > 0; }

  KOKKOS_INLINE_FUNCTION
  bool is_allocated() const { return data_.is_allocated(); }

  // Want to be friends with all other specializations of ParArrayGeneric
  template <class T2>
  friend class ParArrayGeneric;

 private:
  // The stupid void casts below are to suppress compiler warnings about
  // an unused value. Found this trick buried deep in the gcc documentation
  template <class... Args, std::size_t... I>
  ParArrayGeneric(const std::string &label, std::index_sequence<I...>, Args... args)
      : data_(label, ((void)I, 1)..., args...) {}

  template <class... Args, std::size_t... I>
  void NewParArrayND(std::index_sequence<I...>, Args... args, const std::string &label) {
    data_ = Data(label, ((void)I, 1)..., args...);
  }

  template <class... Args, std::size_t... I>
  KOKKOS_FORCEINLINE_FUNCTION auto Get(std::index_sequence<I...>, Args... args) const {
    using view_t = decltype(Kokkos::subview(data_, args..., ((void)I, Kokkos::ALL())...));
    return ParArrayGeneric<view_t>(
        Kokkos::subview(data_, args..., ((void)I, Kokkos::ALL())...));
  }

  template <std::size_t... I>
  KOKKOS_FORCEINLINE_FUNCTION auto
  Get_TemplateVersion_impl(std::index_sequence<I...>) const {
    return Get(((void)I, 0)...);
  }

  template <class... Args, std::size_t... I>
  void Resize(std::index_sequence<I...>, Args... args) {
    Kokkos::resize(data_, ((void)I, 1)..., args...);
  }

  template <class... Args, std::size_t... I>
  KOKKOS_FORCEINLINE_FUNCTION auto &_operator_impl(std::index_sequence<I...>,
                                                   Args... args) const {
    return data_(((void)I, 0)..., args...);
  }

  template <std::size_t... I, std::size_t... J>
  auto SliceD(std::index_sequence<I...>, std::index_sequence<J...>,
              index_pair_t slc) const {
    return Slice(((void)I, std::make_pair(0, 1))..., slc, ((void)J, Kokkos::ALL())...);
  }

  Data data_;
  double sparse_theshold_;
};

} // namespace parthenon

namespace Kokkos {

template <class Space, class U>
inline auto create_mirror_view_and_copy(Space const& space, const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror_view_and_copy(space, static_cast<U>(arr));
}

template <class U>
inline auto create_mirror_view_and_copy(const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror_view_and_copy(static_cast<U>(arr));
}

template <class Space, class U>
inline auto create_mirror_view(Space const& space, const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror_view(space, static_cast<U>(arr));
}

template <class U>
inline auto create_mirror_view(const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror_view(static_cast<U>(arr));
}

template <class Space, class U>
inline auto create_mirror(Space const& space, const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror(space, static_cast<U>(arr));
}

template <class U>
inline auto create_mirror(const parthenon::ParArrayGeneric<U> &arr) {
  return Kokkos::create_mirror(static_cast<U>(arr));
}

template <class T, class U>
inline void deep_copy(const T &dest, const parthenon::ParArrayGeneric<U> &src) {
  Kokkos::deep_copy(dest, static_cast<U>(src));
}

template <class T, class U>
inline void deep_copy(const parthenon::ParArrayGeneric<T> &dest, const U &src) {
  Kokkos::deep_copy(static_cast<T>(dest), src);
}

template <class T, class U>
inline void deep_copy(const parthenon::ParArrayGeneric<T> &dest,
                      const parthenon::ParArrayGeneric<U> &src) {
  Kokkos::deep_copy(static_cast<T>(dest), static_cast<U>(src));
}

} // namespace Kokkos

#endif // PARTHENON_ARRAY_GENERIC_HPP_
