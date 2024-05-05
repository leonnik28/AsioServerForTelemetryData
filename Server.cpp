#include <iostream>
#include <thread>
#include <string>
#include <random>
#include <functional>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <sstream>
#include <prometheus/exposer.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/read_until.hpp>

class TelemetrySession : public std::enable_shared_from_this<TelemetrySession>
{
public:
TelemetrySession(asio::ip::tcp::socket socket, std::shared_ptr<prometheus::Registry> registry)
        : socket_(std::move(socket)),
        registry_(registry)
    {
        auto& sent_messages_counter_family = prometheus::BuildCounter()
            .Name("telemetry_sent_messages_total")
            .Help("Total number of sent telemetry messages")
            .Register(*registry_);
        sent_messages_counter_ = &sent_messages_counter_family.Add({});

        auto& errors_counter_family = prometheus::BuildCounter()
            .Name("telemetry_errors_total")
            .Help("Total number of telemetry errors")
            .Register(*registry_);
        errors_counter_ = &errors_counter_family.Add({});
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
                sent_messages_counter_->Increment(); // Увеличение счетчика отправленных сообщений
                do_read();
            }
            else
            {
                errors_counter_->Increment(); // Увеличение счетчика ошибок
                std::cout << "ERROR" << std::endl;
            }
        });
}

    std::string generate_telemetry_data()
    {
        std::string telemetry_data;

        // Генерация нормально распределенных значений температуры и влажности
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> disTemperature(20, 5); // Среднее значение 20, стандартное отклонение 5
        std::normal_distribution<> disHumidity(50, 10); // Среднее значение 50, стандартное отклонение 10
        int temperature = static_cast<int>(std::round(disTemperature(gen)));
        int humidity = static_cast<int>(std::round(disHumidity(gen)));

        // Введение колебаний в данные
        static bool first = true;
        static int lastTemperature = temperature;
        static int lastHumidity = humidity;
        if (first) {
            first = false;
        } else {
            temperature = lastTemperature + std::round(disTemperature(gen) * 0.1);
            humidity = lastHumidity + std::round(disHumidity(gen) * 0.1);
        }
        lastTemperature = temperature;
        lastHumidity = humidity;

        // Модель изменения температуры в течение дня
        static int hour = 0;
        hour = (hour + 1) % 24;
        float temperatureFactor = 1.0 + 0.2 * std::sin(2 * M_PI * hour / 24.0);
        temperature = static_cast<int>(temperature * temperatureFactor);

        telemetry_data += "Temperature: " + std::to_string(temperature) + " C\n";
        telemetry_data += "Humidity: " + std::to_string(humidity) + " %\n";

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
    else if (request == "METRICKS")
    {
        response = std::to_string(sent_messages_counter_->Value()) + "\n";
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
    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Counter* sent_messages_counter_;
    prometheus::Counter* errors_counter_;
};

class TelemetryServer
{
public:
    TelemetryServer(asio::io_service& io_service, short port)
        : acceptor_(io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
        registry_(std::make_shared<prometheus::Registry>())
    {
        do_accept();
    }

    std::shared_ptr<prometheus::Registry> getRegistry()
    {
        return registry_;
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<TelemetrySession>(std::move(socket), registry_)->start();
                }

                do_accept();
            });
    }

    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<prometheus::Registry> registry_;
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

        prometheus::Exposer exposer("localhost:8080");
        exposer.RegisterCollectable(s.getRegistry());

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}