add_executable(fastfood
    fastfood.cpp
    fql.cpp
    fql.h
    recs_parser.cpp
    recs_parser.h
    predicates.h
    name.cpp
    name.h
    types.h
)

target_link_libraries(fastfood ${Boost_LIBRARIES})
