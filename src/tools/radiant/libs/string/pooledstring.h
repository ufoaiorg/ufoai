
#if !defined(INCLUDED_POOLEDSTRING_H)
#define INCLUDED_POOLEDSTRING_H

#include <map>
#include <string>
#include "generic/static.h"
#include "string/string.h"
#include "container/hashtable.h"
#include "container/hashfunc.h"

/// \brief The string pool class.
class StringPool : public HashTable<std::string&, std::size_t, RawStringHash, RawStringEqual> {
};

/// \brief A string which can be copied with zero memory cost and minimal runtime cost.
///
/// \param PoolContext The string pool context to use.
template<typename PoolContext>
class PooledString {
	StringPool::iterator m_i;
	static StringPool::iterator increment(StringPool::iterator i) {
		++(*i).value;
		return i;
	}
	static StringPool::iterator insert(const std::string& string) {
		StringPool::iterator i = PoolContext::instance().find(string);
		if (i == PoolContext::instance().end()) {
			return PoolContext::instance().insert(string, 1);
		}
		return increment(i);
	}
	static void erase(StringPool::iterator i) {
		if (--(*i).value == 0) {
			PoolContext::instance().erase(i);
		}
	}
public:
	PooledString() : m_i(insert("")) {
	}

	PooledString(const PooledString& other) : m_i(increment(other.m_i)) {
	}

	PooledString(const char* string) : m_i(insert(string)) {
	}

	~PooledString() {
		erase(m_i);
	}

	PooledString& operator=(const PooledString& other) {
		PooledString tmp(other);
		tmp.swap(*this);
		return *this;
	}

	PooledString& operator=(const std::string& string) {
		PooledString tmp(string);
		tmp.swap(*this);
		return *this;
	}

	void swap(PooledString& other) {
		std::swap(m_i, other.m_i);
	}

	bool operator==(const PooledString& other) const {
		return m_i == other.m_i;
	}

	operator const std::string&() const {
		return (*m_i).key;
	}

	operator std::string&() const {
		return (*m_i).key;
	}
};


#endif
