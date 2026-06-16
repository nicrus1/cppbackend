#define CATCH_CONFIG_MAIN
#include <catch2/catch_session.hpp>

// В Catch2 v3 точка входа инициализируется автоматически при линковке с Catch2::Catch2WithMain,
// либо мы можем явно определить сессию, если это необходимо.
// Так как в CMakeLists.txt используется стандартная линковка, оставляем файл как корректный модуль.

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}