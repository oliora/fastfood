#pragma once

#include "name.h"
#include <boost/utility/string_ref.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <tuple>
#include <iosfwd>
#include <limits>
#include <math.h>


namespace fastfood {
    template<class T>
    using optional = boost::optional<T>;

    template<class... Ts>
    using variant = boost::variant<Ts...>;

    template<class... Ts>
    using tuple = std::tuple<Ts...>;

    using Field = variant<std::nullptr_t, string_view, double>; // TODO: add uint64_t when one is fully supported by JSON

    class Record
    {
    protected:
        using Map = std::unordered_map<Name, Field>;

    public:
        using const_iterator = Map::const_iterator; // TODO: filter NULL fields

        Field get(Name field) const noexcept
        {
            auto it = m_fields.find(field);

            if (it == m_fields.end())
                return Field{};
            else
                return it->second;
        }

        bool has(Name field) const noexcept
        {
            auto it = m_fields.find(field);

            return it != m_fields.end() && it->second != Field{};
        }

        bool empty() const noexcept { return m_fields.empty(); }

        const_iterator begin() const noexcept { return m_fields.begin(); }
        const_iterator end() const noexcept { return m_fields.end(); }

    protected:
        Map m_fields;
    };

    class MutableRecord: public Record
    {
    public:
        MutableRecord() = default;

        explicit MutableRecord(size_t reserve, float max_load_factor = 1.0)
        {
            m_fields.max_load_factor(max_load_factor);
            m_fields.reserve(reserve);
        }

        template<class Value>
        bool set(Name name, Value &&value)
        {
            auto it = m_fields.find(name);

            if (it == m_fields.end())
            {
                m_fields.emplace(name, std::forward<Value>(value));
                return true;
            }
            else
            {
                auto res = it->second == Field{};
                it->second = std::forward<Value>(value);
                return res;
            }
        }

        void clear()
        {
            for (auto&& i: m_fields)
                i.second = Field{};
        }
    };

    struct RecordPrinter: boost::static_visitor<std::ostream&>
    {
        explicit RecordPrinter(std::ostream& os): m_os(os) {}

        std::ostream& operator() (std::nullptr_t) const { return m_os << "<NULL>"; }
        std::ostream& operator() (string_view s) const { return m_os << s; }
        std::ostream& operator() (double d) const { return m_os << d; }

        std::ostream& m_os;
    };

    // TODO: use flat_unordered_set
    using FieldSet = std::unordered_set<Name>;

    class Predicate
    {
    public:
        virtual ~Predicate() = default;

        virtual bool match(const Record& record) const = 0;

        virtual std::ostream& print(std::ostream& os) const = 0;

        virtual void visit_fields(const std::function<void(Name)>& visitor) const = 0;
    };

    using PredicatePtr = std::shared_ptr<Predicate>;

    namespace aggregators {
        namespace detail {
            inline double get_double(const Field& f, double def = 0) noexcept
            {
                auto x = boost::get<double>(&f);
                return x ? *x : def;
            }

            struct Sum
            {
                using type = double;

                type operator() (type a, const Field& b) const noexcept { return a + get_double(b); }

                static constexpr type initial_value() noexcept { return 0; }
            };

            struct Min
            {
                using type = double;

                type operator() (type a, const Field& b) const noexcept { return std::min(a, get_double(b, initial_value())); }

                static constexpr type initial_value() noexcept { return std::numeric_limits<type>::quiet_NaN(); }
            };

            struct Max
            {
                using type = double;

                type operator() (type a, const Field& b) const noexcept { return std::max(a, get_double(b, initial_value())); }

                static constexpr type initial_value() noexcept { return std::numeric_limits<type>::quiet_NaN(); }
            };

            struct Count
            {
                using type = double; // TODO: use uint64_t

                type operator() (type a, const Field&) const noexcept { return a + 1; }

                static constexpr type initial_value() noexcept { return 0; }
            };

            struct Avg
            {
                using type = std::pair<double, double>; // (sum, count). TODO: use uint64_t for count

                type operator() (const type& a, const Field& b) const
                {
                    auto x = boost::get<double>(&b);
                    if (x)
                        return {a.first + *x, a.second + 1};
                    else
                        return a;
                }

                static type initial_value() noexcept { return {0, 0}; }
            };
        }
    }
}