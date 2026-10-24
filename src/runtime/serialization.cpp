#include <fincore/runtime/serialization.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace fincore {
namespace {

struct json_number {
  std::string text;
};

struct json_value {
  using array = std::vector<json_value>;
  using object = std::vector<std::pair<std::string, json_value>>;
  using storage = std::variant<std::nullptr_t, bool, json_number, std::string,
                               array, object>;
  storage data;
};

[[noreturn]] void invalid_json(const std::string_view reason) {
  throw input_error{"invalid runtime result JSON: " + std::string(reason)};
}

json_value null_value() { return json_value{nullptr}; }
json_value bool_value(const bool value) { return json_value{value}; }
json_value string_value(std::string value) {
  return json_value{std::move(value)};
}
json_value number_value(std::string value) {
  return json_value{json_number{std::move(value)}};
}
json_value array_value(json_value::array value = {}) {
  return json_value{std::move(value)};
}
json_value object_value(json_value::object value = {}) {
  return json_value{std::move(value)};
}

json_value
object_of(std::initializer_list<std::pair<std::string, json_value>> fields) {
  return object_value(json_value::object(fields));
}

void validate_utf8(const std::string_view value) {
  for (std::size_t offset = 0U; offset < value.size();) {
    const auto lead = static_cast<unsigned char>(value[offset]);
    if (lead < 0x80U) {
      ++offset;
      continue;
    }
    std::size_t length = 0U;
    std::uint32_t code_point = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
      length = 2U;
      code_point = lead & 0x1FU;
      if (code_point < 2U) {
        throw input_error{"runtime result text must be valid UTF-8"};
      }
    } else if ((lead & 0xF0U) == 0xE0U) {
      length = 3U;
      code_point = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
      length = 4U;
      code_point = lead & 0x07U;
      if (code_point > 4U) {
        throw input_error{"runtime result text must be valid UTF-8"};
      }
    } else {
      throw input_error{"runtime result text must be valid UTF-8"};
    }
    if (offset + length > value.size()) {
      throw input_error{"runtime result text must be valid UTF-8"};
    }
    for (std::size_t index = 1U; index < length; ++index) {
      const auto continuation =
          static_cast<unsigned char>(value[offset + index]);
      if ((continuation & 0xC0U) != 0x80U) {
        throw input_error{"runtime result text must be valid UTF-8"};
      }
      code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    if ((length == 3U && code_point < 0x800U) ||
        (length == 4U && code_point < 0x10000U) ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
        code_point > 0x10FFFFU) {
      throw input_error{"runtime result text must be valid UTF-8"};
    }
    offset += length;
  }
}

void append_utf8(std::string &output, const std::uint32_t code_point) {
  if (code_point <= 0x7FU) {
    output.push_back(static_cast<char>(code_point));
  } else if (code_point <= 0x7FFU) {
    output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
    output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
  } else if (code_point <= 0xFFFFU) {
    output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
    output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
  } else if (code_point <= 0x10FFFFU) {
    output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
    output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
  } else {
    invalid_json("Unicode escape is out of range");
  }
}

class json_parser {
public:
  explicit json_parser(const std::string_view input) : input_(input) {}

  json_value parse() {
    skip_space();
    auto value = parse_value(0U);
    skip_space();
    if (position_ != input_.size()) {
      invalid_json("trailing characters");
    }
    return value;
  }

private:
  static constexpr std::size_t maximum_depth = 128U;

  void skip_space() {
    while (position_ < input_.size()) {
      const char character = input_[position_];
      if (character != ' ' && character != '\t' && character != '\n' &&
          character != '\r') {
        break;
      }
      ++position_;
    }
  }

  bool consume(const char character) {
    if (position_ < input_.size() && input_[position_] == character) {
      ++position_;
      return true;
    }
    return false;
  }

  void require(const char character) {
    if (!consume(character)) {
      invalid_json("unexpected token");
    }
  }

  json_value parse_value(const std::size_t depth) {
    if (depth > maximum_depth || position_ >= input_.size()) {
      invalid_json("unexpected end or excessive nesting");
    }
    switch (input_[position_]) {
    case 'n':
      parse_literal("null");
      return null_value();
    case 't':
      parse_literal("true");
      return bool_value(true);
    case 'f':
      parse_literal("false");
      return bool_value(false);
    case '"':
      return string_value(parse_string());
    case '[':
      return parse_array(depth + 1U);
    case '{':
      return parse_object(depth + 1U);
    default:
      if (input_[position_] == '-' ||
          (input_[position_] >= '0' && input_[position_] <= '9')) {
        return parse_number();
      }
      invalid_json("unexpected token");
    }
  }

  void parse_literal(const std::string_view literal) {
    if (input_.substr(position_, literal.size()) != literal) {
      invalid_json("invalid literal");
    }
    position_ += literal.size();
  }

  std::uint16_t parse_hex_quad() {
    if (input_.size() - position_ < 4U) {
      invalid_json("incomplete Unicode escape");
    }
    std::uint16_t result = 0U;
    for (std::size_t index = 0U; index < 4U; ++index) {
      const char character = input_[position_++];
      unsigned digit = 0U;
      if (character >= '0' && character <= '9') {
        digit = static_cast<unsigned>(character - '0');
      } else if (character >= 'a' && character <= 'f') {
        digit = static_cast<unsigned>(character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        digit = static_cast<unsigned>(character - 'A' + 10);
      } else {
        invalid_json("invalid Unicode escape");
      }
      result = static_cast<std::uint16_t>((result << 4U) | digit);
    }
    return result;
  }

  std::string parse_string() {
    require('"');
    std::string result;
    while (position_ < input_.size()) {
      const auto character = static_cast<unsigned char>(input_[position_++]);
      if (character == '"') {
        validate_utf8(result);
        return result;
      }
      if (character < 0x20U) {
        invalid_json("unescaped control character");
      }
      if (character != '\\') {
        result.push_back(static_cast<char>(character));
        continue;
      }
      if (position_ >= input_.size()) {
        invalid_json("incomplete escape");
      }
      const char escaped = input_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        result.push_back(escaped);
        break;
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      case 'u': {
        const std::uint16_t first = parse_hex_quad();
        std::uint32_t code_point = first;
        if (first >= 0xD800U && first <= 0xDBFFU) {
          if (position_ + 2U > input_.size() || input_[position_] != '\\' ||
              input_[position_ + 1U] != 'u') {
            invalid_json("unpaired high surrogate");
          }
          position_ += 2U;
          const std::uint16_t second = parse_hex_quad();
          if (second < 0xDC00U || second > 0xDFFFU) {
            invalid_json("invalid surrogate pair");
          }
          code_point = 0x10000U +
                       ((static_cast<std::uint32_t>(first) - 0xD800U) << 10U) +
                       (static_cast<std::uint32_t>(second) - 0xDC00U);
        } else if (first >= 0xDC00U && first <= 0xDFFFU) {
          invalid_json("unpaired low surrogate");
        }
        append_utf8(result, code_point);
        break;
      }
      default:
        invalid_json("unknown escape");
      }
    }
    invalid_json("unterminated string");
  }

  json_value parse_number() {
    const std::size_t start = position_;
    consume('-');
    if (consume('0')) {
      if (position_ < input_.size() && input_[position_] >= '0' &&
          input_[position_] <= '9') {
        invalid_json("leading zero in number");
      }
    } else {
      parse_digits(true);
    }
    if (consume('.')) {
      parse_digits(true);
    }
    if (consume('e') || consume('E')) {
      if (!consume('+')) {
        consume('-');
      }
      parse_digits(true);
    }
    return number_value(std::string(input_.substr(start, position_ - start)));
  }

  void parse_digits(const bool required) {
    const std::size_t start = position_;
    while (position_ < input_.size() && input_[position_] >= '0' &&
           input_[position_] <= '9') {
      ++position_;
    }
    if (required && position_ == start) {
      invalid_json("expected digits");
    }
  }

  json_value parse_array(const std::size_t depth) {
    require('[');
    skip_space();
    json_value::array result;
    if (consume(']')) {
      return array_value(std::move(result));
    }
    while (true) {
      skip_space();
      result.push_back(parse_value(depth));
      skip_space();
      if (consume(']')) {
        break;
      }
      require(',');
    }
    return array_value(std::move(result));
  }

  json_value parse_object(const std::size_t depth) {
    require('{');
    skip_space();
    json_value::object result;
    std::set<std::string> keys;
    if (consume('}')) {
      return object_value(std::move(result));
    }
    while (true) {
      skip_space();
      if (position_ >= input_.size() || input_[position_] != '"') {
        invalid_json("object key must be a string");
      }
      auto key = parse_string();
      if (!keys.insert(key).second) {
        invalid_json("duplicate object key");
      }
      skip_space();
      require(':');
      skip_space();
      result.emplace_back(std::move(key), parse_value(depth));
      skip_space();
      if (consume('}')) {
        break;
      }
      require(',');
    }
    return object_value(std::move(result));
  }

  std::string_view input_;
  std::size_t position_ = 0U;
};

void append_json(std::string &output, const json_value &value) {
  if (std::holds_alternative<std::nullptr_t>(value.data)) {
    output += "null";
  } else if (const auto *boolean = std::get_if<bool>(&value.data)) {
    output += *boolean ? "true" : "false";
  } else if (const auto *number = std::get_if<json_number>(&value.data)) {
    output += number->text;
  } else if (const auto *string = std::get_if<std::string>(&value.data)) {
    validate_utf8(*string);
    detail::append_json_string(output, *string);
  } else if (const auto *array = std::get_if<json_value::array>(&value.data)) {
    output.push_back('[');
    bool first = true;
    for (const auto &item : *array) {
      if (!first) {
        output.push_back(',');
      }
      first = false;
      append_json(output, item);
    }
    output.push_back(']');
  } else {
    const auto &object = std::get<json_value::object>(value.data);
    output.push_back('{');
    bool first = true;
    for (const auto &[key, item] : object) {
      if (!first) {
        output.push_back(',');
      }
      first = false;
      validate_utf8(key);
      detail::append_json_string(output, key);
      output.push_back(':');
      append_json(output, item);
    }
    output.push_back('}');
  }
}

std::string json_string(const json_value &value) {
  const auto *result = std::get_if<std::string>(&value.data);
  if (result == nullptr) {
    invalid_json("expected string");
  }
  return *result;
}

const json_value::object &json_object(const json_value &value) {
  const auto *result = std::get_if<json_value::object>(&value.data);
  if (result == nullptr) {
    invalid_json("expected object");
  }
  return *result;
}

const json_value::array &json_array(const json_value &value) {
  const auto *result = std::get_if<json_value::array>(&value.data);
  if (result == nullptr) {
    invalid_json("expected array");
  }
  return *result;
}

const json_value &member(const json_value &value, const std::string_view key) {
  const auto &object = json_object(value);
  for (const auto &[name, item] : object) {
    if (name == key) {
      return item;
    }
  }
  invalid_json("missing object field");
}

void exact_keys(const json_value &value,
                const std::initializer_list<std::string_view> expected) {
  const auto &object = json_object(value);
  if (object.size() != expected.size()) {
    invalid_json("unexpected or missing object field");
  }
  for (const auto key : expected) {
    static_cast<void>(member(value, key));
  }
}

bool json_bool(const json_value &value) {
  const auto *result = std::get_if<bool>(&value.data);
  if (result == nullptr) {
    invalid_json("expected boolean");
  }
  return *result;
}

std::string number_text(const json_value &value) {
  const auto *result = std::get_if<json_number>(&value.data);
  if (result == nullptr) {
    invalid_json("expected JSON number");
  }
  return result->text;
}

std::int64_t signed_integer(const json_value &value) {
  const auto text = json_string(value);
  std::int64_t result = 0;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    invalid_json("signed integer is out of range or malformed");
  }
  return result;
}

std::uint64_t unsigned_integer(const json_value &value) {
  const auto text = json_string(value);
  std::uint64_t result = 0U;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    invalid_json("unsigned integer is out of range or malformed");
  }
  return result;
}

int small_integer(const json_value &value) {
  const auto text = number_text(value);
  int result = 0;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    invalid_json("schema integer is out of range or malformed");
  }
  return result;
}

std::string hex_bits(const std::uint64_t bits, const std::size_t digits) {
  static constexpr char alphabet[] = "0123456789abcdef";
  std::string result(digits, '0');
  for (std::size_t offset = 0U; offset < digits; ++offset) {
    const auto shift = static_cast<unsigned>((digits - offset - 1U) * 4U);
    result[offset] = alphabet[(bits >> shift) & 0xFU];
  }
  return result;
}

std::uint64_t parse_hex_bits(const json_value &value,
                             const std::size_t digits) {
  const auto text = json_string(value);
  if (text.size() != digits) {
    invalid_json("floating-point bit string has the wrong length");
  }
  std::uint64_t result = 0U;
  for (const char character : text) {
    unsigned digit = 0U;
    if (character >= '0' && character <= '9') {
      digit = static_cast<unsigned>(character - '0');
    } else if (character >= 'a' && character <= 'f') {
      digit = static_cast<unsigned>(character - 'a' + 10);
    } else {
      invalid_json("floating-point bit string must be lowercase hex");
    }
    result = (result << 4U) | digit;
  }
  return result;
}

json_value float64_value(const double value) {
  return object_of({{"$f64", string_value(hex_bits(
                                 std::bit_cast<std::uint64_t>(value), 16U))}});
}

double parse_float64(const json_value &value) {
  exact_keys(value, {"$f64"});
  return std::bit_cast<double>(parse_hex_bits(member(value, "$f64"), 16U));
}

json_value float32_value(const float value) {
  return object_of({{"$f32", string_value(hex_bits(
                                 std::bit_cast<std::uint32_t>(value), 8U))}});
}

float parse_float32(const json_value &value) {
  exact_keys(value, {"$f32"});
  return std::bit_cast<float>(
      static_cast<std::uint32_t>(parse_hex_bits(member(value, "$f32"), 8U)));
}

template <typename T> json_value integer_column_value(const T value) {
  if constexpr (std::is_signed_v<T>) {
    return string_value(std::to_string(static_cast<std::int64_t>(value)));
  } else {
    return string_value(std::to_string(static_cast<std::uint64_t>(value)));
  }
}

template <typename T> T parse_integer_column(const json_value &value) {
  if constexpr (std::is_signed_v<T>) {
    const auto parsed = signed_integer(value);
    if (parsed < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        parsed > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      invalid_json("signed dataframe value is out of range");
    }
    return static_cast<T>(parsed);
  } else {
    const auto parsed = unsigned_integer(value);
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      invalid_json("unsigned dataframe value is out of range");
    }
    return static_cast<T>(parsed);
  }
}

template <typename T> json_value float_vector(const std::vector<T> &values) {
  json_value::array result;
  result.reserve(values.size());
  for (const auto value : values) {
    if constexpr (std::is_same_v<T, float>) {
      result.push_back(float32_value(value));
    } else {
      result.push_back(float64_value(value));
    }
  }
  return array_value(std::move(result));
}

json_value string_vector(const std::vector<std::string> &values) {
  json_value::array result;
  result.reserve(values.size());
  for (const auto &value : values) {
    result.push_back(string_value(value));
  }
  return array_value(std::move(result));
}

json_value string_map(const std::map<std::string, std::string> &values) {
  json_value::object result;
  result.reserve(values.size());
  for (const auto &[key, value] : values) {
    result.emplace_back(key, string_value(value));
  }
  return object_value(std::move(result));
}

std::map<std::string, std::string> parse_string_map(const json_value &value) {
  std::map<std::string, std::string> result;
  for (const auto &[key, item] : json_object(value)) {
    result.emplace(key, json_string(item));
  }
  return result;
}

json_value size_value(const std::size_t value) {
  return string_value(std::to_string(static_cast<std::uint64_t>(value)));
}

std::size_t parse_size(const json_value &value) {
  const auto parsed = unsigned_integer(value);
  if (parsed >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    invalid_json("size value is out of range");
  }
  return static_cast<std::size_t>(parsed);
}

json_value optional_string_value(const std::optional<std::string> &value) {
  return value.has_value() ? string_value(*value) : null_value();
}

std::optional<std::string> parse_optional_string(const json_value &value) {
  if (std::holds_alternative<std::nullptr_t>(value.data)) {
    return std::nullopt;
  }
  return json_string(value);
}

json_value optional_double_value(const std::optional<double> &value) {
  return value.has_value() ? float64_value(*value) : null_value();
}

std::optional<double> parse_optional_double(const json_value &value) {
  if (std::holds_alternative<std::nullptr_t>(value.data)) {
    return std::nullopt;
  }
  return parse_float64(value);
}

json_value optional_bool_value(const std::optional<bool> &value) {
  return value.has_value() ? bool_value(*value) : null_value();
}

std::optional<bool> parse_optional_bool(const json_value &value) {
  if (std::holds_alternative<std::nullptr_t>(value.data)) {
    return std::nullopt;
  }
  return json_bool(value);
}

json_value report_cell_value(const report_cell &cell) {
  std::string kind;
  switch (cell.type) {
  case report_cell::kind::missing:
    kind = "missing";
    break;
  case report_cell::kind::number:
    kind = "number";
    break;
  case report_cell::kind::text:
    kind = "text";
    break;
  case report_cell::kind::boolean:
    kind = "boolean";
    break;
  default:
    throw input_error{"invalid report cell kind"};
  }
  return object_of({{"type", string_value(std::move(kind))},
                    {"number", float64_value(cell.number)},
                    {"boolean", bool_value(cell.boolean)},
                    {"text", string_value(cell.text)}});
}

report_cell parse_report_cell(const json_value &value) {
  exact_keys(value, {"type", "number", "boolean", "text"});
  report_cell result;
  const auto kind = json_string(member(value, "type"));
  if (kind == "missing") {
    result.type = report_cell::kind::missing;
  } else if (kind == "number") {
    result.type = report_cell::kind::number;
  } else if (kind == "text") {
    result.type = report_cell::kind::text;
  } else if (kind == "boolean") {
    result.type = report_cell::kind::boolean;
  } else {
    invalid_json("unknown report cell type");
  }
  result.number = parse_float64(member(value, "number"));
  result.boolean = json_bool(member(value, "boolean"));
  result.text = json_string(member(value, "text"));
  return result;
}

json_value report_table_value(const report_table &table) {
  json_value::array rows;
  rows.reserve(table.rows.size());
  for (const auto &row : table.rows) {
    json_value::array cells;
    cells.reserve(row.size());
    for (const auto &cell : row) {
      cells.push_back(report_cell_value(cell));
    }
    rows.push_back(array_value(std::move(cells)));
  }
  return object_of({{"key", string_value(table.key)},
                    {"title", string_value(table.title)},
                    {"columns", string_vector(table.columns)},
                    {"rows", array_value(std::move(rows))}});
}

report_table parse_report_table(const json_value &value) {
  exact_keys(value, {"key", "title", "columns", "rows"});
  report_table result;
  result.key = json_string(member(value, "key"));
  result.title = json_string(member(value, "title"));
  for (const auto &column : json_array(member(value, "columns"))) {
    result.columns.push_back(json_string(column));
  }
  for (const auto &row_value : json_array(member(value, "rows"))) {
    std::vector<report_cell> row;
    for (const auto &cell : json_array(row_value)) {
      row.push_back(parse_report_cell(cell));
    }
    result.rows.push_back(std::move(row));
  }
  return result;
}

json_value report_series_value(const report_series &series) {
  return object_of({{"key", string_value(series.key)},
                    {"title", string_value(series.title)},
                    {"x_label", string_value(series.x_label)},
                    {"y_label", string_value(series.y_label)},
                    {"unit", string_value(series.unit)},
                    {"x", float_vector(series.x)},
                    {"y", float_vector(series.y)}});
}

report_series parse_report_series(const json_value &value) {
  exact_keys(value, {"key", "title", "x_label", "y_label", "unit", "x", "y"});
  report_series result;
  result.key = json_string(member(value, "key"));
  result.title = json_string(member(value, "title"));
  result.x_label = json_string(member(value, "x_label"));
  result.y_label = json_string(member(value, "y_label"));
  result.unit = json_string(member(value, "unit"));
  for (const auto &item : json_array(member(value, "x"))) {
    result.x.push_back(parse_float64(item));
  }
  for (const auto &item : json_array(member(value, "y"))) {
    result.y.push_back(parse_float64(item));
  }
  return result;
}

json_value report_metric_value(const report_metric &metric) {
  return object_of({{"key", string_value(metric.key)},
                    {"label", string_value(metric.label)},
                    {"unit", string_value(metric.unit)},
                    {"number", optional_double_value(metric.number)},
                    {"boolean", optional_bool_value(metric.boolean)},
                    {"text", optional_string_value(metric.text)}});
}

report_metric parse_report_metric(const json_value &value) {
  exact_keys(value, {"key", "label", "unit", "number", "boolean", "text"});
  report_metric result;
  result.key = json_string(member(value, "key"));
  result.label = json_string(member(value, "label"));
  result.unit = json_string(member(value, "unit"));
  result.number = parse_optional_double(member(value, "number"));
  result.boolean = parse_optional_bool(member(value, "boolean"));
  result.text = parse_optional_string(member(value, "text"));
  return result;
}

json_value report_section_value(const report_section &section) {
  json_value::array metrics;
  metrics.reserve(section.metrics.size());
  for (const auto &metric : section.metrics) {
    metrics.push_back(report_metric_value(metric));
  }
  json_value::array tables;
  tables.reserve(section.tables.size());
  for (const auto &table : section.tables) {
    tables.push_back(report_table_value(table));
  }
  json_value::array series;
  series.reserve(section.series.size());
  for (const auto &item : section.series) {
    series.push_back(report_series_value(item));
  }
  return object_of({{"key", string_value(section.key)},
                    {"title", string_value(section.title)},
                    {"metrics", array_value(std::move(metrics))},
                    {"tables", array_value(std::move(tables))},
                    {"series", array_value(std::move(series))},
                    {"notes", string_vector(section.notes)},
                    {"units", string_map(section.units)},
                    {"legends", string_map(section.legends)}});
}

report_section parse_report_section(const json_value &value) {
  exact_keys(value, {"key", "title", "metrics", "tables", "series", "notes",
                     "units", "legends"});
  report_section result;
  result.key = json_string(member(value, "key"));
  result.title = json_string(member(value, "title"));
  for (const auto &item : json_array(member(value, "metrics"))) {
    result.metrics.push_back(parse_report_metric(item));
  }
  for (const auto &item : json_array(member(value, "tables"))) {
    result.tables.push_back(parse_report_table(item));
  }
  for (const auto &item : json_array(member(value, "series"))) {
    result.series.push_back(parse_report_series(item));
  }
  for (const auto &item : json_array(member(value, "notes"))) {
    result.notes.push_back(json_string(item));
  }
  result.units = parse_string_map(member(value, "units"));
  result.legends = parse_string_map(member(value, "legends"));
  return result;
}

json_value report_document_value(const report_document &document) {
  // Reuse the report model's validation boundary so native runtime
  // serialization accepts exactly the documents accepted by report APIs.
  static_cast<void>(document.to_json());
  json_value::array sections;
  sections.reserve(document.sections.size());
  for (const auto &section : document.sections) {
    sections.push_back(report_section_value(section));
  }
  return object_of({{"domain", string_value(document.domain)},
                    {"title", string_value(document.title)},
                    {"sections", array_value(std::move(sections))},
                    {"metadata", string_map(document.metadata)},
                    {"schema_version",
                     number_value(std::to_string(document.schema_version))},
                    {"offline_assets", string_map(document.offline_assets)}});
}

report_document parse_report_document(const json_value &value) {
  exact_keys(value, {"domain", "title", "sections", "metadata",
                     "schema_version", "offline_assets"});
  report_document result;
  result.domain = json_string(member(value, "domain"));
  result.title = json_string(member(value, "title"));
  for (const auto &section : json_array(member(value, "sections"))) {
    result.sections.push_back(parse_report_section(section));
  }
  result.metadata = parse_string_map(member(value, "metadata"));
  result.schema_version = small_integer(member(value, "schema_version"));
  result.offline_assets = parse_string_map(member(value, "offline_assets"));
  static_cast<void>(result.to_json());
  return result;
}

json_value dataframe_value(const dataframe &frame);
dataframe parse_dataframe(const json_value &value);
json_value risk_model_spec_value(const risk_model_spec &spec);
risk_model_spec parse_risk_model_spec(const json_value &value);
json_value walk_forward_value(const walk_forward_var_result &result);
walk_forward_var_result parse_walk_forward(const json_value &value);
json_value risk_validation_report_value(const risk_validation_report &report);
risk_validation_report parse_risk_validation_report(const json_value &value);
json_value
transactions_value(const std::vector<portfolio_transaction> &transactions);
std::vector<portfolio_transaction> parse_transactions(const json_value &value);

json_value runtime_value_json(const runtime_value &value) {
  if (std::holds_alternative<std::monostate>(value.data)) {
    return object_of({{"type", string_value("null")}, {"data", null_value()}});
  }
  if (const auto *item = std::get_if<bool>(&value.data)) {
    return object_of(
        {{"type", string_value("bool")}, {"data", bool_value(*item)}});
  }
  if (const auto *item = std::get_if<std::int64_t>(&value.data)) {
    return object_of({{"type", string_value("int64")},
                      {"data", string_value(std::to_string(*item))}});
  }
  if (const auto *item = std::get_if<double>(&value.data)) {
    return object_of(
        {{"type", string_value("double")}, {"data", float64_value(*item)}});
  }
  if (const auto *item = std::get_if<std::string>(&value.data)) {
    return object_of(
        {{"type", string_value("string")}, {"data", string_value(*item)}});
  }
  if (const auto *item = std::get_if<dataframe>(&value.data)) {
    return object_of({{"type", string_value("dataframe")},
                      {"data", dataframe_value(*item)}});
  }
  if (const auto *item = std::get_if<report_document>(&value.data)) {
    return object_of({{"type", string_value("report_document")},
                      {"data", report_document_value(*item)}});
  }
  if (const auto *item = std::get_if<risk_model_spec>(&value.data)) {
    return object_of({{"type", string_value("risk_model_spec")},
                      {"data", risk_model_spec_value(*item)}});
  }
  if (const auto *item = std::get_if<walk_forward_var_result>(&value.data)) {
    return object_of({{"type", string_value("walk_forward_var_result")},
                      {"data", walk_forward_value(*item)}});
  }
  if (const auto *item = std::get_if<risk_validation_report>(&value.data)) {
    return object_of({{"type", string_value("risk_validation_report")},
                      {"data", risk_validation_report_value(*item)}});
  }
  return object_of(
      {{"type", string_value("portfolio_transactions")},
       {"data", transactions_value(std::get<std::vector<portfolio_transaction>>(
                    value.data))}});
}

runtime_value parse_runtime_value(const json_value &value) {
  exact_keys(value, {"type", "data"});
  const auto type = json_string(member(value, "type"));
  const auto &data = member(value, "data");
  if (type == "null") {
    if (!std::holds_alternative<std::nullptr_t>(data.data)) {
      invalid_json("null runtime value must have null data");
    }
    return runtime_value{};
  }
  if (type == "bool") {
    return runtime_value(json_bool(data));
  }
  if (type == "int64") {
    return runtime_value(signed_integer(data));
  }
  if (type == "double") {
    return runtime_value(parse_float64(data));
  }
  if (type == "string") {
    return runtime_value(json_string(data));
  }
  if (type == "dataframe") {
    return runtime_value(parse_dataframe(data));
  }
  if (type == "report_document") {
    return runtime_value(parse_report_document(data));
  }
  if (type == "risk_model_spec") {
    return runtime_value(parse_risk_model_spec(data));
  }
  if (type == "walk_forward_var_result") {
    return runtime_value(parse_walk_forward(data));
  }
  if (type == "risk_validation_report") {
    return runtime_value(parse_risk_validation_report(data));
  }
  if (type == "portfolio_transactions") {
    return runtime_value(parse_transactions(data));
  }
  invalid_json("unknown runtime value type");
}

json_value payload_json(const runtime_result_payload &payload) {
  if (payload.schema_version != runtime_result_schema_version) {
    throw input_error{"unsupported runtime result schema version"};
  }
  return object_of(
      {{"schema", string_value("fincore.runtime_result")},
       {"schema_version", number_value(std::to_string(payload.schema_version))},
       {"value", runtime_value_json(payload.value)},
       {"metadata", string_map(payload.metadata)}});
}

runtime_result_payload parse_payload(const json_value &value) {
  exact_keys(value, {"schema", "schema_version", "value", "metadata"});
  if (json_string(member(value, "schema")) != "fincore.runtime_result") {
    invalid_json("unknown schema identifier");
  }
  runtime_result_payload result;
  result.schema_version = small_integer(member(value, "schema_version"));
  if (result.schema_version != runtime_result_schema_version) {
    invalid_json("unsupported schema version");
  }
  result.value = parse_runtime_value(member(value, "value"));
  result.metadata = parse_string_map(member(value, "metadata"));
  return result;
}

} // namespace

runtime_result_payload to_payload(const runtime_result &result) {
  return {runtime_result_schema_version, result.value, result.metadata};
}

runtime_result from_payload(const runtime_result_payload &payload) {
  if (payload.schema_version != runtime_result_schema_version) {
    throw input_error{"unsupported runtime result schema version"};
  }
  // Apply the same supported-value and nested-shape checks as the JSON
  // boundary, even when the caller starts with an owning C++ payload.
  const auto validated = parse_payload(payload_json(payload));
  return {validated.value, validated.metadata};
}

std::string to_json(const runtime_result &result) {
  const auto payload = to_payload(result);
  const auto value = payload_json(payload);
  std::string output;
  append_json(output, value);
  return output;
}

runtime_result from_json(const std::string_view json) {
  const auto parsed = json_parser(json).parse();
  return from_payload(parse_payload(parsed));
}

std::string semantic_digest(const runtime_result &result) {
  auto stable_payload = to_payload(result);
  for (auto entry = stable_payload.metadata.begin();
       entry != stable_payload.metadata.end();) {
    const auto &key = entry->first;
    const bool volatile_field =
        key == "duration_ns" || key == "finished_ns" || key == "run_id" ||
        key == "started_ns" || key == "temporary_path" ||
        key == "workspace_path" || key.ends_with("_path");
    if (volatile_field) {
      entry = stable_payload.metadata.erase(entry);
    } else {
      ++entry;
    }
  }
  std::string canonical;
  const auto value = payload_json(stable_payload);
  append_json(canonical, value);
  std::string payload{"fincore.runtime_result.semantic_digest:v1"};
  payload.push_back('\0');
  payload += canonical;
  return detail::sha256_hex(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t *>(payload.data()), payload.size()));
}

runtime_result copy_for_consumer(const runtime_result &result) {
  return {result.value, result.metadata};
}

runtime_result
with_metadata(const runtime_result &result,
              const std::map<std::string, std::string> &metadata) {
  auto copy = copy_for_consumer(result);
  for (const auto &[key, value] : metadata) {
    copy.metadata.insert_or_assign(key, value);
  }
  return copy;
}

} // namespace fincore

namespace fincore {
namespace {

json_value risk_model_spec_value(const risk_model_spec &spec) {
  std::string_view target;
  switch (spec.forecast_target) {
  case risk_forecast_target::var:
    target = "var";
    break;
  case risk_forecast_target::es:
    target = "es";
    break;
  case risk_forecast_target::pair:
    target = "pair";
    break;
  default:
    throw input_error{"invalid risk forecast target"};
  }
  std::string_view tail;
  switch (spec.tail) {
  case risk_tail::lower:
    tail = "lower";
    break;
  case risk_tail::upper:
    tail = "upper";
    break;
  default:
    throw input_error{"invalid risk tail"};
  }
  return object_of({{"forecast_target", string_value(std::string(target))},
                    {"confidence_level", float64_value(spec.confidence_level)},
                    {"horizon", size_value(spec.horizon)},
                    {"distribution", string_value(spec.distribution)},
                    {"tail", string_value(std::string(tail))},
                    {"sign_convention", string_value(spec.sign_convention)},
                    {"window", size_value(spec.window)},
                    {"refit_cadence", size_value(spec.refit_cadence)},
                    {"model_version", string_value(spec.model_version)}});
}

risk_model_spec parse_risk_model_spec(const json_value &value) {
  exact_keys(value, {"forecast_target", "confidence_level", "horizon",
                     "distribution", "tail", "sign_convention", "window",
                     "refit_cadence", "model_version"});
  risk_model_spec result;
  const auto target = json_string(member(value, "forecast_target"));
  if (target == "var") {
    result.forecast_target = risk_forecast_target::var;
  } else if (target == "es") {
    result.forecast_target = risk_forecast_target::es;
  } else if (target == "pair") {
    result.forecast_target = risk_forecast_target::pair;
  } else {
    invalid_json("unknown risk forecast target");
  }
  result.confidence_level = parse_float64(member(value, "confidence_level"));
  result.horizon = parse_size(member(value, "horizon"));
  result.distribution = json_string(member(value, "distribution"));
  const auto tail = json_string(member(value, "tail"));
  if (tail == "lower") {
    result.tail = risk_tail::lower;
  } else if (tail == "upper") {
    result.tail = risk_tail::upper;
  } else {
    invalid_json("unknown risk tail");
  }
  result.sign_convention = json_string(member(value, "sign_convention"));
  result.window = parse_size(member(value, "window"));
  result.refit_cadence = parse_size(member(value, "refit_cadence"));
  result.model_version = json_string(member(value, "model_version"));
  return result;
}

std::string_view backtest_method_name(const risk_backtest_method method) {
  switch (method) {
  case risk_backtest_method::var:
    return "var";
  case risk_backtest_method::es:
    return "es";
  }
  throw input_error{"invalid risk backtest method"};
}

json_value backtest_value(const risk_backtest_result &value) {
  const auto method = backtest_method_name(value.method);
  std::string_view status;
  switch (value.status) {
  case risk_backtest_status::pass:
    status = "pass";
    break;
  case risk_backtest_status::fail:
    status = "fail";
    break;
  case risk_backtest_status::inconclusive:
    status = "inconclusive";
    break;
  case risk_backtest_status::experimental:
    status = "experimental";
    break;
  default:
    throw input_error{"invalid risk backtest status"};
  }
  json_value::object diagnostics;
  diagnostics.reserve(value.diagnostics.size());
  for (const auto &[key, item] : value.diagnostics) {
    diagnostics.emplace_back(key, float64_value(item));
  }
  return object_of(
      {{"method", string_value(std::string(method))},
       {"confidence_level", float64_value(value.confidence_level)},
       {"observations", size_value(value.observations)},
       {"exceptions", size_value(value.exceptions)},
       {"expected_exceptions", float64_value(value.expected_exceptions)},
       {"exception_rate", float64_value(value.exception_rate)},
       {"kupiec_lr", float64_value(value.kupiec_lr)},
       {"kupiec_pvalue", float64_value(value.kupiec_pvalue)},
       {"christoffersen_lr", float64_value(value.christoffersen_lr)},
       {"christoffersen_pvalue", float64_value(value.christoffersen_pvalue)},
       {"inputs_digest", string_value(value.inputs_digest)},
       {"diagnostics", object_value(std::move(diagnostics))},
       {"status", string_value(std::string(status))}});
}

risk_backtest_result parse_backtest(const json_value &value) {
  exact_keys(value,
             {"method", "confidence_level", "observations", "exceptions",
              "expected_exceptions", "exception_rate", "kupiec_lr",
              "kupiec_pvalue", "christoffersen_lr", "christoffersen_pvalue",
              "inputs_digest", "diagnostics", "status"});
  risk_backtest_result result{};
  const auto method = json_string(member(value, "method"));
  if (method == "var") {
    result.method = risk_backtest_method::var;
  } else if (method == "es") {
    result.method = risk_backtest_method::es;
  } else {
    invalid_json("unknown risk backtest method");
  }
  result.confidence_level = parse_float64(member(value, "confidence_level"));
  result.observations = parse_size(member(value, "observations"));
  result.exceptions = parse_size(member(value, "exceptions"));
  result.expected_exceptions =
      parse_float64(member(value, "expected_exceptions"));
  result.exception_rate = parse_float64(member(value, "exception_rate"));
  result.kupiec_lr = parse_float64(member(value, "kupiec_lr"));
  result.kupiec_pvalue = parse_float64(member(value, "kupiec_pvalue"));
  result.christoffersen_lr = parse_float64(member(value, "christoffersen_lr"));
  result.christoffersen_pvalue =
      parse_float64(member(value, "christoffersen_pvalue"));
  result.inputs_digest = json_string(member(value, "inputs_digest"));
  for (const auto &[key, item] : json_object(member(value, "diagnostics"))) {
    result.diagnostics.emplace(key, parse_float64(item));
  }
  const auto status = json_string(member(value, "status"));
  if (status == "pass") {
    result.status = risk_backtest_status::pass;
  } else if (status == "fail") {
    result.status = risk_backtest_status::fail;
  } else if (status == "inconclusive") {
    result.status = risk_backtest_status::inconclusive;
  } else if (status == "experimental") {
    result.status = risk_backtest_status::experimental;
  } else {
    invalid_json("unknown risk backtest status");
  }
  return result;
}

std::string_view walk_status_name(const walk_forward_status status) {
  switch (status) {
  case walk_forward_status::ok:
    return "ok";
  case walk_forward_status::insufficient_data:
    return "insufficient_data";
  case walk_forward_status::unsupported:
    return "unsupported";
  }
  throw input_error{"invalid walk-forward status"};
}

json_value refit_value(const refit_parameters &fit) {
  return object_of(
      {{"date", string_value(std::to_string(fit.date))},
       {"forecast", float64_value(fit.forecast)},
       {"mean", float64_value(fit.mean)},
       {"standard_deviation", float64_value(fit.standard_deviation)},
       {"n_observations", size_value(fit.n_observations)}});
}

refit_parameters parse_refit(const json_value &value) {
  exact_keys(value, {"date", "forecast", "mean", "standard_deviation",
                     "n_observations"});
  return {signed_integer(member(value, "date")),
          parse_float64(member(value, "forecast")),
          parse_float64(member(value, "mean")),
          parse_float64(member(value, "standard_deviation")),
          parse_size(member(value, "n_observations"))};
}

json_value walk_forward_value(const walk_forward_var_result &result) {
  json_value::array index;
  index.reserve(result.index.size());
  for (const auto date : result.index) {
    index.push_back(string_value(std::to_string(date)));
  }
  json_value::array forecasts;
  forecasts.reserve(result.forecast.size());
  for (const auto value : result.forecast) {
    forecasts.push_back(float64_value(value));
  }
  json_value::array realized;
  realized.reserve(result.realized.size());
  for (const auto value : result.realized) {
    realized.push_back(float64_value(value));
  }
  json_value::array refit_timestamps;
  refit_timestamps.reserve(result.refit_timestamps.size());
  for (const auto date : result.refit_timestamps) {
    refit_timestamps.push_back(string_value(std::to_string(date)));
  }
  json_value::array fits;
  fits.reserve(result.fits.size());
  for (const auto &fit : result.fits) {
    fits.push_back(refit_value(fit));
  }
  return object_of(
      {{"spec", risk_model_spec_value(result.spec)},
       {"index", array_value(std::move(index))},
       {"forecast", array_value(std::move(forecasts))},
       {"realized", array_value(std::move(realized))},
       {"refit_timestamps", array_value(std::move(refit_timestamps))},
       {"fits", array_value(std::move(fits))},
       {"inputs_digest", string_value(result.inputs_digest)},
       {"status", string_value(std::string(walk_status_name(result.status)))},
       {"unsupported_reason", string_value(result.unsupported_reason)},
       {"n_observations", size_value(result.n_observations)},
       {"backtest", backtest_value(result.backtest)},
       {"has_backtest", bool_value(result.has_backtest)}});
}

walk_forward_var_result parse_walk_forward(const json_value &value) {
  exact_keys(value,
             {"spec", "index", "forecast", "realized", "refit_timestamps",
              "fits", "inputs_digest", "status", "unsupported_reason",
              "n_observations", "backtest", "has_backtest"});
  walk_forward_var_result result{};
  result.spec = parse_risk_model_spec(member(value, "spec"));
  for (const auto &item : json_array(member(value, "index"))) {
    result.index.push_back(signed_integer(item));
  }
  for (const auto &item : json_array(member(value, "forecast"))) {
    result.forecast.push_back(parse_float64(item));
  }
  for (const auto &item : json_array(member(value, "realized"))) {
    result.realized.push_back(parse_float64(item));
  }
  for (const auto &item : json_array(member(value, "refit_timestamps"))) {
    result.refit_timestamps.push_back(signed_integer(item));
  }
  for (const auto &item : json_array(member(value, "fits"))) {
    result.fits.push_back(parse_refit(item));
  }
  result.inputs_digest = json_string(member(value, "inputs_digest"));
  const auto status = json_string(member(value, "status"));
  if (status == "ok") {
    result.status = walk_forward_status::ok;
  } else if (status == "insufficient_data") {
    result.status = walk_forward_status::insufficient_data;
  } else if (status == "unsupported") {
    result.status = walk_forward_status::unsupported;
  } else {
    invalid_json("unknown walk-forward status");
  }
  result.unsupported_reason = json_string(member(value, "unsupported_reason"));
  result.n_observations = parse_size(member(value, "n_observations"));
  result.backtest = parse_backtest(member(value, "backtest"));
  result.has_backtest = json_bool(member(value, "has_backtest"));
  return result;
}

json_value forecast_event_value(const risk_forecast_event &event) {
  return object_of({{"date", string_value(std::to_string(event.date))},
                    {"forecast", float64_value(event.forecast)},
                    {"realized", float64_value(event.realized)},
                    {"exception", bool_value(event.exception)},
                    {"refit", bool_value(event.refit)}});
}

risk_forecast_event parse_forecast_event(const json_value &value) {
  exact_keys(value, {"date", "forecast", "realized", "exception", "refit"});
  return {signed_integer(member(value, "date")),
          parse_float64(member(value, "forecast")),
          parse_float64(member(value, "realized")),
          json_bool(member(value, "exception")),
          json_bool(member(value, "refit"))};
}

json_value risk_refit_value(const risk_refit_event &refit) {
  return object_of(
      {{"date", string_value(std::to_string(refit.date))},
       {"forecast", float64_value(refit.forecast)},
       {"mean", float64_value(refit.mean)},
       {"standard_deviation", float64_value(refit.standard_deviation)},
       {"n_observations", size_value(refit.n_observations)}});
}

risk_refit_event parse_risk_refit(const json_value &value) {
  exact_keys(value, {"date", "forecast", "mean", "standard_deviation",
                     "n_observations"});
  return {signed_integer(member(value, "date")),
          parse_float64(member(value, "forecast")),
          parse_float64(member(value, "mean")),
          parse_float64(member(value, "standard_deviation")),
          parse_size(member(value, "n_observations"))};
}

json_value diagnostics_value(const risk_report_diagnostics &diagnostics) {
  return object_of(
      {{"method", string_value(diagnostics.method)},
       {"quantile_method", string_value(diagnostics.quantile_method)},
       {"window", size_value(diagnostics.window)},
       {"refit_cadence", size_value(diagnostics.refit_cadence)},
       {"forecast_count", size_value(diagnostics.forecast_count)},
       {"backtest_status", string_value(diagnostics.backtest_status)},
       {"reason", string_value(diagnostics.reason)},
       {"n_observations", size_value(diagnostics.n_observations)}});
}

risk_report_diagnostics parse_diagnostics(const json_value &value) {
  exact_keys(value,
             {"method", "quantile_method", "window", "refit_cadence",
              "forecast_count", "backtest_status", "reason", "n_observations"});
  risk_report_diagnostics result;
  result.method = json_string(member(value, "method"));
  result.quantile_method = json_string(member(value, "quantile_method"));
  result.window = parse_size(member(value, "window"));
  result.refit_cadence = parse_size(member(value, "refit_cadence"));
  result.forecast_count = parse_size(member(value, "forecast_count"));
  result.backtest_status = json_string(member(value, "backtest_status"));
  result.reason = json_string(member(value, "reason"));
  result.n_observations = parse_size(member(value, "n_observations"));
  return result;
}

json_value risk_validation_report_value(const risk_validation_report &report) {
  if (report.schema_version != risk_validation_report_schema_version) {
    throw input_error{"unsupported risk validation report schema version"};
  }
  json_value::array events;
  events.reserve(report.forecast_events.size());
  for (const auto &event : report.forecast_events) {
    events.push_back(forecast_event_value(event));
  }
  json_value::array refits;
  refits.reserve(report.refits.size());
  for (const auto &refit : report.refits) {
    refits.push_back(risk_refit_value(refit));
  }
  return object_of(
      {{"status", string_value(std::string(walk_status_name(report.status)))},
       {"inputs_digest", string_value(report.inputs_digest)},
       {"specification", risk_model_spec_value(report.specification)},
       {"forecast_events", array_value(std::move(events))},
       {"refits", array_value(std::move(refits))},
       {"diagnostics", diagnostics_value(report.diagnostics)},
       {"has_backtest", bool_value(report.has_backtest)},
       {"backtest", backtest_value(report.backtest)},
       {"traffic_light_zone", string_value(report.traffic_light_zone)},
       {"timestamp_index_name",
        optional_string_value(report.timestamp_index_name)},
       {"timestamp_timezone", optional_string_value(report.timestamp_timezone)},
       {"disclosure", string_value(report.disclosure)},
       {"schema_version",
        number_value(std::to_string(report.schema_version))}});
}

risk_validation_report parse_risk_validation_report(const json_value &value) {
  exact_keys(value, {"status", "inputs_digest", "specification",
                     "forecast_events", "refits", "diagnostics", "has_backtest",
                     "backtest", "traffic_light_zone", "timestamp_index_name",
                     "timestamp_timezone", "disclosure", "schema_version"});
  risk_validation_report result;
  const auto status = json_string(member(value, "status"));
  if (status == "ok") {
    result.status = walk_forward_status::ok;
  } else if (status == "insufficient_data") {
    result.status = walk_forward_status::insufficient_data;
  } else if (status == "unsupported") {
    result.status = walk_forward_status::unsupported;
  } else {
    invalid_json("unknown risk validation status");
  }
  result.inputs_digest = json_string(member(value, "inputs_digest"));
  result.specification = parse_risk_model_spec(member(value, "specification"));
  for (const auto &item : json_array(member(value, "forecast_events"))) {
    result.forecast_events.push_back(parse_forecast_event(item));
  }
  for (const auto &item : json_array(member(value, "refits"))) {
    result.refits.push_back(parse_risk_refit(item));
  }
  result.diagnostics = parse_diagnostics(member(value, "diagnostics"));
  result.has_backtest = json_bool(member(value, "has_backtest"));
  result.backtest = parse_backtest(member(value, "backtest"));
  result.traffic_light_zone = json_string(member(value, "traffic_light_zone"));
  result.timestamp_index_name =
      parse_optional_string(member(value, "timestamp_index_name"));
  result.timestamp_timezone =
      parse_optional_string(member(value, "timestamp_timezone"));
  result.disclosure = json_string(member(value, "disclosure"));
  result.schema_version = small_integer(member(value, "schema_version"));
  if (result.schema_version != risk_validation_report_schema_version) {
    invalid_json("unsupported risk validation report schema version");
  }
  return result;
}

json_value
transactions_value(const std::vector<portfolio_transaction> &transactions) {
  json_value::array result;
  result.reserve(transactions.size());
  for (const auto &transaction : transactions) {
    result.push_back(
        object_of({{"date", string_value(std::to_string(transaction.date))},
                   {"symbol", string_value(transaction.symbol)},
                   {"amount", float64_value(transaction.amount)},
                   {"price", float64_value(transaction.price)}}));
  }
  return array_value(std::move(result));
}

std::vector<portfolio_transaction> parse_transactions(const json_value &value) {
  std::vector<portfolio_transaction> result;
  for (const auto &item : json_array(value)) {
    exact_keys(item, {"date", "symbol", "amount", "price"});
    result.push_back({signed_integer(member(item, "date")),
                      json_string(member(item, "symbol")),
                      parse_float64(member(item, "amount")),
                      parse_float64(member(item, "price"))});
  }
  return result;
}

template <typename T>
json_value dataframe_column_values(const dataframe &frame,
                                   const std::string &name) {
  const auto &values = frame.get_column<T>(name.c_str());
  json_value::array result;
  result.reserve(values.size());
  for (const auto &item : values) {
    if constexpr (std::is_same_v<T, bool>) {
      result.push_back(bool_value(static_cast<bool>(item)));
    } else if constexpr (std::is_same_v<T, float>) {
      result.push_back(float32_value(item));
    } else if constexpr (std::is_same_v<T, double>) {
      result.push_back(float64_value(item));
    } else if constexpr (std::is_integral_v<T>) {
      result.push_back(integer_column_value(static_cast<T>(item)));
    } else if constexpr (std::is_same_v<T, std::string>) {
      result.push_back(string_value(item));
    }
  }
  return array_value(std::move(result));
}

std::string dataframe_column_type(const std::type_index type) {
#define FINCORE_RUNTIME_COLUMN_TYPE(Type, Label)                               \
  if (type == std::type_index(typeid(Type))) {                                 \
    return Label;                                                              \
  }
  FINCORE_RUNTIME_COLUMN_TYPE(bool, "bool")
  FINCORE_RUNTIME_COLUMN_TYPE(char, "char")
  FINCORE_RUNTIME_COLUMN_TYPE(signed char, "signed_char")
  FINCORE_RUNTIME_COLUMN_TYPE(unsigned char, "unsigned_char")
  FINCORE_RUNTIME_COLUMN_TYPE(short, "short")
  FINCORE_RUNTIME_COLUMN_TYPE(unsigned short, "unsigned_short")
  FINCORE_RUNTIME_COLUMN_TYPE(int, "int")
  FINCORE_RUNTIME_COLUMN_TYPE(unsigned int, "unsigned_int")
  FINCORE_RUNTIME_COLUMN_TYPE(long, "long")
  FINCORE_RUNTIME_COLUMN_TYPE(unsigned long, "unsigned_long")
  FINCORE_RUNTIME_COLUMN_TYPE(long long, "long_long")
  FINCORE_RUNTIME_COLUMN_TYPE(unsigned long long, "unsigned_long_long")
  FINCORE_RUNTIME_COLUMN_TYPE(float, "float")
  FINCORE_RUNTIME_COLUMN_TYPE(double, "double")
  FINCORE_RUNTIME_COLUMN_TYPE(std::string, "string")
#undef FINCORE_RUNTIME_COLUMN_TYPE
  throw input_error{"unsupported runtime dataframe column type"};
}

json_value dataframe_column_value(const dataframe &frame,
                                  const std::string &name,
                                  const std::type_index type) {
#define FINCORE_RUNTIME_COLUMN_VALUE(Type, Label)                              \
  if (type == std::type_index(typeid(Type))) {                                 \
    return object_of(                                                          \
        {{"name", string_value(name)},                                         \
         {"type", string_value(Label)},                                        \
         {"values", dataframe_column_values<Type>(frame, name)}});             \
  }
  FINCORE_RUNTIME_COLUMN_VALUE(bool, "bool")
  FINCORE_RUNTIME_COLUMN_VALUE(char, "char")
  FINCORE_RUNTIME_COLUMN_VALUE(signed char, "signed_char")
  FINCORE_RUNTIME_COLUMN_VALUE(unsigned char, "unsigned_char")
  FINCORE_RUNTIME_COLUMN_VALUE(short, "short")
  FINCORE_RUNTIME_COLUMN_VALUE(unsigned short, "unsigned_short")
  FINCORE_RUNTIME_COLUMN_VALUE(int, "int")
  FINCORE_RUNTIME_COLUMN_VALUE(unsigned int, "unsigned_int")
  FINCORE_RUNTIME_COLUMN_VALUE(long, "long")
  FINCORE_RUNTIME_COLUMN_VALUE(unsigned long, "unsigned_long")
  FINCORE_RUNTIME_COLUMN_VALUE(long long, "long_long")
  FINCORE_RUNTIME_COLUMN_VALUE(unsigned long long, "unsigned_long_long")
  FINCORE_RUNTIME_COLUMN_VALUE(float, "float")
  FINCORE_RUNTIME_COLUMN_VALUE(double, "double")
  FINCORE_RUNTIME_COLUMN_VALUE(std::string, "string")
#undef FINCORE_RUNTIME_COLUMN_VALUE
  throw input_error{"unsupported runtime dataframe column type: " + name};
}

json_value dataframe_value(const dataframe &frame) {
  const auto &index = frame.get_index();
  if (index.size() >
      static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) {
    throw input_error{"runtime dataframe index is too large"};
  }
  json_value::array encoded_index;
  encoded_index.reserve(index.size());
  for (const auto item : index) {
    encoded_index.push_back(string_value(std::to_string(item)));
  }

  const auto columns =
      frame.get_columns_info<bool, char, signed char, unsigned char, short,
                             unsigned short, int, unsigned int, long,
                             unsigned long, long long, unsigned long long,
                             float, double, std::string>();
  if (columns.size() != frame.shape().second) {
    throw input_error{"unsupported runtime dataframe column type"};
  }
  json_value::array encoded_columns;
  encoded_columns.reserve(columns.size());
  for (const auto &info : columns) {
    const std::string name{std::get<0>(info).c_str()};
    const std::size_t column_size = std::get<1>(info);
    if (column_size > index.size()) {
      throw input_error{"runtime dataframe column exceeds row count: " + name};
    }
    const auto type = std::get<2>(info);
    static_cast<void>(dataframe_column_type(type));
    encoded_columns.push_back(dataframe_column_value(frame, name, type));
  }
  return object_of({{"index", array_value(std::move(encoded_index))},
                    {"columns", array_value(std::move(encoded_columns))}});
}

template <typename T>
typename dataframe::template StlVecType<T>
parse_dataframe_values(const json_value &value) {
  typename dataframe::template StlVecType<T> result;
  const auto &input = json_array(value);
  result.reserve(input.size());
  for (const auto &item : input) {
    if constexpr (std::is_same_v<T, bool>) {
      result.push_back(json_bool(item));
    } else if constexpr (std::is_same_v<T, float>) {
      result.push_back(parse_float32(item));
    } else if constexpr (std::is_same_v<T, double>) {
      result.push_back(parse_float64(item));
    } else if constexpr (std::is_integral_v<T>) {
      result.push_back(parse_integer_column<T>(item));
    } else if constexpr (std::is_same_v<T, std::string>) {
      result.push_back(json_string(item));
    }
  }
  return result;
}

template <typename T>
void load_dataframe_column(dataframe &frame, const std::string &name,
                           const json_value &values) {
  auto decoded = parse_dataframe_values<T>(values);
  frame.load_column<T>(name.c_str(), std::move(decoded),
                       hmdf::nan_policy::dont_pad_with_nans);
}

void load_dataframe_column_by_type(dataframe &frame, const std::string &name,
                                   const std::string_view type,
                                   const json_value &values) {
#define FINCORE_RUNTIME_LOAD_COLUMN(Label, Type)                               \
  if (type == Label) {                                                         \
    load_dataframe_column<Type>(frame, name, values);                          \
    return;                                                                    \
  }
  FINCORE_RUNTIME_LOAD_COLUMN("bool", bool)
  FINCORE_RUNTIME_LOAD_COLUMN("char", char)
  FINCORE_RUNTIME_LOAD_COLUMN("signed_char", signed char)
  FINCORE_RUNTIME_LOAD_COLUMN("unsigned_char", unsigned char)
  FINCORE_RUNTIME_LOAD_COLUMN("short", short)
  FINCORE_RUNTIME_LOAD_COLUMN("unsigned_short", unsigned short)
  FINCORE_RUNTIME_LOAD_COLUMN("int", int)
  FINCORE_RUNTIME_LOAD_COLUMN("unsigned_int", unsigned int)
  FINCORE_RUNTIME_LOAD_COLUMN("long", long)
  FINCORE_RUNTIME_LOAD_COLUMN("unsigned_long", unsigned long)
  FINCORE_RUNTIME_LOAD_COLUMN("long_long", long long)
  FINCORE_RUNTIME_LOAD_COLUMN("unsigned_long_long", unsigned long long)
  FINCORE_RUNTIME_LOAD_COLUMN("float", float)
  FINCORE_RUNTIME_LOAD_COLUMN("double", double)
  FINCORE_RUNTIME_LOAD_COLUMN("string", std::string)
#undef FINCORE_RUNTIME_LOAD_COLUMN
  invalid_json("unknown dataframe column type");
}

dataframe parse_dataframe(const json_value &value) {
  exact_keys(value, {"index", "columns"});
  const auto &index_values = json_array(member(value, "index"));
  std::vector<timestamp> index;
  index.reserve(index_values.size());
  for (const auto &item : index_values) {
    index.push_back(signed_integer(item));
  }
  dataframe result;
  result.load_index(index.begin(), index.end());

  std::set<std::string> names;
  for (const auto &column : json_array(member(value, "columns"))) {
    exact_keys(column, {"name", "type", "values"});
    const auto name = json_string(member(column, "name"));
    if (!names.insert(name).second) {
      invalid_json("duplicate dataframe column name");
    }
    if (name == "INDEX" || name.find('\0') != std::string::npos ||
        name.size() > 62U) {
      invalid_json("dataframe column name cannot be represented natively");
    }
    const auto type = json_string(member(column, "type"));
    const auto &values = member(column, "values");
    if (json_array(values).size() > index.size()) {
      invalid_json("dataframe column exceeds index length");
    }
    load_dataframe_column_by_type(result, name, type, values);
  }
  return result;
}

} // namespace
} // namespace fincore
