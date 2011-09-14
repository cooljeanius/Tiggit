#ifndef _DATA_READER_HPP_
#define _DATA_READER_HPP_

#include "datalist.hpp"
#include <stdexcept>
#include <json/json.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include "filegetter.hpp"

struct TigListReader
{
  std::string filename, channel, desc, location, homepage;

  void fail(const std::string &msg)
  {
    throw std::runtime_error("ERROR parsing '" + filename + "':\n\n" + msg);
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
    using namespace Json;

    Value root;

    {
      std::ifstream inf(file.c_str());
      if(!inf)
        return false;

      Reader reader;
      if(!reader.parse(inf, root))
        return false;
    }

    t.url = root["url"].asString();
    t.launch = root["launch"].asString();
    t.version = root["version"].asString();

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

    Value root;

    {
      std::ifstream inf(file.c_str());
      if(!inf)
        fail("Cannot read file");

      Reader reader;
      if(!reader.parse(inf, root))
        fail(reader.getFormatedErrorMessages());
    }

    // Check file type
    if(root["type"] != "tiglist 1.0")
      fail("Not a valid tiglist");

    channel = root["channel"].asString();
    desc = root["desc"].asString();
    location = root["location"].asString();
    homepage = root["homepage"].asString();

    // This must be present, the rest are optional
    if(channel == "") fail("Missing or invalid channel name");

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
        Value game = root[key];

        // Get and parse tigfile
        DataList::TigInfo ti;
        if(!decodeTigUrl(key, game["tigurl"].asString(), ti) || ti.launch == "")
          continue;

        // Push the game into the list
        data.add(0, key, (chan/key).string(),
                 game["title"].asString(), game["desc"].asString(),
                 game["fpshot"].asString(), game["tigurl"].asString(),
                 ti);
      }
  }
};
#endif