#ifndef QUARK_ATTRIB_H
#define QUARK_ATTRIB_H

namespace quark
{

enum known_attrib : quark_attribute_id_t
{
  position = QUARK_KNOWN_ATTRIBUTE_POSITION,
  class_id = QUARK_KNOWN_ATTRIBUTE_CLASS_ID,
  delegate = QUARK_KNOWN_ATTRIBUTE_DELEGATE,
  owner = QUARK_KNOWN_ATTRIBUTE_OWNER_ID,
  group = QUARK_KNOWN_ATTRIBUTE_GROUP_ID
};

enum known_event : quark_event_class_t
{
  object_despawned = 1,
  forget_objects = 2,
  forget_players = 3,
  player_disconnected = 4
};

}

#endif // QUARK_ATTRIB_H