#include "mehari/CodeGen/ReadySignals.h"
#include "mehari/utils/StringUtils.h"

#include <boost/foreach.hpp>

void ReadySignals::addConsumer(const std::string& ready_signal, const std::string& consumer_ready_signal) {
  (*this)[ready_signal].insert(consumer_ready_signal);
}

void ReadySignals::outputVHDL(std::ostream& stream) {
  BOOST_FOREACH(const item_t& item, *this) {
    stream << "   " << item.first << " <= ";
    if (!item.second.empty()) {
      Seperator sep(" and ");
      BOOST_FOREACH(const std::string& consumer, item.second) {
        stream << sep << consumer;
      }
    } else {
      stream << "'1'";
    }
    stream << ";\n";
  }
}
