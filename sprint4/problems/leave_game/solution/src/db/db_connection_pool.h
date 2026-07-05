#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <cassert>
#include <stdexcept>

namespace db {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;
    
    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept
            : conn_(std::move(conn))
            , pool_(&pool) {}
            
        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }
        
        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;
        
        pqxx::connection& operator*() const& noexcept { return *conn_; }
        pqxx::connection* operator->() const& noexcept { return conn_.get(); }
        
    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };
    
    ConnectionPool(size_t capacity, std::function<ConnectionPtr()> factory);
    
    ConnectionWrapper GetConnection();
    
    size_t GetPoolSize() const;
    size_t GetUsedConnections() const;
    
private:
    void ReturnConnection(ConnectionPtr&& conn);
    
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

} // namespace db