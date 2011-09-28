#ifndef _DATA_READER_HPP_
#define _DATA_READER_HPP_

#include "datalist.hpp"
#include <stdexcept>
#include <json/json.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include "filegetter.hpp"

#include "config.hpp"

struct TigListReader
{
  std::string filename, channel, desc, location, homepage;

  static void fail(const std::string &msg)
  {
    throw std::runtime_error(msg);
  }

  void failThis(const std::string &msg)
  {
    fail("ERROR parsing '" + filename + "':\n\n" + msg);
  }

  static Json::Value readJson(const std::string &file)
  {
    using namespace Json;

    std::ifstream inf(file.c_str());
    if(!inf)
      fail("Cannot read " + file);

    Value root;

    Reader reader;
    if(!reader.parse(inf, root))
      fail(reader.getFormatedErrorMessages() + " reading " + file);

    return root;
  }

  // Process URL. This is a poor mans urlencode, but does the trick
  // for 99% of cases.
  static std::string URL(std::string res)
  {
    for(int i=0; i<res.size(); i++)
      {
        switch(res[i])
          {
          case ' ': res.replace(i, 1, "%20"); break;
          case '!': res.replace(i, 1, "%21"); break;
          case '"': res.replace(i, 1, "%22"); break;
          case '\'': res.replace(i, 1, "%27"); break;
          case '(': res.replace(i, 1, "%28"); break;
          case ')': res.replace(i, 1, "%29"); break;
          case '+': res.replace(i, 1, "%2B"); break;
          }
      }
    return res;
  }

  // Get the tigfile, from local cache if possible. Returns "" if the
  // fetch fails.
  std::string getTigFile(const std::string &name, const std::string &url,
                         bool cacheFirst = true)
  {
    // Not allowed on a non-initialized list object.
    assert(filename != "");

    boost::filesystem::path dir;

    dir = "tigfiles";
    dir /= channel;
    dir /= name + ".tig";

    try { return get.getCache(dir.string(), url, cacheFirst); }
    catch(...) {}
    return ""; // Empty string on error
  }

  /* Decode a .tig file. Returns false if the file is invalid or
     should otherwise be rejected.
   */
  static bool decodeTigFile(const std::string &file, DataList::TigInfo &t)
  {
    Json::Value root;

    try { root = readJson(file); }
    catch(...) { return false; }

    t.url = URL(root["url"].asString());
    t.launch = root["launch"].asString();
    t.subdir = root["subdir"].asString();
    t.version = root["version"].asString();

    t.title = root["title"].asString();
    t.desc = root["desc"].asString();
    t.shot = root["shot"].asString();
    t.shot80x50 = root["shot80x50"].asString();
    t.location = root["location"].asString();
    t.devname = root["devname"].asString();
    t.homepage = root["homepage"].asString();

    if(t.url == "")
      return false;

    return true;
  }

  // Combines getTigFile and decodeTigFile into one
  bool decodeTigUrl(const std::string &name,
                    const std::string &url,
                    DataList::TigInfo &data,
                    bool cacheFirst = true)
  {
    // Get the tigfile
    std::string tigf = getTigFile(name, url, cacheFirst);

    // Skip missing files
    if(tigf == "")
      return false;

    // Parse it
    DataList::TigInfo ti;
    return decodeTigFile(tigf, data);
  }

  void loadData(const std::string &file, DataList &data)
  {
    using namespace Json;
    filename = file;

    Value root = readJson(file);

    // Check file type
    if(root["type"] != "tiglist 1.1")
      failThis("Not a valid tiglist");

    channel = root["channel"].asString();
    desc = root["desc"].asString();
    location = URL(root["location"].asString());
    homepage = root["homepage"].asString();

    // This must be present, the rest are optional
    if(channel == "") failThis("Missing or invalid channel name");

    // Traverse the list
    root = root["list"];

    boost::filesystem::path chan = channel;

    // We have to do it this way because the jsoncpp iterators are
    // b0rked.
    Value::Members keys = root.getMemberNames();
    Value::Members::iterator it;
    for(it = keys.begin(); it != keys.end(); it++)
      {
        const std::string &key = *it;
        std::string tigurl = URL(root[key].asString());

        // Get and parse tigfile
        DataList::TigInfo ti;

        // Prefer using cached .tig files, unless config tells us we
        // need to update.
        bool cacheFirst = !conf.updateTigs;

        if(!decodeTigUrl(key, tigurl, ti, cacheFirst) ||
           ti.launch == "" || ti.title == "")
          // Skip the file if not enough info was found.
          continue;

        /* TODO: Use this later more generically, also for the list
           file itself.

        // If the location has changed, redirect to the new url
        for(int i=0; i<10; i++)
          {
            if(ti.location == "" || ti.location == tigurl)
              break;

            DataList::TigInfo ti2;

            // Fetch and decode the new location
            if(!decodeTigUrl(key, ti.location, ti2, true))
              break;

            // We got a new valid file. Override existing info.
            tigurl = ti.location;
            ti = ti2;
          }
        */

        // Push the game into the list
        data.add(0, key, (chan/key).string(),
                 ti.title, ti.desc, tigurl, ti);
      }
  }
};
#endif
