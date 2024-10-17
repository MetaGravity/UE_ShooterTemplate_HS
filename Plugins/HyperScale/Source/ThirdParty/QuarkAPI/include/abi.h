/** 
 * HyperScale Client SDK. Version 2.0
 * Copyright (C) MetaGravity. All rights reserved.
 */


#ifndef QUARK_API_H
#define QUARK_API_H

#include <stdint.h>

#if defined _WIN32 || defined __CYGWIN__
  #ifdef QUARK_EXPORTS
    #ifdef __GNUC__
      #define QUARK_PUBLIC_API __attribute__ ((dllexport))
    #else
      #define QUARK_PUBLIC_API __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define QUARK_PUBLIC_API __attribute__ ((dllimport))
    #else
      #define QUARK_PUBLIC_API __declspec(dllimport)
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define QUARK_PUBLIC_API __attribute__ ((visibility ("default")))
  #else
    #define QUARK_PUBLIC_API
  #endif
#endif


#define QUARK_MAX_SEQUENCE_LENGTH 32

#define QUARK_VALUE_TYPE_NONE 0

#define QUARK_VALUE_TYPE_BOOL 1

#define QUARK_VALUE_TYPE_VEC2 2

#define QUARK_VALUE_TYPE_VEC3 3

#define QUARK_VALUE_TYPE_UINT8 4

#define QUARK_VALUE_TYPE_UINT16 5

#define QUARK_VALUE_TYPE_UINT32 6

#define QUARK_VALUE_TYPE_UINT64 7

#define QUARK_VALUE_TYPE_INT8 8

#define QUARK_VALUE_TYPE_INT16 9

#define QUARK_VALUE_TYPE_INT32 10

#define QUARK_VALUE_TYPE_INT64 11

#define QUARK_VALUE_TYPE_FLOAT32 12

#define QUARK_VALUE_TYPE_FLOAT64 13

#define QUARK_VALUE_TYPE_STRING 14

#define QUARK_VALUE_TYPE_BYTES 15

#define QUARK_VALUE_TYPE_VEC2D 16

#define QUARK_VALUE_TYPE_VEC3D 17

#define QUARK_VALUE_TYPE_VEC4 18

#define QUARK_VALUE_TYPE_VEC4D 19

#define QUARK_MAX_PAYLOAD_LEN 256

#define MTUBytes_SIZE MTU_PAYLOAD_LEN

#define QUARK_VALUE_SEQUENCE_LENGTH (QUARK_MAX_SEQUENCE_LENGTH + 1)

#define QUARK_MAX_INTEREST_RADIUS 1.0

#define QUARK_MIN_INTEREST_RADIUS 0.0

#define QUARK_RTT_NOT_MEASURED 0

typedef uint32_t quark_error_t;

typedef const void *quark_session_t;

typedef uint8_t quark_update_type_t;

typedef uint8_t quark_entity_id_type_t;

typedef uint32_t PlayerId;

typedef PlayerId quark_player_id_t;

typedef uint64_t ObjectId;

typedef ObjectId quark_object_id_t;

typedef union quark_entity_id_value_t {
  quark_player_id_t player;
  quark_object_id_t object;
} quark_entity_id_value_t;

typedef struct quark_entity_id_t {
  quark_entity_id_type_t entity_type;
  union quark_entity_id_value_t entity_id;
} quark_entity_id_t;

typedef uint64_t Timestamp;

typedef Timestamp quark_timestamp_t;

typedef uint16_t AttributeId;

typedef AttributeId quark_attribute_id_t;

typedef uint8_t quark_value_type_t;

typedef union quark_value_payload_t {
  uint8_t none[0];
  uint8_t bool_;
  uint8_t uint8;
  uint16_t uint16;
  uint32_t uint32;
  uint64_t uint64;
  int8_t int8;
  int16_t int16;
  int32_t int32;
  int64_t int64;
  float float32;
  double float64;
  float vec2[2];
  float vec3[3];
  float vec4[4];
  double vec2d[2];
  double vec3d[3];
  double vec4d[4];
  /**
   * The first byte is the length of the string
   * The rest of the bytes are the string data
   */
  char string[QUARK_VALUE_SEQUENCE_LENGTH];
  /**
   * The first byte is the length of the bytes
   * The rest of the bytes are the bytes data
   */
  uint8_t bytes[QUARK_VALUE_SEQUENCE_LENGTH];
} quark_value_payload_t;

/**
 * This type exposes the raw C representation of a Value
 */
typedef struct quark_value_t {
  quark_value_type_t type_id;
  union quark_value_payload_t payload;
} quark_value_t;

/**
 * Holds information about updates to an attribute of a player or an object.
 */
typedef struct quark_remote_entity_update_t {
  /**
   * Entity ID of the player or object that was updated.
   */
  struct quark_entity_id_t entity;
  /**
   * Timestamp of the change as milliseconds since UNIX epoch.
   *
   * This timestamp is the time when the change happened on the
   * originator's side, not the server storage time. This value
   * can be used for dead reckoning and other time-sensitive
   * interpolation operations.
   */
  quark_timestamp_t timestamp;
  /**
   * The ID of the attribute that was updated.
   */
  quark_attribute_id_t attribute_id;
  /**
   * New value of the attribute.
   */
  struct quark_value_t value;
} quark_remote_entity_update_t;

typedef uint16_t EventClass;

typedef EventClass quark_event_class_t;

typedef struct quark_event_payload_t {
  uint16_t size;
  uint8_t data[QUARK_MAX_PAYLOAD_LEN];
} quark_event_payload_t;

/**
 * Represents an event that happened remotely that was
 * delivered to the current session.
 */
typedef struct quark_remote_event_t {
  /**
   * The class of the event.
   */
  quark_event_class_t class_id;
  /**
   * The timestamp of the event in milliseconds since UNIX epoch.
   *
   * This is the timestamp of the origination of the event on the
   * emitter machine.
   */
  quark_timestamp_t timestamp;
  /**
   * The sender of the event.
   *
   * Can be an object or a player.
   */
  struct quark_entity_id_t sender;
  /**
   * The payload of the event.
   */
  struct quark_event_payload_t payload;
} quark_remote_event_t;

typedef union quark_remote_update_value_t {
  struct quark_remote_entity_update_t entity;
  struct quark_remote_event_t event;
} quark_remote_update_value_t;

/**
 * Represents a single update that was received from the server.
 */
typedef struct quark_remote_update_t {
  /**
   * The type of the update.
   */
  quark_update_type_t update_type;
  /**
   * The update value.
   */
  union quark_remote_update_value_t value;
} quark_remote_update_t;

typedef uint32_t quark_qos_t;

/**
 * Describes an update to a property to the player controlled by the local
 * session.
 */
typedef struct quark_local_player_update_t {
  /**
   * The ID of the attribute that was updated.
   */
  quark_attribute_id_t attribute_id;
  /**
   * New value of the attribute.
   */
  struct quark_value_t value;
} quark_local_player_update_t;

/**
 * Describes an update to a property of an object.
 */
typedef struct quark_local_object_update_t {
  /**
   * The ID of the object that was updated.
   */
  quark_object_id_t object_id;
  /**
   * The ID of the attribute that was updated.
   */
  quark_attribute_id_t attribute_id;
  /**
   * New value of the attribute.
   */
  struct quark_value_t value;
} quark_local_object_update_t;

typedef uint8_t quark_recipient_type_t;

typedef float quark_relevance_radius_t;

typedef union quark_recipient_value_t {
  quark_player_id_t player;
  quark_object_id_t object;
  quark_relevance_radius_t radius;
} quark_recipient_value_t;

typedef struct quark_recipient_t {
  quark_recipient_type_t recipient_type;
  union quark_recipient_value_t recipient;
} quark_recipient_t;

typedef struct quark_local_event_t {
  /**
   * The class of the event.
   */
  quark_event_class_t class_id;
  /**
   * The recipient of the event.
   */
  struct quark_recipient_t recipient;
  /**
   * The payload of the event.
   */
  struct quark_event_payload_t payload;
} quark_local_event_t;

typedef union quark_local_update_value_t {
  /**
   * Update for a player.
   */
  struct quark_local_player_update_t player;
  struct quark_local_object_update_t object;
  struct quark_local_event_t event;
} quark_local_update_value_t;

typedef struct quark_local_update_t {
  /**
   * The type of the update.
   */
  uint8_t update_type;
  /**
   * The value of the update.
   */
  union quark_local_update_value_t value;
} quark_local_update_t;

typedef uint32_t quark_session_id_t;

typedef uint32_t quark_rtt_t;

typedef struct quark_tags_t {
  /**
   * The number of tags.
   */
  uint32_t count;
  /**
   * Array of poiners to strings containing the tags of length `count`.
   */
  const char *const *tags;
} quark_tags_t;

typedef uint64_t quark_duration_t;

typedef uint8_t quark_subscription_priority_t;

typedef uint8_t quark_event_filter_mode_t;

typedef struct quark_events_filter_t {
  /**
   * The number of classes.
   */
  uint32_t count;
  /**
   * The mode of the filter.
   * It can be either
   * - `QUARK_EVENT_FILTER_MODE_BLACKLIST` or
   * - `QUARK_EVENT_FILTER_MODE_WHITELIST`.
   */
  quark_event_filter_mode_t mode;
  /**
   * Pointer to an array of length `count` of event classes.
   */
  const quark_event_class_t *classes;
} quark_events_filter_t;

typedef struct quark_subscription_query_t {
  /**
   * The relevance radius of the query.
   */
  quark_relevance_radius_t radius;
  /**
   * The filter tags of the query, empty tags means no filter.
   */
  struct quark_tags_t tags;
  /**
   * The interval between tick of the subscription.
   */
  quark_duration_t interval;
  /**
   * The priority of the subscription.
   */
  quark_subscription_priority_t priority;
  /**
   * The events filter of the subscription.
   */
  struct quark_events_filter_t events;
  /**
   * The number of events this subscription can backlog between ticks.
   */
  uint32_t backlog;
} quark_subscription_query_t;

/**
 * A unique udentifier for a registered `Query`` subscription.
 */
typedef uint32_t SubscriptionId;

typedef SubscriptionId quark_subscription_id;

#define QUARK_UPDATE_TYPE_NONE 0

#define QUARK_UPDATE_TYPE_PLAYER 1

#define QUARK_UPDATE_TYPE_OBJECT 2

#define QUARK_UPDATE_TYPE_EVENT 3

#define QUARK_ENTITY_ID_TYPE_PLAYER 1

#define QUARK_ENTITY_ID_TYPE_OBJECT 2

/**
 * Supported by the spatial index.
 * Attribute type: Vec3
 */
#define QUARK_KNOWN_ATTRIBUTE_POSITION 1

#define QUARK_KNOWN_ATTRIBUTE_ROTATION 2

#define QUARK_KNOWN_ATTRIBUTE_GROUP_ID 5

/**
 * Attribute type: Uint64
 */
#define QUARK_KNOWN_ATTRIBUTE_CLASS_ID 6

/**
 * Attribute type: Uint64 for objects and Uint32 for players
 */
#define QUARK_KNOWN_ATTRIBUTE_OWNER_ID 7

/**
 * Attribute type: Uint64
 */
#define QUARK_KNOWN_ATTRIBUTE_DELEGATE 8

#define QUARK_KNOWN_ATTRIBUTE_HIDDEN 9

#define QUARK_ERROR_SUCCESS 0

#define QUARK_ERROR_INVALID_ARGUMENT 1

#define QUARK_ERROR_SESSION_IO 2

#define QUARK_ERROR_SESSION_REJECTED 3

#define QUARK_ERROR_SESSION_SERVER_STREAM_CLOSED 4

#define QUARK_ERROR_SESSION_PROTOCOL 5

#define QUARK_ERROR_SESSION_SERIALIZATION 6

#define QUARK_ERROR_SESSION_DESERIALIZATION 7

#define QUARK_ERROR_TOO_MANY_ATTRIBS 8

#define QUARK_ERROR_INVALID_ATTRIB_TYPE 9

#define QUARK_ERROR_BUFFER_TOO_SMALL 10

#define QUARK_ERROR_UNKNOWN 11

#define QUARK_ERROR_NO_DATA 12

#define QUARK_ERROR_BAD_ADDR_FORMAT 13

#define QUARK_ERROR_INVALID_DNS_NAME 14

#define QUARK_ERROR_CERT_LOAD_FAILED 15

#define QUARK_ERROR_CERT_ADD_FAILED 16

#define QUARK_ERROR_UDP_STREAM_NOT_AVAILABLE 17

#define QUARK_ERROR_DATA_NOT_SUPPORTED 18

#define QUARK_ERROR_INVALID_PACKET 19

#define QUARK_ERROR_INVALID_USER 20

#define QUARK_EVENT_RECIPIENT_TYPE_PLAYER 1

#define QUARK_EVENT_RECIPIENT_TYPE_OBJECT 2

#define QUARK_EVENT_RECIPIENT_TYPE_RADIUS 3

#define QUARK_QOS_RELIABLE 1

#define QUARK_QOS_UNRELIABLE 2

#define QUARK_EVENT_FILTER_MODE_BLACKLIST 1

#define QUARK_EVENT_FILTER_MODE_WHITELIST 2

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Returns a string describing the error code.
 *
 * The returned string is valid until the next call to this function.
 */
QUARK_PUBLIC_API 
const char *quark_error_string(quark_error_t error);

/**
 * Polls a connected session for the next available update.
 *
 * This is a non-blocking call. If there are no updates available
 * the function will return `QUARK_ERROR_NO_DATA`.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_try_receive(quark_session_t session,
                                        struct quark_remote_update_t *output);

/**
 * Configures the interval between outbound ticks for the session
 * for the given QoS level. The tick rate is in milliseconds.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_set_tick_interval(quark_session_t session,
                                              uint32_t tick_rate_ms,
                                              quark_qos_t qos);

/**
 * Pushes a new update to the server.
 *
 * This function will not send the value immediately instead it will schedule
 * it for sending in the next network tick. The function will return
 * immediately.
 *
 * The outbound tick rate can be controlled by the
 * `quark_session_set_tick_rate` function.
 *
 * The `qos` parameter controls the quality of service of the update. Reliable
 * updates go over TCP and UNRELIABLE updates go over UDP.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_send(quark_session_t session,
                                 quark_qos_t qos,
                                 const struct quark_local_update_t *update);

/**
 * Starts a new session with the server. This is a blocking function that
 * returns when the session is established or an error occurs.
 *
 * The server address is a string in the form of "host:port" or "ip:port" (port
 * is optional). The authentication buffer is a byte blob that is sent to the
 * server as part of the authentication process. The server will either accept
 * and assign a player id or reject the connection.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_start(const char *server_addr,
                                  const uint8_t *auth_buffer,
                                  uint8_t auth_len,
                                  quark_session_t *output);

/**
 * Resumes the connection with the server using the resume token (can be
 * obtained during the session runtime)
 *
 * The server address is a string in the form of "ip:port".
 * The resume token buffer is a byte blob that is received from the server
 * as part of the authentication process.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_resume(const char *server_addr,
                                   const uint8_t *resume_token_buf,
                                   uintptr_t resume_token_len,
                                   quark_session_t *output);

QUARK_PUBLIC_API 
quark_error_t quark_session_is_connected(quark_session_t session,
                                         bool *output);

/**
 * Returns the session id which is also the public id of the player.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_get_assigned_id(quark_session_t session,
                                            quark_session_id_t *output);

/**
 * Returns the resume token that can be used to resume a session.
 *
 * The resume token is allocated inside the SDK memeory and lives as long as
 * the session object. The user is not responsible for freeing the memory,
 * except for destroying the session object.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_get_resume_token(quark_session_t session,
                                             const uint8_t **output,
                                             uintptr_t *output_len);

/**
 * Disconnects the session from the server and frees the session object.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_close(quark_session_t *session);

/**
 * Given a local object id, return an object id derived from the current
 * player id. Only this kind of IDs can be assigned to objects spawned by
 * this player. The user is responsible for ensuring that the local object id
 * id is unique for this player.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_derive_object_id(quark_session_t session,
                                             uint32_t local_id,
                                             quark_object_id_t *output);

/**
 * Given a local object id, return an object id in the shared objects
 * namespace.
 */
QUARK_PUBLIC_API 
quark_error_t quark_derive_shared_object_id(uint32_t local_id,
                                            quark_object_id_t *output);

/**
 * Get the current timestamp of the session. This time is synchronized with
 * Quark server and may differ from OS timestamp.
 * This is not monotonically increased time, and "time jumps" may occur as a
 * side effect of the time sync operation.
 *
 * Timestamp of the change as milliseconds since UNIX epoch.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_current_timestamp(quark_session_t session,
                                              quark_timestamp_t *output);

/**
 * Get RTT of the current session. This RTT value represents
 * the round-trip time for RELIABLE/TCP channel only. The RTT value
 * is in milliseconds. The resolution of RTT is 1ms.
 * In case RTT is less than 1ms or RTT has not been calculated yet,
 * QUARK_ERROR_NO_DATA is returned.
 * Note: RTT will not be available for first 60s of the session.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_rtt(quark_session_t session,
                                quark_rtt_t *output);

/**
 * Creates a new subscription.
 *
 * The subscription is created with the given query and it instructs the server
 * to start sending updates with the given specification to the client over the
 * chosen channel. The channel can be either reliable (tcp) or unreliable
 * (udp).
 *
 * On success the subscription id is returned in the `subscription_id`
 * parameter.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_subscribe(quark_session_t session,
                                      quark_qos_t qos,
                                      const struct quark_subscription_query_t *query,
                                      quark_subscription_id *subscription_id);

/**
 * Unsubscribes from a subscription.
 *
 * Instructs the server to stop sending updates for a previously subscribed
 * query.
 */
QUARK_PUBLIC_API 
quark_error_t quark_session_unsubscribe(quark_session_t session,
                                        quark_subscription_id subscription_id);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* QUARK_API_H */
