#include "fql.h"
#include <iostream>


/*
 Performance improving ideas:
 - ? For field names instead of the string use unique id (of size_t type) that's hashed to itself

 */


using namespace fastfood;


int main(int argc, char *argv[])
{
    try
    {
        //std::string test{"fi-eLd_1 != \"   bla-bla\\n-\\t-\\\" xyz \\\\     -\""};

        //std::string test{"SELECT f1, f2 WHERE Fi-eld_1 != \"   bla-bla-\\t-\\\" xyz \\\\     -\" or field2 <= 12 or f.12 > 42.52 and ghghg < \"45\" or f45 == \"\""};

        std::string test{"SELECT f1, f2, f3 where Field1 = \"bla\" or (f-i <> 1.5)"};

        auto query = fql::parse_query(test);

        for (auto& f: query.m_fields)
            std::cout << f << "\n";

        query.m_where->print(std::cout) << "\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}