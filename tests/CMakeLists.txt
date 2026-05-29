add_executable(halloc_tests main.c)
target_link_libraries(halloc_tests PRIVATE halloc)

add_test(NAME halloc_tests COMMAND halloc_tests)