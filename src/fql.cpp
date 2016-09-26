#include "fql.h"
#include "predicates.h"

//#include <boost/fusion/include/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>

#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/home/qi/string/tst_map.hpp>
#include <boost/phoenix/function.hpp>


#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <tuple>


namespace fastfood {

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

        struct make_query
        {
            Query operator() (const std::vector<std::string>& fields, const PredicatePtr& pred) const
            {
                return Query{fields, pred};
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

    template <typename Iterator, typename Skipper = qi::space_type>
    struct CommonGrammarDefs
    {
        using It = Iterator;

        CommonGrammarDefs()
        {
            boost::phoenix::function<detail::unescape> unescape;

            // Let's use JSON string backslash escaping
            quoted_string = lexeme[
                  lit('"')
                > *((~char_("\\\""))[_val += _1] | (lit('\\') > char_("\\\"/bfnrt")[_val += unescape(_1)]))
                > lit('"')
            ];

            // Probably, better to do it other way around i.e. let it be anything non-space except
            // a floating number can start with (for the first char) or a relation can start with (for the rest chars).
            field_name %= lexeme[char_("A-Za-z_") > *char_("A-Za-z0-9_.:\\/-")];

            value %= (quoted_string | double_);
        }

        qi::rule<It, std::string()> quoted_string;
        qi::rule<It, std::string()> field_name;
        qi::rule<It, Value> value;
    };

    template <typename Iterator, typename Skipper = qi::space_type>
    struct WhereGramar: qi::grammar<Iterator, PredicatePtr(), Skipper>
    {
        WhereGramar(const CommonGrammarDefs<Iterator, Skipper>& common) : WhereGramar::base_type(predicate, "where")
        {
            boost::phoenix::function<detail::make_field_binary_pred> make_field_binary_pred;
            boost::phoenix::function<detail::make_pred_disjunction> make_pred_disjunction;
            boost::phoenix::function<detail::make_pred_conjunction> make_pred_conjunction;
            //boost::phoenix::function<detail::Printer> print;

            rel.add
                ("=" , Relation::eq)
                ("<>", Relation::ne)
                ("<" , Relation::lt)
                ("<=", Relation::le)
                (">" , Relation::gt)
                (">=", Relation::ge)
                ("==", Relation::eq)
                ("!=", Relation::ne)
            ;

            field_predicate = (common.field_name > rel > common.value) [_val = make_field_binary_pred(_1, _2, _3)];

            expr = predicate_disjunction.alias();

            predicate_disjunction = (predicate_conjunction % (lit("or") | "||")) [_val = make_pred_disjunction(_1)];
            predicate_conjunction =
                ((('(' > expr > ')') | field_predicate) % (lit("and") | "&&")) [_val = make_pred_conjunction(_1)];

            predicate %= predicate_disjunction;
        }

        using It = Iterator;
        using Sk = Skipper;

        qi::symbols<char, Relation, qi::tst_map<char, Relation>> rel;
        qi::rule<It, PredicatePtr(), Sk> field_predicate;
        qi::rule<It, PredicatePtr(), Sk> predicate_conjunction;
        qi::rule<It, PredicatePtr(), Sk> predicate_disjunction;
        qi::rule<It, PredicatePtr(), Sk> expr;
        qi::rule<It, PredicatePtr(), Sk> predicate;
    };

    template <typename Iterator, typename Skipper = qi::space_type>
    struct QueryGrammar: qi::grammar<Iterator, Query(), Skipper>
    {
        QueryGrammar(): QueryGrammar::base_type(query, "select"), where(common)
        {
            boost::phoenix::function<detail::make_query> make_query;

            query = no_case[
                (lit("select") > (common.field_name % ',') > "where" > where)[_val = make_query(_1, _2)]
            ];
        }

        using It = Iterator;
        using Sk = Skipper;

        CommonGrammarDefs<It, Sk> common;
        WhereGramar<It, Sk> where;
        qi::rule<Iterator, Query(), Skipper> query;
    };

    template<class InputIt>
    Query parse_query(InputIt first, InputIt last)
    {
        using boost::spirit::qi::_1;
        using boost::phoenix::ref;

        QueryGrammar<InputIt> parser;

        Query res;
        if (!qi::phrase_parse(first, last, parser[boost::phoenix::ref(res) = _1], ns::space))
            throw std::runtime_error("Can not parse 'select' clause. Unparsed: " + std::string{first, last});

        return res;
    }


    Query parse_query(const string_view& s)
    {
        return parse_query(s.begin(), s.end());
    }
}}