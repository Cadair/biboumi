/**
 * Just a very simple test suite, by hand, using assert()
 */

#include <xmpp/xmpp_parser.hpp>
#include <utils/encoding.hpp>
#include <logger/logger.hpp>
#include <config/config.hpp>
#include <bridge/colors.hpp>
#include <utils/tolower.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <irc/iid.hpp>
#include <string.h>

#include <iostream>
#include <vector>

#include <assert.h>

static const std::string color("[35m");
static const std::string reset("[m");

int main()
{
  /**
   * Encoding
   */
  std::cout << color << "Testing encoding…" << reset << std::endl;
  const char* valid = "C̡͔͕̩͙̽ͫ̈́ͥ̿̆ͧ̚r̸̩̘͍̻͖̆͆͛͊̉̕͡o͇͈̳̤̱̊̈͢q̻͍̦̮͕ͥͬͬ̽ͭ͌̾ͅǔ͉͕͇͚̙͉̭͉̇̽ȇ͈̮̼͍͔ͣ͊͞͝ͅ ͫ̾ͪ̓ͥ̆̋̔҉̢̦̠͈͔̖̲̯̦ụ̶̯͐̃̋ͮ͆͝n̬̱̭͇̻̱̰̖̤̏͛̏̿̑͟ë́͐҉̸̥̪͕̹̻̙͉̰ ̹̼̱̦̥ͩ͑̈́͑͝ͅt͍̥͈̹̝ͣ̃̔̈̔ͧ̕͝ḙ̸̖̟̙͙ͪ͢ų̯̞̼̲͓̻̞͛̃̀́b̮̰̗̩̰̊̆͗̾̎̆ͯ͌͝.̗̙͎̦ͫ̈́ͥ͌̈̓ͬ";
  assert(utils::is_valid_utf8(valid) == true);
  const char* invalid = "\xF0\x0F";
  assert(utils::is_valid_utf8(invalid) == false);
  const char* invalid2 = "\xFE\xFE\xFF\xFF";
  assert(utils::is_valid_utf8(invalid2) == false);

  std::string in = "Biboumi ╯°□°）╯︵ ┻━┻";
  std::cout << in << std::endl;
  assert(utils::is_valid_utf8(in.c_str()) == true);
  std::string res = utils::convert_to_utf8(in, "UTF-8");
  assert(utils::is_valid_utf8(res.c_str()) == true && res == in);

  std::string original_utf8("couc¥ou");
  std::string original_latin1("couc\xa5ou");

  // When converting back to utf-8
  std::string from_latin1 = utils::convert_to_utf8(original_latin1.c_str(), "ISO-8859-1");
  assert(from_latin1 == original_utf8);

  // Check the behaviour when the decoding fails (here because we provide a
  // wrong charset)
  std::string from_ascii = utils::convert_to_utf8(original_latin1, "US-ASCII");
  assert(from_ascii == "couc�ou");
  std::cout << from_ascii << std::endl;

  std::string without_ctrl_char("𤭢€¢$");
  assert(utils::remove_invalid_xml_chars(without_ctrl_char) == without_ctrl_char);
  assert(utils::remove_invalid_xml_chars(in) == in);
  assert(utils::remove_invalid_xml_chars("\acouco\u0008u\uFFFEt\uFFFFe\r\n♥") == "coucoute\r\n♥");

  /**
   * Utils
   */
  std::cout << color << "Testing utils…" << reset << std::endl;
  std::vector<std::string> splitted = utils::split("a::a", ':', false);
  assert(splitted.size() == 2);
  splitted = utils::split("a::a", ':', true);
  assert(splitted.size() == 3);
  assert(splitted[0] == "a");
  assert(splitted[1] == "");
  assert(splitted[2] == "a");
  splitted = utils::split("\na", '\n', true);
  assert(splitted.size() == 2);
  assert(splitted[0] == "");
  assert(splitted[1] == "a");

  const std::string lowercase = utils::tolower("CoUcOu LeS CoPaiNs ♥");
  std::cout << lowercase << std::endl;
  assert(lowercase == "coucou les copains ♥");

  /**
   * XML parsing
   */
  std::cout << color << "Testing XML parsing…" << reset << std::endl;
  XmppParser xml;
  const std::string doc = "<stream xmlns='stream_ns'><stanza b='c'>inner<child1><grandchild/></child1><child2 xmlns='child2_ns'/>tail</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        std::cout << stanza.to_string() << std::endl;
        assert(stanza.get_name() == "stream_ns:stanza");
        assert(stanza.get_tag("b") == "c");
        assert(stanza.get_inner() == "inner");
        assert(stanza.get_tail() == "");
        assert(stanza.get_child("stream_ns:child1") != nullptr);
        assert(stanza.get_child("stream_ns:child2") == nullptr);
        assert(stanza.get_child("child2_ns:child2") != nullptr);
        assert(stanza.get_child("child2_ns:child2")->get_tail() == "tail");
      });
  xml.feed(doc.data(), doc.size(), true);

  const std::string doc2 = "<stream xmlns='s'><stanza>coucou\r\n\a</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        std::cout << stanza.to_string() << std::endl;
        assert(stanza.get_inner() == "coucou\r\n");
      });
  xml.feed(doc2.data(), doc.size(), true);

  /**
   * XML escape/escape
   */
  std::cout << color << "Testing XML escaping…" << reset << std::endl;
  const std::string unescaped = "'coucou'<cc>/&\"gaga\"";
  assert(xml_escape(unescaped) == "&apos;coucou&apos;&lt;cc&gt;/&amp;&quot;gaga&quot;");
  assert(xml_unescape(xml_escape(unescaped)) == unescaped);

  /**
   * Colors conversion
   */
  std::cout << color << "Testing IRC colors conversion…" << reset << std::endl;
  std::unique_ptr<XmlNode> xhtml;
  std::string cleaned_up;

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("bold");
  std::cout << xhtml->to_string() << std::endl;
  assert(xhtml && xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'><span style='font-weight:bold;'>bold</span></body>");

  std::tie(cleaned_up, xhtml) =
    irc_format_to_xhtmlim("normalboldunder-and-boldbold normal"
                          "5red,5default-on-red10,2cyan-on-blue");
  assert(xhtml);
  assert(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>normal<span style='font-weight:bold;'>bold</span><span style='font-weight:bold;text-decoration:underline;'>under-and-bold</span><span style='font-weight:bold;'>bold</span> normal<span style='color:red;'>red</span><span style='background-color:red;'>default-on-red</span><span style='color:cyan;background-color:blue;'>cyan-on-blue</span></body>");
  assert(cleaned_up == "normalboldunder-and-boldbold normalreddefault-on-redcyan-on-blue");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("normal");
  assert(!xhtml && cleaned_up == "normal");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("");
  assert(xhtml && !xhtml->has_children() && cleaned_up.empty());

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",a");
  assert(xhtml && !xhtml->has_children() && cleaned_up == "a");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",");
  assert(xhtml && !xhtml->has_children() && cleaned_up.empty());

  /**
   * JID parsing
   */
  std::cout << color << "Testing JID parsing…" << reset << std::endl;
  // Full JID
  Jid jid1("♥@ツ.coucou/coucou@coucou/coucou");
  std::cout << jid1.local << "@" << jid1.domain << "/" << jid1.resource << std::endl;
  assert(jid1.local == "♥");
  assert(jid1.domain == "ツ.coucou");
  assert(jid1.resource == "coucou@coucou/coucou");

  // Domain and resource
  Jid jid2("ツ.coucou/coucou@coucou/coucou");
  std::cout << jid2.local << "@" << jid2.domain << "/" << jid2.resource << std::endl;
  assert(jid2.local == "");
  assert(jid2.domain == "ツ.coucou");
  assert(jid2.resource == "coucou@coucou/coucou");

  // Jidprep
  const std::string& badjid("~zigougou@EpiK-7D9D1FDE.poez.io/Boujour/coucou/slt");
  const std::string correctjid = jidprep(badjid);
  assert(correctjid == "~zigougou@epik-7d9d1fde.poez.io/Boujour/coucou/slt");

  const std::string& badjid2("Zigougou@poez.io");
  const std::string correctjid2 = jidprep(badjid2);
  std::cout << correctjid2 << std::endl;
  assert(correctjid2 == "zigougou@poez.io");

  /**
   * Config
   */
  std::cout << color << "Testing config…" << reset << std::endl;
  Config::filename = "test.cfg";
  Config::file_must_exist = false;
  Config::set("coucou", "bonjour");
  Config::close();

  bool error = false;
  try
    {
      Config::file_must_exist = true;
      assert(Config::get("coucou", "") == "bonjour");
      assert(Config::get("does not exist", "default") == "default");
      Config::close();
    }
  catch (const std::ios::failure& e)
    {
      error = true;
    }
  assert(error == false);

  Config::set("log_level", "3");
  Config::set("log_file", "");

  log_debug("coucou");
  log_info("coucou");
  log_warning("coucou");
  log_error("coucou");

  return 0;
}
