#include "audio.h"
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <thread>

using namespace std::literals;

void StartServer(uint16_t port) {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::udp::socket socket(io_context, 
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port));
        
        Player player(ma_format_u8, 1);
        
        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Waiting for audio messages..." << std::endl;
        
        while (true) {
            std::vector<char> buffer(65000);
            boost::asio::ip::udp::endpoint sender_endpoint;
            
            size_t bytes_received = socket.receive_from(
                boost::asio::buffer(buffer), sender_endpoint);

            buffer.resize(bytes_received);
            
            int frame_size = player.GetFrameSize();
            size_t frames = bytes_received / frame_size;
            
            std::cout << "Received " << bytes_received << " bytes (" 
                      << frames << " frames) from " 
                      << sender_endpoint.address().to_string() 
                      << ":" << sender_endpoint.port() << std::endl;

            player.PlayBuffer(buffer.data(), frames, 1.5s);
            std::cout << "Playing done" << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

void StartClient(uint16_t port) {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::udp::socket socket(io_context, 
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
        
        Recorder recorder(ma_format_u8, 1);
        
        std::cout << "Client started on port " << port << std::endl;
        
        while (true) {
            std::string server_ip;
            std::cout << "Enter server IP address (or 'quit' to exit): ";
            std::getline(std::cin, server_ip);
            
            if (server_ip == "quit" || server_ip == "q") {
                break;
            }
            
            boost::asio::ip::udp::endpoint server_endpoint(
                boost::asio::ip::make_address(server_ip), port);
            
            std::cout << "Press Enter to record message..." << std::endl;
            std::string dummy;
            std::getline(std::cin, dummy);
            
            auto rec_result = recorder.Record(65000, 1.5s);
            std::cout << "Recording done" << std::endl;
            
            int frame_size = recorder.GetFrameSize();
            size_t bytes_to_send = rec_result.frames * frame_size;
            
            std::cout << "Sending " << bytes_to_send << " bytes (" 
                      << rec_result.frames << " frames) to " 
                      << server_ip << ":" << port << std::endl;
            
            socket.send_to(boost::asio::buffer(rec_result.data.data(), bytes_to_send), 
                          server_endpoint);
            
            std::cout << "Message sent" << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
    
    if (mode == "server") {
        StartServer(port);
    }
    else if (mode == "client") {
        StartClient(port);
    }
    else {
        std::cerr << "Invalid mode. Use 'client' or 'server'" << std::endl;
        return 1;
    }
    
    return 0;
}