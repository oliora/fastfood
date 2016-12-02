#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <iosfwd>
#include <boost/utility/string_ref.hpp>
#include <boost/functional/hash.hpp>
#include <boost/thread/shared_mutex.hpp>


// Old versions of Boost don't have it. TODO: ifndef for newer Boost versions
namespace std {
    template<>
    struct hash<boost::string_ref> {
        size_t operator() (const boost::string_ref& s) const noexcept
        {
            return boost::hash_range(s.begin(), s.end());
        }
    };
}


namespace fastfood {
    using string_view = boost::string_ref;

    class Name;

    namespace detail {
        class NameRegistry
        {
            friend class fastfood::Name;

            static NameRegistry& instance() { return s_instance; }

            static const std::string *emptyStr() { return &s_emptyStr; }

            const std::string *get(string_view s)
            {
                if (s.empty())
                    return emptyStr();

                boost::shared_lock<boost::shared_mutex> read_lock(m_mux);

                auto it = m_strings.find(s);

                if (it != m_strings.end())
                    return it->second.get();

                // Add entry
                read_lock.unlock();
                boost::unique_lock<boost::shared_mutex> write_lock(m_mux);

                std::unique_ptr<std::string> entry(new std::string{s.to_string()});
                auto res = entry.get();
                m_strings.emplace(string_view{*res}, std::move(entry));
                return res;
            }

            boost::shared_mutex m_mux;
            std::unordered_map<string_view, std::unique_ptr<std::string>> m_strings;

            static NameRegistry s_instance;
            static const std::string s_emptyStr;
        };
    }

    class Name
    {
    public:
        Name(): m_impl(detail::NameRegistry::emptyStr()) {}
        explicit Name(string_view op): m_impl(detail::NameRegistry::instance().get(op)) {}

        size_t hash() const noexcept { return std::hash<const std::string *>()(m_impl); }

        const std::string& str() const noexcept { return *m_impl; }
        operator const std::string& () const noexcept { return str(); }
        operator string_view () const noexcept { return str(); }

        inline bool operator!= (const Name& r) const noexcept { return m_impl != r.m_impl; }
        inline bool operator== (const Name& r) const noexcept { return m_impl == r.m_impl; }

    private:
        const std::string *m_impl;
    };

    inline bool operator!= (const Name& l, const string_view& r) noexcept { return l.str() != r; }
    inline bool operator== (const Name& l, const string_view& r) noexcept { return l.str() == r; }
    inline bool operator!= (const string_view& l, const Name& r) noexcept { return r.str() != l; }
    inline bool operator== (const string_view& l, const Name& r) noexcept { return r.str() == l; }

    inline std::ostream& operator<< (std::ostream& os, const Name& f)
    {
        os << f.str();
        return os;
    }
}

namespace std {
    template<>
    struct hash<fastfood::Name>
    {
        size_t operator() (const fastfood::Name& f) const noexcept { return f.hash(); }
    };
}
