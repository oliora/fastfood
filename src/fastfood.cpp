#include <boost/fusion/include/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>
#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/home/qi/string/tst_map.hpp>
#include <boost/phoenix/function.hpp>

#include <boost/utility/string_ref.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <tuple>


/*
 TODO:
    Features:
    - AND
    - AND has a stronger binding than OR
    - parentheses

    - ? ~ (?) regex check
    - ? LIKE check
    - ? IN check
    - ? BETWEEN check
    - ? IS NULL and IS NOT NULL checks
    - ? negation for any predicate
    - improve parsing errors diagnostic

    Perf:
    - ? For field names instead of the string use unique id (of size_t type) that's hashed to itself

 */


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


namespace fql {
    enum class Relation { eq, ne, gt, ge, lt, le };

    using Value = variant<std::string, double>;

    namespace detail {
        struct unescape
        {
            char operator()(char c) const
            {
                switch (c)
                {
                    default: return c;
                    case 'b': return '\b';
                    case 'f': return '\f';
                    case 'n': return '\n';
                    case 'r': return '\r';
                    case 't': return '\t';
                }
            }
        };

        template<class Comp>
        struct MakeBinaryFieldPredicate: public boost::static_visitor<PredicatePtr>
        {
            MakeBinaryFieldPredicate(const std::string& field): m_field(field) {}

            const std::string& m_field;

            template<class T>
            PredicatePtr operator() (const T& val) const
            {
                return std::make_shared<BinaryFieldPredicate<Comp, T>>(m_field, val);
            }
        };

        struct make_field_binary_pred
        {
            PredicatePtr operator() (const std::string& field, Relation comp, const Value& val) const
            {

                switch (comp)
                {
                case Relation::eq:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<EqualTo>{field}, val);
                case Relation::ne:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<NotEqualTo>{field}, val);
                case Relation::lt:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<Less>{field}, val);
                case Relation::le:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<LessEqual>{field}, val);
                case Relation::gt:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<Greater>{field}, val);
                case Relation::ge:
                    return boost::apply_visitor(MakeBinaryFieldPredicate<GreaterEqual>{field}, val);
                }
            }
        };

        struct make_pred_disjunction
        {
            PredicatePtr operator() (const std::vector<PredicatePtr>& preds) const
            {
                if (preds.size() == 1)
                    return preds.front();
                else
                    return std::make_shared<PredicateDisjunction>(preds.begin(), preds.end());
            }
        };

        struct make_pred_conjunction
        {
            PredicatePtr operator() (const std::vector<PredicatePtr>& preds) const
            {
                if (preds.size() == 1)
                    return preds.front();
                else
                    return std::make_shared<PredicateConjunction>(preds.begin(), preds.end());
            }
        };

        /*struct Printer
        {
            typedef void result_type;

            template<class T>
            void operator()(const T& t) const
            {
                std::cout << t << "\n";
            }
        };*/
    }

    namespace qi = boost::spirit::qi;
    namespace ns = boost::spirit::standard;

    template <typename Iterator, typename Skipper = qi::space_type>
    struct Where: qi::grammar<Iterator, PredicatePtr(), ns::space_type>
    {
        Where() : Where::base_type(predicate, "where")
        {
            using ns::char_;
            using ns::alpha;
            using ns::digit;
            using ns::no_case;
            using ns::string;
            using boost::spirit::no_skip;
            using boost::spirit::lit;
            using boost::spirit::lexeme;
            using boost::spirit::double_;
            using boost::spirit::qi::_1;
            using boost::spirit::qi::_2;
            using boost::spirit::qi::_3;
            using boost::spirit::qi::_val;

            //using namespace detail;

            boost::phoenix::function<detail::unescape> unescape;
            boost::phoenix::function<detail::make_field_binary_pred> make_field_binary_pred;
            boost::phoenix::function<detail::make_pred_disjunction> make_pred_disjunction;
            boost::phoenix::function<detail::make_pred_conjunction> make_pred_conjunction;
            //boost::phoenix::function<detail::Printer> print;

            // Let's use JSON string backslash escaping
            quoted_string = lexeme[
                  lit('"')
                > *((~char_("\\\""))[_val += _1] | (lit('\\') > char_("\\\"/bfnrt")[_val += unescape(_1)]))
                > lit('"')
            ];

            rel.add
                ("==", Relation::eq)
                ("!=", Relation::ne)
                ("<" , Relation::lt)
                ("<=", Relation::le)
                (">" , Relation::gt)
                (">=", Relation::ge)
            ;

            // Probably, better to do it other way around i.e. let it be anything non-space except
            // a floating number can start with (the first char) a relation can start with (the rest chars).
            field_name %= lexeme[char_("A-Za-z_") > *char_("A-Za-z0-9_.,:\\/-")];

            value %= (quoted_string | double_);

            field_predicate = (field_name > rel > value) [_val = make_field_binary_pred(_1, _2, _3)];

            expr = predicate_disjunction.alias();

            predicate_disjunction = (predicate_conjunction % "or") [_val = make_pred_disjunction(_1)];
            predicate_conjunction =
                ((('(' > expr > ')') | field_predicate) % "and") [_val = make_pred_conjunction(_1)];

            predicate %= no_case[predicate_disjunction];
        }

        using It = Iterator;
        using Sk = Skipper;

        qi::symbols<char, Relation, qi::tst_map<char, Relation>> rel;
        qi::rule<It, std::string()> quoted_string;
        qi::rule<It, std::string()> field_name;
        qi::rule<It, Value> value;
        qi::rule<It, PredicatePtr(), Sk> field_predicate;
        qi::rule<It, PredicatePtr(), Sk> predicate_conjunction;
        qi::rule<It, PredicatePtr(), Sk> predicate_disjunction;
        qi::rule<It, PredicatePtr(), Sk> expr;
        qi::rule<It, PredicatePtr(), Sk> predicate;
    };

    template<class InputIt>
    PredicatePtr parse_predicate(InputIt first, InputIt last)
    {
        using boost::spirit::qi::_1;
        using boost::phoenix::ref;

        fastfood::fql::Where<std::string::iterator> parser;

        PredicatePtr res;
        if (!qi::phrase_parse(first, last, parser[boost::phoenix::ref(res) = _1], ns::space))
            throw std::runtime_error("Can not parse 'where' clause");

        return res;
    }
}
}


int main(int argc, char *argv[])
{
    try
    {
        //std::string test{"fi-eLd_1 != \"   bla-bla\\n-\\t-\\\" xyz \\\\     -\""};

        std::string test{"Fi-eld_1 != \"   bla-bla-\\t-\\\" xyz \\\\     -\" or field2 <= 12 or f.12 > 42.52 and ghghg < \"45\" or f45 == \"\""};

        auto pred = fastfood::fql::parse_predicate(test.begin(), test.end());

        pred->print(std::cout) << "\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}