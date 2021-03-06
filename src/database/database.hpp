#ifndef DATABASE_HPP_INCLUDED
#define DATABASE_HPP_INCLUDED

#include <biboumi.h>
#ifdef USE_DATABASE

#include "biboudb.hpp"

#include <memory>

#include <litesql.hpp>

class Database
{
public:
  Database() = default;
  ~Database() = default;

  static void set_verbose(const bool val);

  template<typename PersistentType>
  static size_t count()
  {
    return litesql::select<PersistentType>(Database::get_db()).count();
  }
  /**
   * Return the object from the db. Create it beforehand (with all default
   * values) if it is not already present.
   */
  static db::IrcServerOptions get_irc_server_options(const std::string& owner,
                                                     const std::string& server);

  static void close();

private:
  static std::unique_ptr<db::BibouDB> db;

  static db::BibouDB& get_db();

  Database(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(const Database&) = delete;
  Database& operator=(Database&&) = delete;
};
#endif /* USE_DATABASE */

#endif /* DATABASE_HPP_INCLUDED */
