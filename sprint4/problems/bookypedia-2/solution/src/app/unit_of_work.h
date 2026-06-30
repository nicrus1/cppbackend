#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../postgres/postgres.h"

namespace app {

class UnitOfWork {
public:
    explicit UnitOfWork(pqxx::connection& connection)
        : connection_(connection)
        , transaction_(connection)
        , authors_(transaction_)
        , books_(transaction_)
        , tags_(transaction_) {
    }

    postgres::AuthorRepositoryImpl& Authors() {
        return authors_;
    }

    postgres::BookRepositoryImpl& Books() {
        return books_;
    }

    postgres::BookTagRepositoryImpl& Tags() {
        return tags_;
    }

    void Commit() {
        transaction_.commit();
    }

    void Rollback() {
        transaction_.abort();
    }

private:
    pqxx::connection& connection_;
    pqxx::work transaction_;
    postgres::AuthorRepositoryImpl authors_;
    postgres::BookRepositoryImpl books_;
    postgres::BookTagRepositoryImpl tags_;
};

}  // namespace app