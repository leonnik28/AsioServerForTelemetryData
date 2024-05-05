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
#include <matplotlibcpp.h>
#include <Eigen/Dense>
#include <fstream>
#include "TelemetryFileProcessor.h"

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

    std::string  VisualizeData() {
        std::string filename = "telemetry_data.txt";
        return TelemetryFileProcessor().VisualizeData(filename);
    }

    std::string StatisticalAnalysis() {
        std::string filename = "telemetry_data.txt";
        return TelemetryFileProcessor().StatisticalAnalysis(filename);
    }

    std::string Predict() {
        std::string filename = "telemetry_data.txt";
        return TelemetryFileProcessor().Predict(filename);
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
        std::string telemetryData;
        std::string telemetryDataToFile;

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

        telemetryData += "Temperature: " + std::to_string(temperature) + " C\n";
        telemetryData += "Humidity: " + std::to_string(humidity) + " %\n";

        telemetryDataToFile += std::to_string(temperature) + " " + std::to_string(humidity) + "\n";

        std::string filename = "telemetry_data.txt";
        std::ofstream outfile(filename, std::ios_base::app);
        outfile << telemetryDataToFile;
        outfile.close();
        
        return telemetryData;
    }

    void handle_request(std::string request)
    {
        std::string response;

        if (request == "QUIT")
        {
            socket_.close();
            return;
        }
        else if (request == "INFO")
        {
            response = "Server information ... : \n";
        }
        else if (request == "TELEMETRYDATA")
        {
            response = generate_telemetry_data();
        }
        else if (request == "METRICKS")
        {
            response = std::to_string(sent_messages_counter_->Value()) + "\n";
        }
        else if (request == "VISUALIZE") {
            response = VisualizeData();
        }
        else if (request == "STATISTICS") {
            response = StatisticalAnalysis();
        }
        else if (request == "PREDICT") {
            response = Predict();
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

    void stop()
    {
        acceptor_.close();
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

static TelemetryServer* server_ptr = nullptr;
static asio::io_service* io_service_ptr;

void signal_handler(int signal)
{
    if (server_ptr != nullptr)
    {
        std::cout << "\nSignal " << signal << " received, shutting down server...\n";
        server_ptr->stop();
        io_service_ptr->stop();
    }
}


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
        io_service_ptr = &io_service;

        TelemetryServer server(io_service, std::atoi(argv[1]));
        server_ptr = &server;

        prometheus::Exposer exposer("localhost:8080");
        exposer.RegisterCollectable(server.getRegistry());

        std::signal(SIGINT, signal_handler);

        io_service.run();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}