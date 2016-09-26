#pragma once

#include "types.h"
#include <iostream>


namespace fastfood
{
    template<class T, class Enabler = void>
    struct compatible_field_type { using type = T; };

    template<>
    struct compatible_field_type<std::string> { using type = string_view; };


    struct EqualTo
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l == r; }

        static const char *name() noexcept { return "=="; }
    };

    struct NotEqualTo
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l != r; }

        static const char *name() noexcept { return "!="; }
    };

    struct Less
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l < r; }

        static const char *name() noexcept { return "<"; }
    };

    struct LessEqual
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l <= r; }

        static const char *name() noexcept { return "<="; }
    };

    struct Greater
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l > r; }

        static const char *name() noexcept { return ">"; }
    };

    struct GreaterEqual
    {
        template<class T1, class T2>
        bool operator() (const T1& l, const T2& r) const noexcept { return l >= r; }

        static const char *name() noexcept { return ">="; }
    };

    template<class Comp>
    inline const char *comp_name(const Comp&) noexcept
    {
        return Comp::name();
    }

    template<class T>
    struct is_string_field_compatible: std::integral_constant<bool,
    std::is_same<string_view, typename compatible_field_type<T>::type>::value> {};

    template<class T>
    inline typename std::enable_if<is_string_field_compatible<T>::value, std::ostream&>::type
    print(std::ostream& os, const T& val)
    {
        // TODO: escape it back
        return os << "\"" << val << "\"";
    }

    template<class T>
    inline typename std::enable_if<!is_string_field_compatible<T>::value, std::ostream&>::type
    print(std::ostream& os, const T& val)
    {
        return os << val;
    }

    template<class Comp, class T, class FieldType = typename compatible_field_type<T>::type>
    class BinaryFieldPredicate final: public Predicate
    {
    public:
        template<class U, class... CompArgs>
        BinaryFieldPredicate(std::string field, U&& val, CompArgs&&... comp)
        : m_field(std::move(field))
        , m_val(std::forward<U>(val))
        , m_comp(std::forward<CompArgs>(comp)...)
        {}

        bool match(const Record& record) const override
        {
            const auto field = record.get(m_field);

            auto field_val = boost::get<FieldType>(&field);

            return !field_val ? false : m_comp(*field_val, m_val);
        }

        std::ostream& print(std::ostream& os) const override
        {
            os << m_field << " " << comp_name(m_comp) << " ";
            return fastfood::print(os, m_val);
        }

    private:
        std::string m_field;
        T m_val;     // use boost::compressed_pair
        Comp m_comp; //
    };

    class CompositePredicateMixin
    {
    public:
        CompositePredicateMixin() = default;

        CompositePredicateMixin(std::initializer_list<PredicatePtr> predicates): m_predicates(predicates) {}

        template<class InputIt>
        CompositePredicateMixin(InputIt first, InputIt last): m_predicates(first, last) {}

        void push_back(PredicatePtr pred) { m_predicates.push_back(std::move(pred)); }

    protected:
        const std::vector<PredicatePtr>& predicates() const { return m_predicates; }

        std::ostream& print(std::ostream& os, const string_view& op) const
        {
            if (m_predicates.empty())
                return os;

            os << "(";
            auto it = m_predicates.begin(), end = m_predicates.end();
            (*it)->print(os);
            ++it;

            for (; it != end; ++it)
            {
                os << " " << op << " ";
                (*it)->print(os);
            }

            os << ")";

            return os;
        }

    private:
        std::vector<PredicatePtr> m_predicates;
    };

    // OR
    class PredicateDisjunction final: public Predicate, public CompositePredicateMixin
    {
    public:
        PredicateDisjunction() = default;
        PredicateDisjunction(std::initializer_list<PredicatePtr> predicates): CompositePredicateMixin(predicates) {}
        template<class InputIt>
        PredicateDisjunction(InputIt first, InputIt last): CompositePredicateMixin(first, last) {}

        bool match(const Record& record) const override
        {
            for (auto& p: predicates())
                if (p->match(record))
                    return true;

            return false;
        }

        std::ostream& print(std::ostream& os) const override
        {
            return CompositePredicateMixin::print(os, "OR");
        }

    };

    // AND
    class PredicateConjunction final: public Predicate, public CompositePredicateMixin
    {
    public:
        PredicateConjunction() = default;
        PredicateConjunction(std::initializer_list<PredicatePtr> predicates): CompositePredicateMixin(predicates) {}
        template<class InputIt>
        PredicateConjunction(InputIt first, InputIt last): CompositePredicateMixin(first, last) {}

        bool match(const Record& record) const override
        {
            for (auto& p: predicates())
                if (!p->match(record))
                    return false;
            
            return true;
        }
        
        std::ostream& print(std::ostream& os) const override
        {
            return CompositePredicateMixin::print(os, "AND");
        }
    };
}