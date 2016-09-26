#pragma once


#include <boost/utility/string_ref.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <tuple>
#include <iosfwd>


// Old versions of Boost don't have it. TODO: ifndef it for newer Boost versions
namespace std {
    template<>
    struct hash<boost::string_ref> {
        size_t operator() (const boost::string_ref& s) const noexcept
        {
            return boost::hash_range(s.begin(), s.end());
        }
    };
}

namespace fastfood{
    using string_view = boost::string_ref;

    template<class T>
    using optional = boost::optional<T>;

    template<class... Ts>
    using variant = boost::variant<Ts...>;

    template<class... Ts>
    using tuple = std::tuple<Ts...>;

    //struct null_t {};

    using Field = variant<std::nullptr_t, string_view, double>;


    class Record
    {
    public:
        Field get(const string_view& field) const noexcept
        {
            auto it = m_fields.find(field);

            if (it == m_fields.end())
                return Field{};
            else
                return it->second;
        }

        bool has(const string_view& field) const noexcept
        {
            return m_fields.find(field) != m_fields.end();
        }

        bool empty() const noexcept { return m_fields.empty(); }

    protected:
        std::unordered_map<string_view, Field> m_fields;
    };


    class Predicate
    {
    public:
        virtual ~Predicate() = default;

        virtual bool match(const Record& record) const = 0;

        virtual std::ostream& print(std::ostream& os) const = 0;
    };
    
    using PredicatePtr = std::shared_ptr<Predicate>;
}