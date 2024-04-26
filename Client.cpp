#include <iostream>
#include <thread>
#include <deque>
#include <array>
#include <string>
#include <sstream>
#include "Message.h"
#include <prometheus/exposer.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/buffer.hpp>
#include <asio/read_until.hpp>
#include <asio/error_code.hpp>

prometheus::Registry registry;

auto& sent_messages_counter_family = prometheus::BuildCounter()
    .Name("client_sent_messages_total")
    .Help("Total number of sent messages from the client")
    .Register(registry);

auto& sent_messages_counter = sent_messages_counter_family.Add({});

auto& errors_counter_family = prometheus::BuildCounter()
    .Name("client_errors_total")
    .Help("Total number of errors encountered by the client")
    .Register(registry);

auto& errors_counter = errors_counter_family.Add({});

class Client
{
public:
    Client(asio::io_context& io_context, const std::string& host, const std::string& port)
        : io_context_(io_context), resolver_(io_context), socket_(io_context)
    {
        do_resolve(host, port);
    }

    void write(const std::string& msg)
    {
        asio::post(socket_.get_executor(), [this, msg]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if (!write_in_progress)
            {
                do_write();
            }
        });
        sent_messages_counter.Increment();
    }

    void close()
    {
        asio::post(socket_.get_executor(), [this] { socket_.close(); });
        std::cout << "Close Socket" << std::endl;
    }

private:
    void do_resolve(const std::string& host, const std::string& port)
    {
        resolver_.async_resolve(host, port,
            [this](const std::error_code& ec, asio::ip::tcp::resolver::results_type results)
            {
                if (!ec)
                {
                    do_connect(results);
                }
                else
                {
                    errors_counter.Increment();
                }
            });
    }

    void do_connect(asio::ip::tcp::resolver::results_type results)
    {
        asio::async_connect(socket_, results,
            [this](const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint)
            {
                if (!ec)
                {
                    do_read();
                }
                else
                {
                    errors_counter.Increment();
                }
            });
    }

    void do_read()
    {
        asio::async_read_until(socket_, buffer_, '\n',
            [this](const std::error_code& ec, std::size_t length)
            {
                if (!ec)
                {
                    std::istream stream(&buffer_);
                    std::string message;
                    std::getline(stream, message, '!'); // Читаем строку из потока до символа '\n'
                    std::cout << "Received: " << message << std::endl;
                    buffer_.consume(length); // Удаляем прочитанные данные из буфера                
                    do_read();
                }
                else
                {
                    socket_.close();
                    errors_counter.Increment();
                }
            });
    }

    void do_write()
    {
        if(!write_msgs_.empty()){
            asio::async_write(socket_, asio::buffer(write_msgs_.front()),
                [this](std::error_code ec, std::size_t /*length*/)
                {
                    if (!ec)
                    {
                        write_msgs_.pop_front();
                        if (!write_msgs_.empty())
                        {
                            do_write();
                        }
                    }
                    else
                    {
                        socket_.close();
                        errors_counter.Increment();
                        std::cout << "ERROR" << std::endl;
                    }
                });
        }
    }

    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket_;
    asio::io_context& io_context_;
    std::deque<std::string> write_msgs_;
    asio::streambuf buffer_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: chat_client <host> <port>\n";
            return 1;
        }

        asio::io_context io_context;

        Client client(io_context, argv[1], argv[2]);

        std::thread t([&io_context] { io_context.run(); });

        while (true)
        {
            std::string input;
            std::getline(std::cin, input);

            if (input == "exit")
            {
                client.close();
                break;
            }

            client.write(input);
        }

        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}