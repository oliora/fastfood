#include "types.h"


namespace fastfood { namespace fql {

    // Parse WHERE clause
    PredicatePtr parse_predicate(const string_view& s);

}}