#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {

/// Schema version written into data snapshot manifests.
inline constexpr int data_snapshot_schema_version = 1;

/// Immutable, content-addressed snapshot of fetched market data.
///
/// The content digest is computed over a canonical native CSV (UTC-nanosecond
/// index plus columns in insertion order); it is a deterministic native
/// provenance digest, not a claim that Python/pandas CSV bytes are identical.
class data_snapshot {
 public:
  /// Creates an empty snapshot.
  ///
  /// The timezone defaults to `"UTC"` and the price adjustment to `"raw"`; the
  /// remaining metadata and the frame are empty.
  data_snapshot() = default;

  /// Creates a snapshot from explicit metadata and data.
  ///
  /// Values are stored as supplied; no digest is computed and no consistency
  /// check is performed.
  ///
  /// @param provider Provider name.
  /// @param requested_start Requested start date.
  /// @param requested_end Requested end date.
  /// @param as_of Time at which the data was captured.
  /// @param timezone Timezone label.
  /// @param price_adjustment Price adjustment label.
  /// @param content_sha256 Content digest text.
  /// @param data Snapshot frame.
  data_snapshot(std::string provider, std::string requested_start,
                std::string requested_end, std::string as_of,
                std::string timezone, std::string price_adjustment,
                std::string content_sha256, dataframe data);

  /// Creates a snapshot and computes its content digest from `frame`.
  ///
  /// A defensive copy of `frame` is stored, and the digest is
  /// dataframe_content_sha256() of that copy.
  ///
  /// @param frame Frame to snapshot.
  /// @param provider Provider name.
  /// @param requested_start Requested start date.
  /// @param requested_end Requested end date.
  /// @param as_of Time at which the data was captured.
  /// @param timezone Timezone label; defaults to `"UTC"`.
  /// @param price_adjustment Price adjustment label; defaults to `"raw"`.
  /// @return Snapshot with the computed digest and a stored copy of `frame`.
  /// @throws fincore_error With category `UnsupportedFormatError` when the
  ///     frame cannot be encoded as canonical CSV.
  static data_snapshot from_frame(const dataframe& frame,
                                  std::string provider,
                                  std::string requested_start,
                                  std::string requested_end, std::string as_of,
                                  std::string timezone = "UTC",
                                  std::string price_adjustment = "raw");

  /// Returns a defensive copy of the snapshot frame.
  ///
  /// @return Copy of the stored frame; later changes to it do not affect the
  ///     snapshot.
  dataframe data() const { return data_; }

  /// Returns a reference to the stored frame without copying.
  ///
  /// @return Const reference to the snapshot-owned frame.
  const dataframe& stored_data() const noexcept { return data_; }

  /// Returns the identity metadata of the snapshot.
  ///
  /// The map holds `provider`, `requested_start`, `requested_end`, `as_of`,
  /// `timezone` and `price_adjustment`; the digest and data are excluded.
  ///
  /// @return Map of identity metadata.
  std::map<std::string, std::string> identity_kwargs() const;

  /// Returns a JSON manifest describing the snapshot.
  ///
  /// The object contains `schema_version`, `provider`, `requested_start`,
  /// `requested_end`, `as_of`, `timezone`, `price_adjustment` and
  /// `content_sha256`; frame data is not included.
  ///
  /// @return JSON manifest text.
  std::string to_manifest() const;

  /// Returns the manifest as a flat string map.
  ///
  /// The same fields as to_manifest() are returned, with `schema_version`
  /// rendered as a decimal string.
  ///
  /// @return Map of manifest fields.
  std::map<std::string, std::string> to_manifest_dict() const;

  /// Returns the provider name.
  const std::string& provider() const noexcept { return provider_; }
  /// Returns the requested start date text.
  const std::string& requested_start() const noexcept {
    return requested_start_;
  }
  /// Returns the requested end date text.
  const std::string& requested_end() const noexcept { return requested_end_; }
  /// Returns the capture time text.
  const std::string& as_of() const noexcept { return as_of_; }
  /// Returns the timezone label.
  const std::string& timezone() const noexcept { return timezone_; }
  /// Returns the price adjustment label.
  const std::string& price_adjustment() const noexcept {
    return price_adjustment_;
  }
  /// Returns the content digest text.
  const std::string& content_sha256() const noexcept {
    return content_sha256_;
  }
  /// Returns the data snapshot schema version.
  ///
  /// @return data_snapshot_schema_version.
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

/// Returns a canonical CSV rendering of `frame`.
///
/// The first line is `index` followed by the column names in insertion order;
/// each subsequent line holds the UTC-nanosecond index value and the column
/// values. Doubles use Python-compatible float formatting, except that NaN is
/// an empty field and infinities are `inf` or `-inf`; booleans are `True` or
/// `False`; strings are quoted only when needed, with embedded quotes doubled.
/// Integral columns are rendered as decimal integers. The output ends with a
/// newline.
///
/// @param frame Frame to render.
/// @return CSV text.
/// @throws fincore_error With category `UnsupportedFormatError` when the frame
///     cannot be encoded as canonical CSV.
std::string canonical_dataframe_csv(const dataframe& frame);

/// Returns the lowercase hexadecimal SHA-256 of canonical_dataframe_csv().
///
/// @param frame Frame to digest.
/// @return 64-character lowercase hexadecimal digest.
/// @throws fincore_error With category `UnsupportedFormatError` when the frame
///     cannot be encoded as canonical CSV.
std::string dataframe_content_sha256(const dataframe& frame);

}  // namespace fincore
