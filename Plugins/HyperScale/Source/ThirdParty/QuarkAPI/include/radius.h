#ifndef QUARK_RADIUS_H
#define QUARK_RADIUS_H

namespace quark
{

class QUARK_PUBLIC_API radius {
public:
  static radius max() noexcept;
  static radius some(float) noexcept;
  static radius none() noexcept;

public:
  radius() noexcept;
  radius(float value) noexcept;
  float value() const noexcept;
  operator float() const noexcept;

private:
  float _;
};

}

#endif // QUARK_RADIUS_H