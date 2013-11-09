#include <irc/irc_channel.hpp>
#include <utils/make_unique.hpp>

IrcChannel::IrcChannel():
  joined(false),
  self(nullptr)
{
}

void IrcChannel::set_self(const std::string& name)
{
  this->self = std::make_unique<IrcUser>(name);
}

IrcUser* IrcChannel::add_user(const std::string& name)
{
  this->users.emplace_back(std::make_unique<IrcUser>(name));
  return this->users.back().get();
}

IrcUser* IrcChannel::get_self() const
{
  return this->self.get();
}

IrcUser* IrcChannel::find_user(const std::string& name)
{
  IrcUser user(name);
  for (const auto& u: this->users)
    {
      if (u->nick == user.nick)
        return u.get();
    }
  return nullptr;
}

void IrcChannel::remove_user(const IrcUser* user)
{
  for (auto it = this->users.begin(); it != this->users.end(); ++it)
    {
      IrcUser* u = it->get();
      if (u->nick == user->nick)
        {
          this->users.erase(it);
          break ;
        }
    }
}
