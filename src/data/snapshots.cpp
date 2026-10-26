#include <fincore/data/snapshots.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/errors.hpp>

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <typeindex>
#include <utility>

namespace fincore {
namespace {

std::string csv_escape(const std::string& text) {
  const bool quote = text.find_first_of(",\"\n\r") != std::string::npos;
  if (!quote) {
    return text;
  }
  std::string escaped;
  escaped.reserve(text.size() + 2U);
  escaped.push_back('"');
  for (const char character : text) {
    if (character == '"') {
      escaped += "\"\"";
    } else {
      escaped.push_back(character);
    }
  }
  escaped.push_back('"');
  return escaped;
}

std::string float_field(const double value) {
  if (std::isnan(value)) {
    return {};
  }
  if (std::isinf(value)) {
    return value < 0.0 ? "-inf" : "inf";
  }
  return detail::python_float(value, "data snapshot CSV");
}

template <typename T>
std::string integral_field(const T value) {
  return std::to_string(static_cast<std::int64_t>(value));
}

std::string column_value(const dataframe& frame, const std::string& name,
                         const std::type_index type, const std::size_t row) {
  if (type == typeid(double)) {
    return float_field(frame.get_column<double>(name.c_str())[row]);
  }
  if (type == typeid(float)) {
    return float_field(
        static_cast<double>(frame.get_column<float>(name.c_str())[row]));
  }
  if (type == typeid(bool)) {
    return frame.get_column<bool>(name.c_str())[row] ? "True" : "False";
  }
  if (type == typeid(std::string)) {
    return csv_escape(frame.get_column<std::string>(name.c_str())[row]);
  }
  if (type == typeid(char)) {
    return integral_field(frame.get_column<char>(name.c_str())[row]);
  }
  if (type == typeid(signed char)) {
    return integral_field(frame.get_column<signed char>(name.c_str())[row]);
  }
  if (type == typeid(unsigned char)) {
    return integral_field(frame.get_column<unsigned char>(name.c_str())[row]);
  }
  if (type == typeid(short)) {
    return integral_field(frame.get_column<short>(name.c_str())[row]);
  }
  if (type == typeid(unsigned short)) {
    return integral_field(frame.get_column<unsigned short>(name.c_str())[row]);
  }
  if (type == typeid(int)) {
    return integral_field(frame.get_column<int>(name.c_str())[row]);
  }
  if (type == typeid(unsigned int)) {
    return integral_field(frame.get_column<unsigned int>(name.c_str())[row]);
  }
  if (type == typeid(long)) {
    return integral_field(frame.get_column<long>(name.c_str())[row]);
  }
  if (type == typeid(unsigned long)) {
    return integral_field(frame.get_column<unsigned long>(name.c_str())[row]);
  }
  if (type == typeid(long long)) {
    return integral_field(frame.get_column<long long>(name.c_str())[row]);
  }
  if (type == typeid(unsigned long long)) {
    return integral_field(
        frame.get_column<unsigned long long>(name.c_str())[row]);
  }
  throw fincore_error{
      unsupported_format_error("unsupported dataframe column type", name)};
}

}  // namespace

std::string canonical_dataframe_csv(const dataframe& frame) {
  // The supported type list is closed: a column outside it would serialize to
  // something the digest could not reproduce, so the snapshot fails closed
  // instead of quietly omitting the column.
  const auto columns = frame.get_columns_info<
      bool, char, signed char, unsigned char, short, unsigned short, int,
      unsigned int, long, unsigned long, long long, unsigned long long, float,
      double, std::string>();
  if (columns.size() != frame.shape().second) {
    throw fincore_error{
        unsupported_format_error("unsupported dataframe column type")};
  }
  const auto& index = frame.get_index();

  std::ostringstream output;
  // Header order and row order are the frame's own, not a sorted order, so two
  // frames holding the same data with different column order get different
  // digests by design.
  output << "index";
  for (const auto& column : columns) {
    output << ',' << csv_escape(std::string(std::get<0>(column).c_str()));
  }
  output << '\n';
  for (std::size_t row = 0U; row < index.size(); ++row) {
    output << index[row];
    for (const auto& column : columns) {
      output << ','
             << column_value(frame, std::get<0>(column).c_str(),
                             std::get<2>(column), row);
    }
    output << '\n';
  }
  return output.str();
}

std::string dataframe_content_sha256(const dataframe& frame) {
  const std::string payload = canonical_dataframe_csv(frame);
  return detail::sha256_hex(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()));
}

data_snapshot::data_snapshot(std::string provider, std::string requested_start,
                             std::string requested_end, std::string as_of,
                             std::string timezone, std::string price_adjustment,
                             std::string content_sha256, dataframe data)
    : provider_(std::move(provider)),
      requested_start_(std::move(requested_start)),
      requested_end_(std::move(requested_end)),
      as_of_(std::move(as_of)),
      timezone_(std::move(timezone)),
      price_adjustment_(std::move(price_adjustment)),
      content_sha256_(std::move(content_sha256)),
      data_(std::move(data)) {}

data_snapshot data_snapshot::from_frame(const dataframe& frame,
                                        std::string provider,
                                        std::string requested_start,
                                        std::string requested_end,
                                        std::string as_of, std::string timezone,
                                        std::string price_adjustment) {
  dataframe copy = frame;
  std::string digest = dataframe_content_sha256(copy);
  return data_snapshot(std::move(provider), std::move(requested_start),
                       std::move(requested_end), std::move(as_of),
                       std::move(timezone), std::move(price_adjustment),
                       std::move(digest), std::move(copy));
}

std::map<std::string, std::string> data_snapshot::identity_kwargs() const {
  return {{"provider", provider_},
          {"requested_start", requested_start_},
          {"requested_end", requested_end_},
          {"as_of", as_of_},
          {"timezone", timezone_},
          {"price_adjustment", price_adjustment_}};
}

std::map<std::string, std::string> data_snapshot::to_manifest_dict() const {
  return {{"schema_version", std::to_string(data_snapshot_schema_version)},
          {"provider", provider_},
          {"requested_start", requested_start_},
          {"requested_end", requested_end_},
          {"as_of", as_of_},
          {"timezone", timezone_},
          {"price_adjustment", price_adjustment_},
          {"content_sha256", content_sha256_}};
}

std::string data_snapshot::to_manifest() const {
  std::string output;
  detail::json_writer writer(output);
  writer.begin_object();
  writer.key("schema_version");
  writer.value_integer(data_snapshot_schema_version);
  writer.key("provider");
  writer.value_string(provider_);
  writer.key("requested_start");
  writer.value_string(requested_start_);
  writer.key("requested_end");
  writer.value_string(requested_end_);
  writer.key("as_of");
  writer.value_string(as_of_);
  writer.key("timezone");
  writer.value_string(timezone_);
  writer.key("price_adjustment");
  writer.value_string(price_adjustment_);
  writer.key("content_sha256");
  writer.value_string(content_sha256_);
  writer.end_object();
  return output;
}

}  // namespace fincore
