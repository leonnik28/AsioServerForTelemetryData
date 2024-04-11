#include <iostream>
#include <thread>
#include <string>
#include <random>
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
        if (socket_.is_open()){
            asio::async_read_until(socket_, buffer_, '%',
                [this, self](const std::error_code& ec, std::size_t length)
                {
                    if (!ec)
                    {
                        std::istream stream(&buffer_);
                        std::string message;
                        std::getline(stream, message); // Читаем строку из потока
                        std::cout << "Received: " << message << std::endl;
                        buffer_.consume(length);
                        do_write();
                    }
                    else
                    {
                        std::cout << "ERROR: " << ec << std::endl;
                    }
                });
        }
    }

    void do_write()
    {
        auto self(shared_from_this());
        std::string message = generate_telemetry_data() + "!";
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

    std::string generate_telemetry_data() {

        std::string telemetry_data;

        int temperature = generate_random_number(-40, 50); // температура от -40 до 50 градусов
        int humidity = generate_random_number(0, 100); // влажность от 0 до 100%
        float pressure = generate_random_number(950, 1050) + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(0.1))); // давление от 950 до 1050 мм рт. ст.
        int wind_speed = generate_random_number(0, 20); // скорость ветра от 0 до 20 м/с

        telemetry_data += "Temperature: " + std::to_string(temperature) + " C\n";
        telemetry_data += "Humidity: " + std::to_string(humidity) + " %\n";
        telemetry_data += "Pressure: " + std::to_string(pressure) + " mmHg\n";
        telemetry_data += "Wind speed: " + std::to_string(wind_speed) + " m/s\n";

        return telemetry_data;
    }

    int generate_random_number(int min, int max) {
    static bool first = true;
    if (first) {  
        srand( time(NULL) ); // seeding for the first time only!
        first = false;
    }
    return min + rand() % (( max + 1 ) - min);
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