#include "NativePackageAPI.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>

namespace {

void err(ExprPackageStringView *e, const std::string &s) {
  if (!e)
    return;
  static thread_local std::string message;
  message = s;
  *e = {message.c_str(), message.size()};
}

bool stringValue(const ExprPackageValue &value, std::string &out,
                 ExprPackageStringView *e, const char *description) {
  if (value.kind != EXPR_PACKAGE_VALUE_STR ||
      (!value.as.string_value.data && value.as.string_value.length != 0)) {
    err(e, std::string("expected ") + description + " to be a str");
    return false;
  }
  if (value.as.string_value.length == 0) {
    out.clear();
  } else {
    out.assign(value.as.string_value.data, value.as.string_value.length);
  }
  return true;
}

bool one(const ExprPackageValue *a, size_t n, std::string &o,
         ExprPackageStringView *e, const char *functionName) {
  if (n != 1 || !a) {
    err(e, std::string(functionName) + " expects one str argument");
    return false;
  }
  if (!stringValue(a[0], o, e, "path"))
    return false;
  return true;
}

bool read(const ExprHostApi *, const ExprPackageValue *a, size_t n,
          ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e, "readText"))
    return false;
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    err(e, "could not read '" + p + "'");
    return false;
  }
  static thread_local std::string s;
  s.assign(std::istreambuf_iterator<char>(f), {});
  if (f.bad()) {
    err(e, "could not completely read '" + p + "'");
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_STR;
  r->as.string_value = {s.c_str(), s.size()};
  return true;
}
bool write(const ExprHostApi *, const ExprPackageValue *a, size_t n,
           ExprPackageValue *r, ExprPackageStringView *e) {
  if (n != 2 || !a) {
    err(e, "writeText expects path and content strings");
    return false;
  }
  std::string p;
  std::string content;
  if (!stringValue(a[0], p, e, "path") ||
      !stringValue(a[1], content, e, "content"))
    return false;
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) {
    err(e, "could not write '" + p + "'");
    return false;
  }
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!f) {
    err(e, "could not write '" + p + "'");
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}
enum Q { E, F, D };
bool query(const ExprHostApi *, const ExprPackageValue *a, size_t n,
           ExprPackageValue *r, ExprPackageStringView *e, Q q) {
  std::string p;
  if (!one(a, n, p, e, q == E ? "exists" : q == F ? "isFile" : "isDirectory"))
    return false;
  std::error_code c;
  bool v = q == E   ? std::filesystem::exists(p, c)
           : q == F ? std::filesystem::is_regular_file(p, c)
                    : std::filesystem::is_directory(p, c);
  if (c) {
    err(e, c.message());
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_BOOL;
  r->as.boolean_value = v;
  return true;
}
bool ex(const ExprHostApi *h, const ExprPackageValue *a, size_t n,
        ExprPackageValue *r, ExprPackageStringView *e) {
  return query(h, a, n, r, e, E);
}
bool file(const ExprHostApi *h, const ExprPackageValue *a, size_t n,
          ExprPackageValue *r, ExprPackageStringView *e) {
  return query(h, a, n, r, e, F);
}
bool dir(const ExprHostApi *h, const ExprPackageValue *a, size_t n,
         ExprPackageValue *r, ExprPackageStringView *e) {
  return query(h, a, n, r, e, D);
}
bool cwd(const ExprHostApi *, const ExprPackageValue *, size_t n,
         ExprPackageValue *r, ExprPackageStringView *e) {
  if (n) {
    err(e, "workingDirectory expects no arguments");
    return false;
  }
  std::error_code c;
  const std::filesystem::path path = std::filesystem::current_path(c);
  if (c) {
    err(e, "could not get working directory: " + c.message());
    return false;
  }
  static thread_local std::string s;
  s = path.string();
  r->kind = EXPR_PACKAGE_VALUE_STR;
  r->as.string_value = {s.c_str(), s.size()};
  return true;
}
bool mk(const ExprHostApi *, const ExprPackageValue *a, size_t n,
        ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e, "mkdir"))
    return false;
  std::error_code c;
  std::filesystem::create_directories(p, c);
  if (c) {
    err(e, c.message());
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}
bool move(const ExprHostApi *, const ExprPackageValue *a, size_t n,
          ExprPackageValue *r, ExprPackageStringView *e) {
  if (n != 2 || !a) {
    err(e, "rename expects two strings");
    return false;
  }
  std::string from;
  std::string to;
  if (!stringValue(a[0], from, e, "source path") ||
      !stringValue(a[1], to, e, "destination path"))
    return false;
  std::error_code c;
  std::filesystem::rename(from, to, c);
  if (c) {
    err(e, "could not rename '" + from + "' to '" + to + "': " +
               c.message());
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}

bool copyFile(const ExprHostApi *, const ExprPackageValue *a, size_t n,
              ExprPackageValue *r, ExprPackageStringView *e) {
  if (n != 3 || !a || a[2].kind != EXPR_PACKAGE_VALUE_BOOL) {
    err(e, "copyFile expects source and destination strings and an overwrite bool");
    return false;
  }
  std::string from;
  std::string to;
  if (!stringValue(a[0], from, e, "source path") ||
      !stringValue(a[1], to, e, "destination path"))
    return false;
  const auto options = a[2].as.boolean_value
                           ? std::filesystem::copy_options::overwrite_existing
                           : std::filesystem::copy_options::none;
  std::error_code c;
  const bool copied = std::filesystem::copy_file(from, to, options, c);
  if (c || !copied) {
    err(e, "could not copy '" + from + "' to '" + to + "': " +
               (c ? c.message() : "destination already exists"));
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}

bool fileSize(const ExprHostApi *, const ExprPackageValue *a, size_t n,
              ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e, "fileSize"))
    return false;
  std::error_code c;
  if (!std::filesystem::is_regular_file(p, c)) {
    err(e, c ? "could not inspect '" + p + "': " + c.message()
             : "fileSize requires a regular file: '" + p + "'");
    return false;
  }
  const std::uintmax_t size = std::filesystem::file_size(p, c);
  if (c) {
    err(e, "could not get size of '" + p + "': " + c.message());
    return false;
  }
  if (size > std::numeric_limits<uint64_t>::max()) {
    err(e, "size of '" + p + "' is outside the u64 range");
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_U64;
  r->as.u64_value = static_cast<uint64_t>(size);
  return true;
}

bool rm(const ExprHostApi *, const ExprPackageValue *a, size_t n,
        ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e, "remove"))
    return false;
  std::error_code c;
  std::filesystem::remove_all(p, c);
  if (c) {
    err(e, "could not recursively remove '" + p + "': " + c.message());
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}
constexpr ExprPackageFunctionExport f[] = {
    {"readText", "fn(str) -> str", 1, read},
    {"writeText", "fn(str, str) -> void", 2, write},
    {"exists", "fn(str) -> bool", 1, ex},
    {"isFile", "fn(str) -> bool", 1, file},
    {"isDirectory", "fn(str) -> bool", 1, dir},
    {"workingDirectory", "fn() -> str", 0, cwd},
    {"mkdir", "fn(str) -> void", 1, mk},
    {"rename", "fn(str, str) -> void", 2, move},
    {"copyFile", "fn(str, str, bool) -> void", 3, copyFile},
    {"fileSize", "fn(str) -> u64", 1, fileSize},
    {"remove", "fn(str) -> void", 1, rm}};
constexpr ExprPackageRegistration x = {3, "github", "fs", f, 11, nullptr, 0};
} // namespace
extern "C" const ExprPackageRegistration *exprRegisterPackage() { return &x; }
