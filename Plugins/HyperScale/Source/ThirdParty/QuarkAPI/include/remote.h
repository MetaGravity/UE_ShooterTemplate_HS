#ifndef QUARK_REMOTE_H
#define QUARK_REMOTE_H

#include "abi.h"
namespace quark
{

enum class update_type : quark_update_type_t
{
  player = QUARK_UPDATE_TYPE_PLAYER,
  object = QUARK_UPDATE_TYPE_OBJECT,
  event = QUARK_UPDATE_TYPE_EVENT
};

class QUARK_PUBLIC_API remote_player_update
{
public:
  quark_player_id_t player_id() const noexcept;
  quark_attribute_id_t attribute_id() const noexcept;
  quark_timestamp_t timestamp() const noexcept;
  class value value() const noexcept;

private:
  remote_player_update(quark_remote_entity_update_t const*) noexcept;

private:
  quark_remote_entity_update_t const* _;

friend class remote_update;
};

class QUARK_PUBLIC_API remote_object_update
{
public:
  quark_object_id_t object_id() const noexcept;
  quark_attribute_id_t attribute_id() const noexcept;
  quark_timestamp_t timestamp() const noexcept;
  class value value() const noexcept;

private:
  remote_object_update(quark_remote_entity_update_t const*) noexcept;

private:
  quark_remote_entity_update_t const* _;

friend class remote_update;
};


class QUARK_PUBLIC_API remote_event
{
public:
  quark_event_class_t event_class() const noexcept;
  entity_id sender() const noexcept;
  quark_timestamp_t timestamp() const noexcept;
  
  uint8_t const* data() const noexcept;
  size_t size() const noexcept;

  uint8_t const* begin() const noexcept;
  uint8_t const* end() const noexcept;

private:
  remote_event(quark_remote_event_t const*) noexcept;

private:
  quark_remote_event_t const* _;

friend class remote_update;
};

class QUARK_PUBLIC_API remote_update
{
public:
  update_type type() const noexcept;
  bool is_player() const noexcept;
  bool is_object() const noexcept;
  bool is_event() const noexcept;

public:
  std::optional<remote_event> event() const noexcept;
  std::optional<remote_player_update> player() const noexcept;
  std::optional<remote_object_update> object() const noexcept;

private:
  remote_update(quark_remote_update_t) noexcept;

private:
  quark_remote_update_t _;

friend class session;
};

}

#endif // QUARK_REMOTE_H