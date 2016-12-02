#include "fql.h"
#include "recs_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
/*
 Performance improving ideas:
 - ? For field names instead of the string use unique id (of size_t type) that's hashed to itself or string + precalculated hash

 */


using namespace fastfood;


int main(int argc, char *argv[])
{
    try
    {
        if (argc <= 1)
            throw std::runtime_error("Usage: fastfood <query> [<filename>]");

        auto query = fql::parse_query(argv[1]);

        FieldSet interestingFields;

        interestingFields.insert(begin(query.m_fields), end(query.m_fields));
        query.m_where->visit_fields([&interestingFields](Name f) { interestingFields.insert(f); });

        std::ifstream input_file;
        std::istream *is = &std::cin;

        if (argc > 1)
        {
            const std::string inputFilename(argv[2]);
            input_file.open(inputFilename, std::ios_base::binary);
            if (!input_file)
                throw std::runtime_error("Can not open file '" + inputFilename + "'");
            is = &input_file;
        }


        RecsParser parser(*is, interestingFields);

        while (parser.next())
        {
            const auto& currentRecord = parser.current();

            if (query.m_where->match(currentRecord))
            {
                // Do something with current record
                for (auto&& r: currentRecord)
                {
                    if (!r.second.which())
                        continue;

                    std::cout << r.first.str() << ": ";
                    boost::apply_visitor(RecordPrinter{std::cout}, r.second);
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}