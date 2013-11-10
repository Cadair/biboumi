#include <utils/make_unique.hpp>

#include <xmpp/xmpp_component.hpp>
#include <xmpp/jid.hpp>

#include <iostream>

// CryptoPP
#include <filters.h>
#include <hex.h>
#include <sha.h>

#define STREAM_NS        "http://etherx.jabber.org/streams"
#define COMPONENT_NS     "jabber:component:accept"
#define MUC_NS           "http://jabber.org/protocol/muc"
#define MUC_USER_NS      MUC_NS"#user"
#define DISCO_NS         "http://jabber.org/protocol/disco"
#define DISCO_ITEMS_NS   DISCO_NS"#items"
#define DISCO_INFO_NS    DISCO_NS"#info"


XmppComponent::XmppComponent(const std::string& hostname, const std::string& secret):
  served_hostname(hostname),
  secret(secret),
  authenticated(false)
{
  this->parser.add_stream_open_callback(std::bind(&XmppComponent::on_remote_stream_open, this,
                                                  std::placeholders::_1));
  this->parser.add_stanza_callback(std::bind(&XmppComponent::on_stanza, this,
                                                  std::placeholders::_1));
  this->parser.add_stream_close_callback(std::bind(&XmppComponent::on_remote_stream_close, this,
                                                  std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":handshake",
                                std::bind(&XmppComponent::handle_handshake, this,std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":presence",
                                std::bind(&XmppComponent::handle_presence, this,std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":message",
                                std::bind(&XmppComponent::handle_message, this,std::placeholders::_1));
}

XmppComponent::~XmppComponent()
{
}

void XmppComponent::start()
{
  this->connect("127.0.0.1", "5347");
}

void XmppComponent::send_stanza(const Stanza& stanza)
{
  std::cout << "====== Sending ========" << std::endl;
  std::cout << stanza.to_string() << std::endl;
  this->send_data(stanza.to_string());
}

void XmppComponent::on_connected()
{
  std::cout << "connected to XMPP server" << std::endl;
  XmlNode node("stream:stream", nullptr);
  node["xmlns"] = COMPONENT_NS;
  node["xmlns:stream"] = STREAM_NS;
  node["to"] = this->served_hostname;
  this->send_stanza(node);
}

void XmppComponent::on_connection_close()
{
  std::cout << "XMPP server closed connection" << std::endl;
}

void XmppComponent::parse_in_buffer()
{
  this->parser.feed(this->in_buf.data(), this->in_buf.size(), false);
  this->in_buf.clear();
}

void XmppComponent::on_remote_stream_open(const XmlNode& node)
{
  std::cout << "====== DOCUMENT_OPEN =======" << std::endl;
  std::cout << node.to_string() << std::endl;
  try
    {
      this->stream_id = node["id"];
    }
  catch (const AttributeNotFound& e)
    {
      std::cout << "Error: no attribute 'id' found" << std::endl;
      this->send_stream_error("bad-format", "missing 'id' attribute");
      this->close_document();
      return ;
    }

  // Try to authenticate
  CryptoPP::SHA1 sha1;
  std::string digest;
  CryptoPP::StringSource foo(this->stream_id + this->secret, true,
                      new CryptoPP::HashFilter(sha1,
                          new CryptoPP::HexEncoder(
                              new CryptoPP::StringSink(digest), false)));
  Stanza handshake("handshake", nullptr);
  handshake.set_inner(digest);
  handshake.close();
  this->send_stanza(handshake);
}

void XmppComponent::on_remote_stream_close(const XmlNode& node)
{
  std::cout << "====== DOCUMENT_CLOSE =======" << std::endl;
  std::cout << node.to_string() << std::endl;
}

void XmppComponent::on_stanza(const Stanza& stanza)
{
  std::cout << "=========== STANZA ============" << std::endl;
  std::cout << stanza.to_string() << std::endl;
  std::function<void(const Stanza&)> handler;
  try
    {
      handler = this->stanza_handlers.at(stanza.get_name());
    }
  catch (const std::out_of_range& exception)
    {
      std::cout << "No handler for stanza of type " << stanza.get_name() << std::endl;
      return;
    }
  handler(stanza);
}

void XmppComponent::send_stream_error(const std::string& name, const std::string& explanation)
{
  XmlNode node("stream:error", nullptr);
  XmlNode error(name, nullptr);
  error["xmlns"] = STREAM_NS;
  if (!explanation.empty())
    error.set_inner(explanation);
  error.close();
  node.add_child(std::move(error));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::close_document()
{
  std::cout << "====== Sending ========" << std::endl;
  std::cout << "</stream:stream>" << std::endl;
  this->send_data("</stream:stream>");
}

void XmppComponent::handle_handshake(const Stanza& stanza)
{
  (void)stanza;
  this->authenticated = true;
}

void XmppComponent::handle_presence(const Stanza& stanza)
{
  Bridge* bridge = this->get_user_bridge(stanza["from"]);
  Jid to(stanza["to"]);
  Iid iid(to.local);
  std::string type;
  try {
    type = stanza["type"];
  }
  catch (const AttributeNotFound&) {}

  if (!iid.chan.empty() && !iid.chan.empty())
    { // presence toward a MUC that corresponds to an irc channel
      if (type.empty())
        {
          const std::string own_nick = bridge->get_own_nick(iid);
          if (!own_nick.empty() && own_nick != to.resource)
            bridge->send_irc_nick_change(iid, to.resource);
          else
            bridge->join_irc_channel(iid, to.resource);
        }
      else if (type == "unavailable")
        {
          XmlNode* status = stanza.get_child(MUC_USER_NS":status");
          bridge->leave_irc_channel(std::move(iid), status ? std::move(status->get_inner()) : "");
        }
    }
}

void XmppComponent::handle_message(const Stanza& stanza)
{
  Bridge* bridge = this->get_user_bridge(stanza["from"]);
  Jid to(stanza["to"]);
  Iid iid(to.local);
  XmlNode* body = stanza.get_child(COMPONENT_NS":body");
  if (stanza["type"] == "groupchat")
    {
      if (to.resource.empty())
        if (body && !body->get_inner().empty())
          bridge->send_channel_message(iid, body->get_inner());
    }
  else
    {
      if (body && !body->get_inner().empty())
        bridge->send_private_message(iid, body->get_inner());
    }
}

Bridge* XmppComponent::get_user_bridge(const std::string& user_jid)
{
  try
    {
      return this->bridges.at(user_jid).get();
    }
  catch (const std::out_of_range& exception)
    {
      this->bridges.emplace(user_jid, std::make_unique<Bridge>(user_jid, this, this->poller));
      return this->bridges.at(user_jid).get();
    }
}

void XmppComponent::send_message(const std::string& from, const std::string& body, const std::string& to)
{
  XmlNode node("message");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname;
  XmlNode body_node("body");
  body_node.set_inner(body);
  body_node.close();
  node.add_child(std::move(body_node));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_user_join(const std::string& from, const std::string& nick, const std::string& to)
{
  XmlNode node("presence");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname + "/" + nick;

  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;

  // TODO: put real values here
  XmlNode item("item");
  item["affiliation"] = "member";
  item["role"] = "participant";
  item.close();
  x.add_child(std::move(item));
  x.close();
  node.add_child(std::move(x));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_self_join(const std::string& from, const std::string& nick, const std::string& to)
{
  XmlNode node("presence");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname + "/" + nick;

  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;

  // TODO: put real values here
  XmlNode item("item");
  item["affiliation"] = "member";
  item["role"] = "participant";
  item.close();
  x.add_child(std::move(item));

  XmlNode status("status");
  status["code"] = "110";
  status.close();
  x.add_child(std::move(status));

  x.close();

  node.add_child(std::move(x));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_topic(const std::string& from, const std::string& topic, const std::string& to)
{
  XmlNode message("message");
  message["to"] = to;
  message["from"] = from + "@" + this->served_hostname;
  message["type"] = "groupchat";
  XmlNode subject("subject");
  subject.set_inner(topic);
  subject.close();
  message.add_child(std::move(subject));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_message(const std::string& muc_name, const std::string& nick, const std::string body_str, const std::string& jid_to)
{
  Stanza message("message");
  message["to"] = jid_to;
  message["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  message["type"] = "groupchat";
  XmlNode body("body");
  body.set_inner(body_str);
  body.close();
  message.add_child(std::move(body));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_leave(std::string&& muc_name, std::string&& nick, std::string&& message, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  presence["type"] = "unavailable";
  if (!message.empty() || self)
    {
      XmlNode status("status");
      if (!message.empty())
        status.set_inner(std::move(message));
      if (self)
        status["code"] = "110";
      status.close();
      presence.add_child(std::move(status));
    }
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_nick_change(const std::string& muc_name, const std::string& old_nick, const std::string& new_nick, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + old_nick;
  presence["type"] = "unavailable";
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  XmlNode item("item");
  item["nick"] = new_nick;
  item.close();
  x.add_child(std::move(item));
  XmlNode status("status");
  status["code"] = "303";
  status.close();
  x.add_child(std::move(status));
  if (self)
    {
      XmlNode status2("status");
      status2["code"] = "110";
      status2.close();
      x.add_child(std::move(status2));
    }
  x.close();
  presence.add_child(std::move(x));
  presence.close();
  this->send_stanza(presence);

  if (self)
    this->send_self_join(muc_name, new_nick, jid_to);
  else
    this->send_user_join(muc_name, new_nick, jid_to);
}
