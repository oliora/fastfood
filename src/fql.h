#include "types.h"
#include <string>
#include <vector>

/*
 Query w/o aggregation (i.e. reduce):
    SELECT fld1, fld2 WHERE fld1 = "val1" AND (fld2 >= 1.5 OR fld3 <> "start")

 TODO:
    v1:
    - Aggregation
    - Sorting
    - regex match =~ (other name? MATCH, ~) operator
    v2:
    - IN operator
    - BETWEEN operator
    - LIKE operator (for simpler than regex 'starts with')
    - improve parsing errors diagnostic
    - ? All fields (*)
    - ? IS NULL, IS NOT NULL operators
    - ? NOT (or !)
 */


namespace fastfood { namespace fql {

    struct Query
    {
        std::vector<std::string> m_fields;
        PredicatePtr m_where;
    };

    Query parse_query(const string_view& s);

}}
