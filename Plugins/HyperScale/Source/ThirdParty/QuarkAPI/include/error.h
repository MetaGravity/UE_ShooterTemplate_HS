// Copyright (C) MetaGravity - All Rights Reserved
// 
// Unauthorized copying of this file, via any medium is
// strictly prohibited. Proprietary and confidential.

#ifndef QUARK_ERROR_H
#define QUARK_ERROR_H


namespace quark
{

/**
 * @brief Represents an error code that can be returned by the Quark API.
 */
enum class QUARK_PUBLIC_API error_code : uint32_t
{
  success = 0,
  invalid_argument = 1,
  io_error = 2,
  rejected = 3,
  server_error = 4,
  protocol = 5,
  serialization = 6,
  deserialization = 7,
  too_many_attribs = 8,
  invalid_attrib_type = 9,
  buffer_too_small = 10,
  unknown = 11,
  no_data = 12,
  bad_addr_format = 13,
  invalid_dns_name = 14,
  cert_load_failed = 15,
  cert_add_failed = 16,
  udp_stream_not_available = 17,
  data_not_supported = 18,
  invalid_packet = 19,
  invalid_user = 20
};

class QUARK_PUBLIC_API error
{
public:
  error();
  error(uint32_t code);
  error(error_code code);
  error operator=(error_code code);
  error operator=(uint32_t code);
  
public:
  bool is_success() const noexcept;
  bool is_error() const noexcept;
  operator bool() const noexcept;

public:
  error_code code() const noexcept;
  const char* message() const noexcept;

public:
  bool operator==(error) const noexcept;
  bool operator!=(error) const noexcept;

  bool operator==(error_code code) const noexcept;
  bool operator!=(error_code code) const noexcept;

private:
  error_code code_;
};

}

#endif // QUARK_ERROR_H