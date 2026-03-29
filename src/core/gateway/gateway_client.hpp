#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace kind {

class GatewayClient {
public:
  virtual ~GatewayClient() = default;

  // event_name is the "t" field, data_json is the "d" field as raw JSON string
  using EventCallback = std::function<void(std::string_view event_name, const std::string& data_json)>;

  virtual void connect(std::string_view url, std::string_view token) = 0;
  virtual void disconnect() = 0;
  virtual void send(const std::string& payload_json) = 0;
  virtual void set_event_callback(EventCallback cb) = 0;
  virtual bool is_connected() const = 0;

  // Intent flags for IDENTIFY (bot tokens only)
  virtual void set_intents(uint32_t intents) = 0;

  // Set whether this is a bot token (affects IDENTIFY payload)
  virtual void set_bot_mode(bool is_bot) = 0;
};

struct GatewayConfig {
  int base_reconnect_delay_ms = 1000;
  int max_reconnect_delay_ms = 30000;
  int max_retries = 10;
};

} // namespace kind
