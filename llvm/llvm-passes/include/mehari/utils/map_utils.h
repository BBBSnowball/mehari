#ifndef MAP_UTILS_H
#define MAP_UTILS_H

#include <map>

template<typename KEY, typename VALUE>
inline const VALUE& contains(const std::map<KEY, VALUE>& map, const KEY& key) {
  typename std::map<KEY, VALUE>::const_iterator found = map.find(key);
  return (found != map.end());
}

template<typename KEY, typename VALUE>
inline const VALUE& getValueOrDefault(const std::map<KEY, VALUE>& map, const KEY& key, const VALUE& default_value) {
  typename std::map<KEY, VALUE>::const_iterator found = map.find(key);
  if (found != map.end())
    return found->second;
  else
    return default_value;
}

template<typename KEY, typename VALUE>
inline VALUE* getValueOrNull(std::map<KEY, VALUE>& map, const KEY& key) {
  typename std::map<KEY, VALUE>::iterator found = map.find(key);
  if (found != map.end())
    return &found->second;
  else
    return NULL;
}

#endif /*MAP_UTILS_H*/
