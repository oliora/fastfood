#pragma once

#include "types.h"
#include <stdexcept>
#include <iostream>
#include <limits>
#include <vector>


namespace fastfood {

    class RecsParser
    {
    public:
        RecsParser(std::istream& is, const FieldSet& interestingFields);

        const Record& current() const { return m_current; }

        bool next();

    private:
        bool skip_divider()
        {
            auto c = m_stream.get();

            if (c == traits::eof())
                return false;

            if (c != '-')
            {
                m_stream.unget();
                return true;
            }

            // Let's assume it's a delimiter line so ignore it.
            // TODO: It's a bit hacky because we throw away a name=value line where name starts with '-', but let's ignore it for now.
            m_stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return true;
        }

        bool is_interesting_field(Name name) const noexcept
        {
            return m_interestingFields.count(name) != 0;
        }

        string_view extract_name()
        {
            auto pos = m_line->find('=');

            if (pos == std::string::npos)
                throw std::runtime_error("Can not parse recs stream: not a name=value line: " + *m_line);

            return {m_line->data(), pos};
        }

        size_t parse_timing(string_view timings);
        size_t parse_counters(string_view counters);

        using traits = std::istream::traits_type;

        std::istream& m_stream;
        const FieldSet& m_interestingFields;

        MutableRecord m_current;

        // Parsing state and buffers
        std::vector<std::string> m_lines;
        std::vector<std::string>::iterator m_line;
        std::string m_name1, m_name2;
        bool m_empty;
    };
}