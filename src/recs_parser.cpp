#include "recs_parser.h"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>
#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_int.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <tuple>


namespace fastfood {
    namespace {
        namespace qi = boost::spirit::qi;
        namespace ns = boost::spirit::standard;

        using ns::no_case;
        using boost::spirit::lit;
        using boost::spirit::long_;
        using boost::spirit::double_;
        using boost::spirit::qi::_1;

        using component_time = std::tuple<long, long>;

        template <typename Iterator, typename Skipper = qi::space_type>
        struct ComponentTimeGramar: qi::grammar<Iterator, component_time(), Skipper>
        {
            ComponentTimeGramar() : ComponentTimeGramar::base_type(t, "component_time")
            {
                t %= no_case[long_ > "msecs" > long_ > "usecs"];
            }

            using It = Iterator;
            using Sk = Skipper;

            qi::rule<It, component_time(), Sk> t;
        };

        const ComponentTimeGramar<string_view::const_iterator> s_component_time_parser;

        inline Field convert_time(string_view time_str)
        {
            component_time t;

            if (!qi::phrase_parse(time_str.begin(), time_str.end(), s_component_time_parser[boost::phoenix::ref(t) = _1], ns::space))
                return time_str;
            else
                return double(std::get<0>(t)) + double(std::get<1>(t)) / 1000.0;
        }

        inline double convert_double(string_view s)
        {
            double res;

            if (!qi::phrase_parse(s.begin(), s.end(), double_[boost::phoenix::ref(res) = _1], ns::space))
                throw std::runtime_error("Can not parse recs stream: invalid floating point number: " + s.to_string());
            return res;
        }

        inline long convert_long(string_view s)
        {
            long res;

            if (!qi::phrase_parse(s.begin(), s.end(), long_[boost::phoenix::ref(res) = _1], ns::space))
                throw std::runtime_error("Can not parse recs stream: invalid long number: " + s.to_string());
            return res;
        }
    }

    inline size_t RecsParser::parse_timing(string_view timings)
    {
        size_t added = 0;

        while (!timings.empty())
        {
            auto pos = timings.find(':');

            if (pos == string_view::npos)
                throw std::runtime_error("Can not parse recs stream: invalid 'Timing' field: " + timings.to_string());

            m_name1 = "timer-";
            m_name1.append(timings.data(), pos);
            m_name1.append("-time");

            m_name2 = "timer-";
            m_name2.append(timings.data(), pos);
            m_name2.append("-count");

            timings.remove_prefix(pos + 1);

            pos = timings.find('/');
            if (pos == string_view::npos)
                throw std::runtime_error("Can not parse recs stream: invalid 'Timing' field: " + timings.to_string());

            auto time_value = timings.substr(0, pos);

            timings.remove_prefix(pos + 1);

            pos = timings.find(',');

            auto count_value = timings.substr(0, pos);

            Name name1{m_name1};

            if (is_interesting_field(name1))
            {
                m_current.set(name1, convert_double(time_value));
                ++added;
            }

            Name name2{m_name2};

            if (is_interesting_field(name2))
            {
                m_current.set(name2, convert_long(count_value));
                ++added;
            }

            if (pos == string_view::npos)
                return added;

            timings.remove_prefix(pos + 1);
        }

        return added;
    }

    inline size_t RecsParser::parse_counters(string_view counters)
    {
        size_t added = 0;

        while (!counters.empty())
        {
            auto pos = counters.find('=');

            if (pos == string_view::npos)
                throw std::runtime_error("Can not parse recs stream: invalid 'Counters' field: " + counters.to_string());

            m_name1 = "counter-";
            m_name1.append(counters.data(), pos);
            m_name1.append("-value");

            counters.remove_prefix(pos + 1);
            pos = counters.find(',');

            auto value = counters.substr(0, pos);

            Name name1{m_name1};

            if (is_interesting_field(name1))
            {
                m_current.set(name1, convert_double(value));
                ++added;
            }

            if (pos == string_view::npos)
                return added;

            counters.remove_prefix(pos + 1);
        }

        return added;
    }

    constexpr auto Line_Buf_Size = 65535;
    constexpr auto Lines_Buf = 1024;

    RecsParser::RecsParser(std::istream& is, const FieldSet& interestingFields)
    : m_stream(is)
    , m_interestingFields(interestingFields)
    , m_current(Lines_Buf)
    {
        m_stream.exceptions(std::ios_base::badbit);

        m_name1.reserve(256);
        m_name2.reserve(256);

        m_lines.resize(Lines_Buf);
        for (auto& i: m_lines)
            i.reserve(Line_Buf_Size);
    }

    bool RecsParser::next()
    {
        m_current.clear();
        m_empty = true;
        m_line = m_lines.begin();

        skip_divider();

        for (;;)
        {
            size_t added = 0;

            std::getline(m_stream, *m_line);

            if (m_stream.eof())
            {
                if (m_empty)
                    return false;
                else
                    throw std::runtime_error("Can not parse recs stream: unexpected EOF");
            }

            if (*m_line == "EOE")
                return true;

            m_empty = false;

            auto name = extract_name();
            auto value = string_view{*m_line}.substr(name.size() + 1);

            if (name == "Timing")
            {
                added = parse_timing(value);
            }
            else if (name == "Counters")
            {
                added = parse_counters(value);
            }
            else
            {
                Name f{name};
                if (!is_interesting_field(f))
                    continue;

                if (name == "UserTime" || name == "SystemTime" || name == "Time")
                {
                    m_current.set(f, convert_time(value));
                    ++added;
                }
                else
                {
                    m_current.set(f, value);
                    ++added;
                }
            }

            if (added)
            {
                ++m_line;

                if (m_line == m_lines.end())
                {
                    m_lines.push_back(std::string());
                    m_lines.back().reserve(Line_Buf_Size);
                }
            }
        }
    }
}
