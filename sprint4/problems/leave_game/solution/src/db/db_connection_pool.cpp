#include "db_connection_pool.h"
#include <stdexcept>

namespace db {

ConnectionPool::ConnectionPool(size_t capacity, std::function<ConnectionPtr()> factory) {
    if (capacity == 0) {
        throw std::runtime_error("Connection pool capacity must be > 0");
    }
    
    pool_.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
        auto conn = factory();
        if (!conn) {
            throw std::runtime_error("Failed to create database connection");
        }
        pool_.emplace_back(std::move(conn));
    }
}

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection() {
    std::unique_lock lock(mutex_);
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });
    return {std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
    {
        std::lock_guard lock(mutex_);
        assert(used_connections_ > 0);
        pool_[--used_connections_] = std::move(conn);
    }
    cond_var_.notify_one();
}

size_t ConnectionPool::GetPoolSize() const {
    return pool_.size();
}

size_t ConnectionPool::GetUsedConnections() const {
    std::lock_guard lock(mutex_);
    return used_connections_;
}

} // namespace db