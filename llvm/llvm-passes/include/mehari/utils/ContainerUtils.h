#ifndef CONTAINER_UTILS_H
#define CONTAINER_UTILS_H

#include <map>
#include <set>
#include <iterator>

template<typename KEY, typename VALUE>
inline static bool contains(const std::map<KEY, VALUE>& map, const KEY& key) {
  typename std::map<KEY, VALUE>::const_iterator found = map.find(key);
  return (found != map.end());
}

template<typename KEY, typename VALUE>
inline static const VALUE& getValueOrDefault(
    const std::map<KEY, VALUE>& map, const KEY& key, const VALUE& default_value) {
  typename std::map<KEY, VALUE>::const_iterator found = map.find(key);
  if (found != map.end())
    return found->second;
  else
    return default_value;
}

template<typename KEY, typename VALUE>
inline static const VALUE& getValueOrDefault(const std::map<KEY, VALUE>& map, const KEY& key) {
  return getValueOrDefault(map, key, (VALUE)0);
}

template<typename KEY, typename VALUE>
inline static VALUE* getValueOrNull(std::map<KEY, VALUE>& map, const KEY& key) {
  typename std::map<KEY, VALUE>::iterator found = map.find(key);
  if (found != map.end())
    return &found->second;
  else
    return NULL;
}


template<typename T>
struct SetInserterItem {
  std::set<T>* container;

  SetInserterItem(std::set<T>* container) : container(container) { }

  const T& operator =(const T& value) {
    container->insert(value);
    return value;
  }
};
template<typename T>
struct SetInserter : public std::iterator<std::output_iterator_tag, T, std::ptrdiff_t, T*, T&> {
  std::set<T>* container;

  SetInserter(std::set<T>& container) : container(&container) { }

  SetInserter<T> operator ++() { return *this; }

  SetInserterItem<T> operator *() { return SetInserterItem<T>(container); }
};
template<typename T>
SetInserter<T> set_inserter(std::set<T>& container) { return SetInserter<T>(container); }

#endif /*CONTAINER_UTILS_H*/
