add_executable(fastfood_tests
    main.cpp
    fql.cpp
)

target_include_directories(fastfood_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME fastfood_tests COMMAND fastfood_tests)
