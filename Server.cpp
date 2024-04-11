#include <iostream>
#include <thread>
#include <string>
#include <random>
#include <functional>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <sstream>
#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/read_until.hpp>

class TelemetrySession : public std::enable_shared_from_this<TelemetrySession>
{
public:
    TelemetrySession(asio::ip::tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        if (socket_.is_open()) {
            asio::async_read_until(socket_, buffer_, "",
            [this, self](const std::error_code& ec, std::size_t length)
            {
                if (!ec)
                {
                    std::istream stream(&buffer_);
                    std::string request;
                    std::getline(stream, request); // Read the request from the stream
                    std::cout << "Received: " << request << std::endl;
                    buffer_.consume(length);
                    handle_request(request);
                    
                }
                else
                {
                    std::cout << "ERROR: " << ec << std::endl;
                }
            });
        }
    }

void do_write(std::string message)
{
    auto self(shared_from_this());
    message += "!";
    std::cout << "Received: " << message << std::endl;
    asio::async_write(socket_, asio::buffer(message),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read();
            }
            else
            {
                std::cout << "ERROR" << std::endl;
            }
        });
}

std::string generate_telemetry_data()
{
    std::string telemetry_data;

    int temperature = generate_random_number(-40, 50);
    int humidity = generate_random_number(0, 100);
    float pressure = generate_random_number(950, 1050) + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (0.1)));
    int wind_speed = generate_random_number(0, 20);

    telemetry_data += "Temperature: " + std::to_string(temperature) + " C\n";
    telemetry_data += "Humidity: " + std::to_string(humidity) + " %\n";
    telemetry_data += "Pressure: " + std::to_string(pressure) + " mmHg\n";
    telemetry_data += "Wind speed: " + std::to_string(wind_speed) + " m/s\n";

    return telemetry_data;
}

int generate_random_number(int min, int max)
{
    static bool first = true;
    if (first) {
        srand(time(NULL));
        first = false;
    }
    return min + rand() % ((max + 1) - min);
}

void handle_request(std::string request)
{
    std::string response;

    if (request == "ECHO")
    {
        response = request + "\n";
    }
    else if (request == "QUIT")
    {
        socket_.close();
        return;
    }
    else if (request == "INFO")
    {
        response = "Server information ... : \n";
    }
    else if (request == "LIST")
    {
        std::string current_directory = getCurrentDirectory();
        response = listDirectory(current_directory);
    }
    else if (request.substr(0, 3) == "CD ")
    {
        std::string directory = request.substr(3);
        bool success = changeDirectory(directory);
        if (success)
        {
            std::string current_directory = getCurrentDirectory();
            response = (std::string)"Directory changed to: " + current_directory + "\n";
        }
        else
        {
            response = (std::string)"Failed to change directory\n";
        }
    }
    else if (request == "TELEMETRYDATA")
    {
        response = generate_telemetry_data();
    }
    else
    {
        response = updateToString("Unknown request\n");
    }

    do_write(response);
}

std::string updateToString(const char* string){
    std::string newString;
    newString = string;
    return newString;
}

std::string getCurrentDirectory()
{
    char* buffer = get_current_dir_name();
    std::string current_directory(buffer);
    free(buffer);
    return current_directory;
}

std::string listDirectory(const std::string& directory)
{
    std::vector<std::string> contents = getDirectoryContents(directory);
    std::string result;

    for (const std::string& entry : contents)
    {
        result += entry + "\n";
    }

    return result;
}

std::vector<std::string> getDirectoryContents(const std::string& directory)
{
    std::vector<std::string> contents;
    DIR* dir = opendir(directory.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name(entry->d_name);
            // Игнорируем текущий и родительский каталоги
            if (name != "." && name != "..")
            {
                contents.push_back(name);
            }
        }
        closedir(dir);
    }
    return contents;
}

bool changeDirectory(const std::string& directory)
{
    if (chdir(directory.c_str()) == 0)
    {
        return true;
    }
    return false;
}


    asio::ip::tcp::socket socket_;
    asio::streambuf buffer_;
};

class TelemetryServer
{
public:
    TelemetryServer(asio::io_service& io_service, short port)
        : acceptor_(io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<TelemetrySession>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: telemetry <port>\n";
            return 1;
        }

        asio::io_service io_service;

        TelemetryServer s(io_service, std::atoi(argv[1]));

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}