#ifndef QUARK_VALUE_H
#define QUARK_VALUE_H

namespace quark 
{

struct QUARK_PUBLIC_API vec2
{
  float x;
  float y;
};

struct QUARK_PUBLIC_API vec3
{
  float x;
  float y;
  float z;
};

struct QUARK_PUBLIC_API vec4
{
  float x;
  float y;
  float z;
  float w;
};

struct QUARK_PUBLIC_API vec2d
{
  double x;
  double y;
};

struct QUARK_PUBLIC_API vec3d
{
  double x;
  double y;
  double z;
};

struct QUARK_PUBLIC_API vec4d
{
  double x;
  double y;
  double z;
  double w;
};

struct none_t {};

enum class QUARK_PUBLIC_API value_type : quark_value_type_t
{
  none = QUARK_VALUE_TYPE_NONE,

  uint8 = QUARK_VALUE_TYPE_UINT8,
  uint16 = QUARK_VALUE_TYPE_UINT16,
  uint32 = QUARK_VALUE_TYPE_UINT32,
  uint64 = QUARK_VALUE_TYPE_UINT64,

  int8 = QUARK_VALUE_TYPE_INT8,
  int16 = QUARK_VALUE_TYPE_INT16,
  int32 = QUARK_VALUE_TYPE_INT32,
  int64 = QUARK_VALUE_TYPE_INT64,

  float32 = QUARK_VALUE_TYPE_FLOAT32,
  float64 = QUARK_VALUE_TYPE_FLOAT64,

  bool_ = QUARK_VALUE_TYPE_BOOL,

  vec2 = QUARK_VALUE_TYPE_VEC2,
  vec3 = QUARK_VALUE_TYPE_VEC3,
  vec4 = QUARK_VALUE_TYPE_VEC4,

  vec2d = QUARK_VALUE_TYPE_VEC2D,
  vec3d = QUARK_VALUE_TYPE_VEC3D,
  vec4d = QUARK_VALUE_TYPE_VEC4D,

  string = QUARK_VALUE_TYPE_STRING,
  bytes = QUARK_VALUE_TYPE_BYTES
};

class QUARK_PUBLIC_API value
{ 
public:
  template<typename T>
  value(T const&) noexcept;

  value(const char*) noexcept;
  value(const char*, size_t) noexcept;
  value(const uint8_t*, size_t) noexcept;

public: // accessors
  value_type type() const noexcept;

  template <typename T>
  std::optional<T> as() const noexcept;

public: // debug
  quark::string to_string() const noexcept;

private:
  value(quark_value_t) noexcept;

private:
  quark_value_t _;

friend class local_update;
friend class remote_player_update;
friend class remote_object_update;
};

}

#endif // QUARK_VALUE_H