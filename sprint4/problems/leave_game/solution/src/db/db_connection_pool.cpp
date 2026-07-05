#include "db_connection_pool.h"
#include <stdexcept>
#include <iostream>

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
    return GetConnection(std::chrono::seconds(5));
}

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    
    // Проверяем, не пытается ли текущий поток получить второе соединение
    auto thread_id = std::this_thread::get_id();
    if (locked_by_thread_.count(thread_id) > 0) {
        throw std::runtime_error("Thread already holds a connection from this pool");
    }
    
    // Ждем освобождения соединения с таймаутом
    bool success = cond_var_.wait_for(lock, timeout, [this] {
        return used_connections_ < pool_.size();
    });
    
    if (!success) {
        throw std::runtime_error("Timeout waiting for database connection");
    }
    
    locked_by_thread_.insert(thread_id);
    return {std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
    auto thread_id = std::this_thread::get_id();
    {
        std::lock_guard lock(mutex_);
        assert(used_connections_ > 0);
        pool_[--used_connections_] = std::move(conn);
        locked_by_thread_.erase(thread_id);
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