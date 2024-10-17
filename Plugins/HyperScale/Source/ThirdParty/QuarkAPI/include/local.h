#ifndef QUARK_LOCAL_H
#define QUARK_LOCAL_H

namespace quark
{

class QUARK_PUBLIC_API recipient
{
public:
  static recipient player(quark_player_id_t) noexcept;
  static recipient object(quark_object_id_t) noexcept;
  static recipient radius(radius) noexcept;

private:
  recipient();

private:
  quark_recipient_t _;

friend class local_event;
};

class QUARK_PUBLIC_API local_event
{
public:
  local_event(quark_event_class_t, recipient, quark::vector<uint8_t>) noexcept;
  local_event(quark_event_class_t, recipient, uint8_t const*, size_t) noexcept;

private:
  quark_local_event_t _;

friend class local_update;
};


class QUARK_PUBLIC_API local_update
{
public:
  static local_update player(quark_attribute_id_t, value) noexcept;
  static local_update object(quark_object_id_t, quark_attribute_id_t, value) noexcept;
  static local_update event(local_event) noexcept;
private:
  quark_local_update_t _;

friend class session;
};

}

#endif // QUARK_LOCAL_H