#include "fql.h"
#include <iostream>


using namespace fastfood;


int main(int argc, char *argv[])
{
    try
    {
        //std::string test{"fi-eLd_1 != \"   bla-bla\\n-\\t-\\\" xyz \\\\     -\""};

        std::string test{"Fi-eld_1 != \"   bla-bla-\\t-\\\" xyz \\\\     -\" or field2 <= 12 or f.12 > 42.52 and ghghg < \"45\" or f45 == \"\""};

        auto pred = fql::parse_predicate(test);

        pred->print(std::cout) << "\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}