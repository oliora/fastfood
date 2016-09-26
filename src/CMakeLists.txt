add_executable(fastfood
    fastfood.cpp
    fql.cpp
    fql.h
    predicates.h
    types.h
)

target_link_libraries(fastfood ${Boost_LIBRARIES})
