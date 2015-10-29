#include "catch.hpp"

#include <utils/tolower.hpp>
#include <utils/revstr.hpp>
#include <utils/string.hpp>
#include <utils/split.hpp>
#include <utils/xdg.hpp>

TEST_CASE("String split")
{
  std::vector<std::string> splitted = utils::split("a::a", ':', false);
  CHECK(splitted.size() == 2);
  splitted = utils::split("a::a", ':', true);
  CHECK(splitted.size() == 3);
  CHECK(splitted[0] == "a");
  CHECK(splitted[1] == "");
  CHECK(splitted[2] == "a");
  splitted = utils::split("\na", '\n', true);
  CHECK(splitted.size() == 2);
  CHECK(splitted[0] == "");
  CHECK(splitted[1] == "a");
}

TEST_CASE("tolower")
{
  const std::string lowercase = utils::tolower("CoUcOu LeS CoPaiNs ♥");
  CHECK(lowercase == "coucou les copains ♥");

  const std::string ltr = "coucou";
  CHECK(utils::revstr(ltr) == "uocuoc");
}

TEST_CASE("to_bool")
{
  CHECK(to_bool("true"));
  CHECK(!to_bool("trou"));
  CHECK(to_bool("1"));
  CHECK(!to_bool("0"));
  CHECK(!to_bool("-1"));
  CHECK(!to_bool("false"));
}

TEST_CASE("xdg_*_path")
{
  ::unsetenv("XDG_CONFIG_HOME");
  ::unsetenv("HOME");
  std::string res;

  SECTION("Without XDG_CONFIG_HOME nor HOME")
    {
      res = xdg_config_path("coucou.txt");
      CHECK(res == "coucou.txt");
    }
  SECTION("With only HOME")
    {
      ::setenv("HOME", "/home/user", 1);
      res = xdg_config_path("coucou.txt");
      CHECK(res == "/home/user/.config/biboumi/coucou.txt");
    }
  SECTION("With only XDG_CONFIG_HOME")
    {
      ::setenv("XDG_CONFIG_HOME", "/some_weird_dir", 1);
      res = xdg_config_path("coucou.txt");
      CHECK(res == "/some_weird_dir/biboumi/coucou.txt");
    }
  SECTION("With XDG_DATA_HOME")
    {
      ::setenv("XDG_DATA_HOME", "/datadir", 1);
      res = xdg_data_path("bonjour.txt");
      CHECK(res == "/datadir/biboumi/bonjour.txt");
    }
}
