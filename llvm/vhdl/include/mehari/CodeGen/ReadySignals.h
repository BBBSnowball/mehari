#ifndef READY_SIGNALS_H
#define READY_SIGNALS_H

#include <string>
#include <map>
#include <set>
#include <ios>

struct ReadySignals : public std::map< std::string, std::set<std::string> > {
  typedef std::pair< std::string, std::set<std::string> > item_t;

  void addConsumer(const std::string& ready_signal, const std::string& consumer_ready_signal);

  void outputVHDL(std::ostream& stream);
};

#endif /*READY_SIGNALS_H*/
