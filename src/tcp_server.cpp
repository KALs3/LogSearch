#include "tcp_server.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace logagg {

TCPServer::TCPServer(int port, RingBuffer<std::string>& buffer)
    : port_(port), 
      server_fd_(-1), 
      buffer_(buffer) {
}

TCPServer::~TCPServer() {
    stop();
}

void TCPServer::start() {
    if (running_) {
        return;
    }
    
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + 
                                 std::string(strerror(errno)));
    }
    
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[TCPServer] Warning: setsockopt failed: " 
                  << strerror(errno) << "\n";
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error("Failed to bind to port " + 
                                 std::to_string(port_) + ": " + 
                                 std::string(strerror(errno)));
    }
    
    if (listen(server_fd_, 10) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error("Failed to listen: " + 
                                 std::string(strerror(errno)));
    }
    
    running_ = true;
    accept_thread_ = std::thread(&TCPServer::accept_loop, this);
    
    std::cout << "[TCPServer] Listening on port " << port_ << "\n";
}

void TCPServer::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "[TCPServer] Stopping...\n";
    
    running_ = false;
    
    // CRITICAL: Shutdown server socket to interrupt accept()
    // close() alone does NOT reliably wake a thread blocked in accept()
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);  // Interrupt accept()
        close(server_fd_);
        server_fd_ = -1;
    }
    
    // Join accept thread FIRST (now unblocked by shutdown)
    if (accept_thread_.joinable()) {
        accept_thread_.join();
        std::cout << "[TCPServer] Accept thread joined\n";
    }
    
    // Shutdown all client sockets to interrupt recv()
    shutdown_all_clients();
    
    // Join client threads WITHOUT holding the mutex
    std::vector<std::thread> threads_to_join;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        threads_to_join = std::move(client_threads_);
        client_fds_.clear();
    }
    
    for (auto& t : threads_to_join) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << "[TCPServer] Stopped\n";
}

void TCPServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (!running_) {
                break;  // Normal shutdown
            }
            continue;  // Transient error, keep trying
        }
        
        std::cout << "[TCPServer] New client connected (fd=" 
                  << client_fd << ")\n";
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
            client_threads_.emplace_back(&TCPServer::handle_client, 
                                         this, client_fd);
        }
    }
    
    std::cout << "[TCPServer] Accept loop exiting\n";
}

void TCPServer::handle_client(int client_fd) {
    char buffer[4096];
    std::string partial_line;
    
    while (running_) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // Client disconnected normally
            } else if (running_) {
                // Error during recv
            }
            break;
        }
        
        partial_line.append(buffer, bytes_read);
        
        size_t pos;
        while ((pos = partial_line.find('\n')) != std::string::npos) {
            std::string line = partial_line.substr(0, pos);
            partial_line.erase(0, pos + 1);
            
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (!line.empty()) {
                // Use try_push with timeout to avoid blocking forever
                while (running_ && !buffer_.try_push(line)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                if (!running_) {
                    break;  // Shutdown during push wait
                }
            }
        }
    }
    
    // Remove from tracked fds BEFORE closing
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
        if (it != client_fds_.end()) {
            client_fds_.erase(it);
        }
    }
    
    close(client_fd);
}

void TCPServer::shutdown_all_clients() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    for (int fd : client_fds_) {
        // CRITICAL: shutdown() interrupts recv()
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    client_fds_.clear();
}

bool TCPServer::is_running() const {
    return running_;
}

int TCPServer::get_port() const {
    return port_;
}

} // namespace logagg