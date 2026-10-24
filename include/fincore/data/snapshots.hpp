#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {

inline constexpr int data_snapshot_schema_version = 1;

// Immutable, content-addressed snapshot of fetched market data. The content
// digest is computed over a canonical native CSV (UTC-nanosecond index plus
// columns in insertion order); it is a deterministic native provenance digest,
// not a claim that Python/pandas CSV bytes are identical.
class data_snapshot {
 public:
  data_snapshot() = default;
  data_snapshot(std::string provider, std::string requested_start,
                std::string requested_end, std::string as_of,
                std::string timezone, std::string price_adjustment,
                std::string content_sha256, dataframe data);

  static data_snapshot from_frame(const dataframe& frame,
                                  std::string provider,
                                  std::string requested_start,
                                  std::string requested_end, std::string as_of,
                                  std::string timezone = "UTC",
                                  std::string price_adjustment = "raw");

  // Defensive copy, matching the source property contract.
  dataframe data() const { return data_; }
  const dataframe& stored_data() const noexcept { return data_; }

  std::map<std::string, std::string> identity_kwargs() const;
  std::string to_manifest() const;
  std::map<std::string, std::string> to_manifest_dict() const;

  const std::string& provider() const noexcept { return provider_; }
  const std::string& requested_start() const noexcept {
    return requested_start_;
  }
  const std::string& requested_end() const noexcept { return requested_end_; }
  const std::string& as_of() const noexcept { return as_of_; }
  const std::string& timezone() const noexcept { return timezone_; }
  const std::string& price_adjustment() const noexcept {
    return price_adjustment_;
  }
  const std::string& content_sha256() const noexcept {
    return content_sha256_;
  }
  int schema_version() const noexcept { return data_snapshot_schema_version; }

 private:
  std::string provider_;
  std::string requested_start_;
  std::string requested_end_;
  std::string as_of_;
  std::string timezone_ = "UTC";
  std::string price_adjustment_ = "raw";
  std::string content_sha256_;
  dataframe data_;
};

std::string canonical_dataframe_csv(const dataframe& frame);
std::string dataframe_content_sha256(const dataframe& frame);

}  // namespace fincore
