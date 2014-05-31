#include <xmpp/adhoc_commands_handler.hpp>
#include <xmpp/xmpp_component.hpp>

#include <logger/logger.hpp>
#include <config/config.hpp>
#include <xmpp/jid.hpp>

#include <iostream>

AdhocCommandsHandler::AdhocCommandsHandler(XmppComponent* xmpp_component):
  xmpp_component(xmpp_component),
  commands{
  {"ping", AdhocCommand({&PingStep1}, "Do a ping", false)},
  {"hello", AdhocCommand({&HelloStep1, &HelloStep2}, "Receive a custom greeting", false)},
  {"disconnect-user", AdhocCommand({&DisconnectUserStep1, &DisconnectUserStep2}, "Disconnect a user from the gateway", true)}
  }
{
}

AdhocCommandsHandler::~AdhocCommandsHandler()
{
}

const std::map<const std::string, const AdhocCommand>& AdhocCommandsHandler::get_commands() const
{
  return this->commands;
}

XmlNode&& AdhocCommandsHandler::handle_request(const std::string& executor_jid, XmlNode command_node)
{
  // TODO check the type of action. Currently it assumes it is always
  // 'execute'.
  std::string action = command_node.get_tag("action");
  if (action.empty())
    action = "execute";
  command_node.del_tag("action");

  Jid jid(executor_jid);

  const std::string node = command_node.get_tag("node");
  auto command_it = this->commands.find(node);
  if (command_it == this->commands.end())
    {
      XmlNode error(ADHOC_NS":error");
      error["type"] = "cancel";
      XmlNode condition(STANZA_NS":item-not-found");
      condition.close();
      error.add_child(std::move(condition));
      error.close();
      command_node.add_child(std::move(error));
    }
  else if (command_it->second.is_admin_only() &&
           Config::get("admin", "") != jid.local + "@" + jid.domain)
    {
      XmlNode error(ADHOC_NS":error");
      error["type"] = "cancel";
      XmlNode condition(STANZA_NS":forbidden");
      condition.close();
      error.add_child(std::move(condition));
      error.close();
      command_node.add_child(std::move(error));
    }
  else
    {
      std::string sessionid = command_node.get_tag("sessionid");
      if (sessionid.empty())
        {                       // create a new session, with a new id
          sessionid = XmppComponent::next_id();
          command_node["sessionid"] = sessionid;
          this->sessions.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(sessionid, executor_jid),
                                 std::forward_as_tuple(command_it->second, executor_jid));
          // TODO add a timed event to have an expiration date that deletes
          // this session. We could have a nasty client starting commands
          // but never finishing the last step, and that would fill the map
          // with dummy sessions.
        }
      auto session_it = this->sessions.find(std::make_pair(sessionid, executor_jid));
      if (session_it == this->sessions.end())
        {
          XmlNode error(ADHOC_NS":error");
          error["type"] = "modify";
          XmlNode condition(STANZA_NS":bad-request");
          condition.close();
          error.add_child(std::move(condition));
          error.close();
          command_node.add_child(std::move(error));
        }
      else
        {
          // execute the step
          AdhocSession& session = session_it->second;
          const AdhocStep& step = session.get_next_step();
          step(this->xmpp_component, session, command_node);
          if (session.remaining_steps() == 0 ||
              session.is_terminated())
            {
              this->sessions.erase(session_it);
              command_node["status"] = "completed";
            }
          else
            {
              command_node["status"] = "executing";
              XmlNode actions("actions");
              XmlNode next("next");
              next.close();
              actions.add_child(std::move(next));
              actions.close();
              command_node.add_child(std::move(actions));
            }
        }
    }
  // TODO remove that once we make sure so session can stay there for ever,
  // by mistake.
  log_debug("Number of existing sessions: " << this->sessions.size());
  return std::move(command_node);
}
