#ifndef QUARK_ENTITY_H
#define QUARK_ENTITY_H

namespace quark
{

enum class QUARK_PUBLIC_API entity_type : quark_entity_id_type_t
{
  player = QUARK_ENTITY_ID_TYPE_PLAYER,
  object = QUARK_ENTITY_ID_TYPE_OBJECT,
};

class QUARK_PUBLIC_API entity_id
{
public:
  static entity_id player(quark_player_id_t) noexcept;
  static entity_id object(quark_object_id_t) noexcept;

public:
  bool is_player() const noexcept;
  bool is_object() const noexcept;
  entity_type type() const noexcept;
  
  std::optional<quark_player_id_t> player_id() const noexcept;
  std::optional<quark_object_id_t> object_id() const noexcept;

public: // debug
  quark::string to_string() const;

private:
  entity_id(quark_entity_id_t) noexcept;

private:
  quark_entity_id_t _;

friend class remote_event;
};

}

#endif // QUARK_ENTITY_H