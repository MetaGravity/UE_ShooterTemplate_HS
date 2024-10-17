// Copyright (C) MetaGravity - All Rights Reserved
// 
// Unauthorized copying of this file, via any medium is
// strictly prohibited. Proprietary and confidential.

#ifndef QUARK_QUERY_H
#define QUARK_QUERY_H

namespace quark
{

using subscription_id_t = uint32_t;

class QUARK_PUBLIC_API priority
{
public:
  static priority low() noexcept;
  static priority normal() noexcept;
  static priority high() noexcept;

private:
  priority() noexcept;

private:
  uint8_t _;
friend class query;
};

class QUARK_PUBLIC_API events_filter
{
public:
  static events_filter whitelist(quark::vector<quark_event_class_t>) noexcept;
  static events_filter blacklist(quark::vector<quark_event_class_t>) noexcept;

public:
  bool is_whitelist;
  quark::vector<quark_event_class_t> events;
};

/**
 * @brief This struct represents a query that is used to subscribe to updates 
 * from the server.
 *
 * Periodically the server will send updates to the client that match the
 * query.
 */
class QUARK_PUBLIC_API query
{
public: // initialize builder pattern
  query() noexcept;

public:
  /**
    * @brief The relevance radius of the query relative to 
    * the current centroid of interest.
    */
  query with_radius(class radius) noexcept;

  /**
    * @brief Tags that the relevant objects, players and their attributes must
    * have to match the query.
    *
    * If this field is empty, the query will return all objects and players and
    * their attributes that are within the specified radius.
    */
  query with_tags(quark::vector<quark::string>) noexcept;

  /**
    * @brief The interval at which the server sends updates to the client
    * 
    */
  query with_interval(std::chrono::milliseconds) noexcept;

  /**
    * @brief The events filter that decides which events the subscription is
    * interested in. Only events that match this filter will be delivered to 
    * the client subscribed to this query.
    *
    * If more than subscription is interested in the same event, the server will
    * send only one copy of the event to the client.
    */
  query with_filter(events_filter) noexcept;
  
  /**
    * @brief Priority of the query
    *
    * This value is considered when the server has to decide which queries to
    * send updates to first when throttling bandwidth usage for a client.
    */
  query with_priority(class priority) noexcept;

  /**
    * @brief The maximum number of events that the query will queue up between updates.
    */
  query with_backlog(size_t) noexcept;

private:
  quark_subscription_query_t _;
  quark::vector<quark::string> tags_;
  quark::vector<quark_event_class_t> events_;

friend class session;
};

} // namespace quark

#endif // QUARK_QUERY_H