#ifndef QUARK_MEMORY_H
#define QUARK_MEMORY_H

namespace quark
{

void set_alloc(void* (*allocate)(std::size_t));
void set_dealloc(void (*deallocate)(void*)) noexcept;

template <typename T>
class QUARK_PUBLIC_API allocator {
public:
  using value_type = T;

public:
  allocator() noexcept = default;
  ~allocator() noexcept = default;
  template <typename U> allocator(const allocator<U>&) noexcept {};

public:
  [[nodiscard]] T* allocate(std::size_t n);
  void deallocate(T* p, std::size_t n) noexcept;
};

template<typename T, typename U>
constexpr bool operator== (allocator<T> const&, allocator<U> const&) noexcept {
  return true;
}

template<typename T, typename U>
constexpr bool operator!= (allocator<T> const&, allocator<U> const&) noexcept {
  return false;
}

#define MAKE_ALLOC(type) \
  template <> \
  [[nodiscard]] type* quark::allocator<type>::allocate(std::size_t n); \
  template <> \
  void quark::allocator<type>::deallocate(type* p, std::size_t) noexcept; \
  


MAKE_ALLOC(char);
MAKE_ALLOC(int8_t);
MAKE_ALLOC(int16_t);
MAKE_ALLOC(int32_t);
MAKE_ALLOC(int64_t);

MAKE_ALLOC(uint8_t);
MAKE_ALLOC(uint16_t);
MAKE_ALLOC(uint32_t);
MAKE_ALLOC(uint64_t);

MAKE_ALLOC(const char*);

#undef MAKE_ALLOC


using string = std::basic_string<
  char, std::char_traits<char>, allocator<char>>;

template<typename T>
using vector = std::vector<T, allocator<T>>;

}

#endif // QUARK_MEMORY_H
