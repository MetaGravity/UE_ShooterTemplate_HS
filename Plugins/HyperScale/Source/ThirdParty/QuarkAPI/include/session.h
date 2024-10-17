#ifndef QUARK_SESSION_H
#define QUARK_SESSION_H

#include "quark.h"

namespace quark 
{

/**
 * @brief Quality of Service (QoS) levels that can be used to control the reliability
 * of the data being sent or received.
 *
 * This type is used throughout the API to specify the transport protocol to use in that
 * context. 
 */
enum class qos : quark_qos_t
{
    /**
     * @brief Use the TCP protocol to send and receive data.
     * This is slower and guarantees order and delivery.
     */
    reliable = QUARK_QOS_RELIABLE,

    /**
     * @brief Use the UDP protocol to send and receive data.
     * This is faster but does not guarantee order or delivery.
     */
    unreliable = QUARK_QOS_UNRELIABLE,
};

template <typename T>
using expected = tl::expected<T, error>;

using token_t = quark::vector<uint8_t>;

class QUARK_PUBLIC_API session
{
public: // construction

  /**
   * @brief Initializes a new persistent session with the server.
   * 
   * @param server Address of the server. This can be either an IP address or a DNS name.
   *               The port number is optional and if not specified the default 5670 will
   *               be used. Examples of valid server addresses are:
   *               - "localhost"
   *               - "localhost:873"
   *               - "127.0.0.1"
   *               - "127.0.0.1:8891"
   *               - "someserver.com"
   *               - "someserver.com:1234"
   *               - "2345:0425:2CA1:0:0:0567:5673:23b5"
   * @param authtoken An authentication token obtained from an identity management system.
   *                  If working against a development server then this can be omitted and 
   *                  the server will create a new ephemeral session with random ID.
   * @return session 
   */
  static expected<session> start(
    quark::string server, 
    token_t authtoken = token_t());

  /**
   * @brief Resumes a previously established session.
   *
   * Resuming a session is possible for a brief period of time after the session has been
   * closed. The server will keep the session alive for a short period of time and a resume
   * token can be used to restore it and all its state. The resume token is provided by the
   * server when the session is started.
   * 
   * @param server Address of the server. This can be either an IP address or a DNS name.
   * @param resume Resume token. A 32-byte token that was assigned on session start.
   * @return session 
   */
  static expected<session> resume(
    quark::string server,
    token_t resume);

public: // status
  /**
   * @brief Gets the server-assigned ID of the current session.
   * 
   * @return session_id_t 
   */
  quark_session_id_t id() const noexcept;

  /**
   * @brief Gets the server-assigned resume token that can be used to restore a dropped
  *  session for a brief period of time after its closed.
   * 
   * @return token_t 
   */
  token_t resume_token() const noexcept;

public: // subscriptions
  expected<subscription_id_t> subscribe(query, qos);
  expected<void> unsubscribe(subscription_id_t id);

public: // Streams API -- send
  expected<void> send(local_update, qos = qos::reliable);
  expected<void> set_tick_interval(
    std::chrono::milliseconds, 
    qos = qos::reliable);
  
public: // Streams API -- receive
  expected<remote_update> receive(); // blocking 
  std::optional<remote_update> try_receive(); // non-blocking

public: // diagnostics
  bool is_connected() const noexcept;

public: // current timestamp
  quark_timestamp_t current_timestamp() const noexcept;

public: // object ids
  quark_object_id_t derive_object_id(uint32_t) noexcept;

public: // move only
  session(session&&) noexcept;
  session& operator=(session&&) noexcept;

public: // no copies
  session(session const&) = delete;
  session& operator=(session const&) = delete;

public: // destruction
  ~session();

private:
  session();

private:
  const void* impl_;
};

}

std::ostream& operator<<(
  std::ostream& os, 
  quark::token_t const& token);

#endif // QUARK_SESSION_H
