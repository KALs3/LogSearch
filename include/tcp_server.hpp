#ifndef LOG_AGGREGATOR_TCP_SERVER_HPP
#define LOG_AGGREGATOR_TCP_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "ring_buffer.hpp"

namespace logagg {

class TCPServer {
public:
    TCPServer(int port, RingBuffer<std::string>& buffer);
    ~TCPServer();
    
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
    
    void start();
    void stop();
    
    bool is_running() const;
    int get_port() const;
    
private:
    void accept_loop();
    void handle_client(int client_fd);
    void shutdown_all_clients();
    
    int port_;
    int server_fd_;
    RingBuffer<std::string>& buffer_;
    
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    
    std::mutex clients_mutex_;
    std::vector<int> client_fds_;
    std::vector<std::thread> client_threads_;
};

} 

#endif 