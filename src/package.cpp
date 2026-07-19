#include "NativePackageAPI.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
namespace {
void err(ExprPackageStringView *e, const std::string &s) {
  if (e)
    *e = {s.c_str(), s.size()};
}
bool one(const ExprPackageValue *a, size_t n, std::string &o,
         ExprPackageStringView *e) {
  if (n != 1 || a[0].kind != EXPR_PACKAGE_VALUE_STR) {
    err(e, "expected one str argument");
    return false;
  }
  o.assign(a[0].as.string_value.data, a[0].as.string_value.length);
  return true;
}
bool read(const ExprHostApi *, const ExprPackageValue *a, size_t n,
          ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e))
    return false;
  std::ifstream f(p);
  if (!f) {
    err(e, "could not read '" + p + "'");
    return false;
  }
  static thread_local std::string s;
  s.assign(std::istreambuf_iterator<char>(f), {});
  r->kind = EXPR_PACKAGE_VALUE_STR;
  r->as.string_value = {s.c_str(), s.size()};
  return true;
}
bool write(const ExprHostApi *, const ExprPackageValue *a, size_t n,
           ExprPackageValue *r, ExprPackageStringView *e) {
  if (n != 2 || a[0].kind != EXPR_PACKAGE_VALUE_STR ||
      a[1].kind != EXPR_PACKAGE_VALUE_STR) {
    err(e, "writeText expects path and content strings");
    return false;
  }
  std::string p(a[0].as.string_value.data, a[0].as.string_value.length);
  std::ofstream f(p);
  if (!f) {
    err(e, "could not write '" + p + "'");
    return false;
  }
  f.write(a[1].as.string_value.data, a[1].as.string_value.length);
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
  if (!one(a, n, p, e))
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
  static thread_local std::string s = std::filesystem::current_path().string();
  r->kind = EXPR_PACKAGE_VALUE_STR;
  r->as.string_value = {s.c_str(), s.size()};
  return true;
}
bool mk(const ExprHostApi *, const ExprPackageValue *a, size_t n,
        ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e))
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
  if (n != 2 || a[0].kind != EXPR_PACKAGE_VALUE_STR ||
      a[1].kind != EXPR_PACKAGE_VALUE_STR) {
    err(e, "rename expects two strings");
    return false;
  }
  std::error_code c;
  std::filesystem::rename(
      std::string(a[0].as.string_value.data, a[0].as.string_value.length),
      std::string(a[1].as.string_value.data, a[1].as.string_value.length), c);
  if (c) {
    err(e, c.message());
    return false;
  }
  r->kind = EXPR_PACKAGE_VALUE_NULL;
  return true;
}
bool rm(const ExprHostApi *, const ExprPackageValue *a, size_t n,
        ExprPackageValue *r, ExprPackageStringView *e) {
  std::string p;
  if (!one(a, n, p, e))
    return false;
  std::error_code c;
  std::filesystem::remove_all(p, c);
  if (c) {
    err(e, c.message());
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
    {"remove", "fn(str) -> void", 1, rm}};
constexpr ExprPackageRegistration x = {3, "github", "fs", f, 9, nullptr, 0};
} // namespace
extern "C" const ExprPackageRegistration *exprRegisterPackage() { return &x; }
