#pragma once
#include <pqxx/pqxx>

#include "app/use_cases_impl.h"
#include "postgres/postgres.h"

namespace bookypedia {

struct AppConfig {
    std::string db_url;
};

class Application {
public:
    explicit Application(const AppConfig& config);

    void Run();

private:
    pqxx::connection connection_;
    postgres::Database db_{connection_};
    app::UseCasesImpl use_cases_{connection_};
};

}  // namespace bookypedia