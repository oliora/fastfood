#include "types.h"
#include <string>
#include <vector>

/*
 Query w/o aggregation (i.e. reduce):
    SELECT fld1, fld2 WHERE fld1 = "val1" AND (fld2 >= 1.5 OR fld3 <> "start")

 TODO:
    v1:
    - Aggregation
    - regex match =~ (other name? MATCH, ~) operator
    v2:
    - IN operator
    - BETWEEN operator
    - LIKE operator (simpler than force user to write a regex for 'starts with')
    - improve parsing errors diagnostic
    - ? All fields (*)
    - ? IS NULL, IS NOT NULL operators
    - ? NOT (or !)
 */


namespace fastfood { namespace fql {

    struct Query
    {
        std::vector<std::string> m_fields;
        //std::vector<Aggregator> m_aggregators;
        PredicatePtr m_where;
    };

    Query parse_query(string_view s);

}}
