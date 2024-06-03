#include <string>
#include <vector>
#include <matplotlibcpp.h>
#include <Eigen/Dense>
#include <fstream>

class TelemetryFileProcessor {
public:
    std::string visualizeData(const std::string& filename) {
        std::ifstream infile(filename);
        std::vector<double> x, y;
        double temp_x, temp_y;
        while (infile >> temp_x >> temp_y) {
            x.push_back(temp_x);
            y.push_back(temp_y);
        }
        matplotlibcpp::plot(x, y);
        matplotlibcpp::show();

        std::stringstream result;

        result << "Visualize Comliete\n";
        return result.str();

    }

    std::string statisticalAnalysis(const std::string& filename) {
        std::ifstream infile(filename);
        std::vector<double> data1, data2;
        double temp1, temp2;
        while (infile >> temp1 >> temp2) {
            data1.push_back(temp1);
            data2.push_back(temp2);
        }
        Eigen::VectorXd v1(data1.size());
        Eigen::VectorXd v2(data2.size());
        for (int i = 0; i < data1.size(); i++) {
            v1(i) = data1[i];
        }
        for (int i = 0; i < data2.size(); i++) {
            v2(i) = data2[i];
        }

        double mean1 = v1.mean();
        double squared_diffs_sum1 = (v1.array() - mean1).pow(2).sum();
        int n1 = v1.size();
        double stddev1 = std::sqrt(squared_diffs_sum1 / (n1 - 1));

        double mean2 = v2.mean();
        double squared_diffs_sum2 = (v2.array() - mean2).pow(2).sum();
        int n2 = v2.size();
        double stddev2 = std::sqrt(squared_diffs_sum2 / (n2 - 1));

        std::stringstream result;
        result << "Temperature:\n";
        result << "Mean: " << mean1 << "\n";
        result << "Standard deviation: " << stddev1 << "\n";
        result << "Humidity:\n";
        result << "Mean: " << mean2 << "\n";
        result << "Standard deviation: " << stddev2 << "\n";
        return result.str();
    }

    std::string predict(const std::string& filename) {
        std::ifstream infile(filename);
        std::vector<double> data1, data2;
        double temp1, temp2;
        while (infile >> temp1 >> temp2) {
            data1.push_back(temp1);
            data2.push_back(temp2);
        }
        Eigen::VectorXd v1(data1.size());
        Eigen::VectorXd v2(data2.size());
        for (int i = 0; i < data1.size(); i++) {
            v1(i) = data1[i];
        }
        for (int i = 0; i < data2.size(); i++) {
            v2(i) = data2[i];
        }
        Eigen::VectorXd coeff1 = Eigen::VectorXd::Ones(2);
        Eigen::MatrixXd A1(data1.size(), 2);
        for (int i = 0; i < data1.size(); i++) {
            A1(i, 0) = i;
            A1(i, 1) = 1;
        }
        coeff1 = (A1.transpose() * A1).inverse() * A1.transpose() * v1;
        double predicted_value1 = coeff1(0) * data1.size() + coeff1(1);

        Eigen::VectorXd coeff2 = Eigen::VectorXd::Ones(2);
        Eigen::MatrixXd A2(data2.size(), 2);
        for (int i = 0; i < data2.size(); i++) {
            A2(i, 0) = i;
            A2(i, 1) = 1;
        }
        coeff2 = (A2.transpose() * A2).inverse() * A2.transpose() * v2;
        double predicted_value2 = coeff2(0) * data2.size() + coeff2(1);

        std::stringstream result;
        result << "Temperature:\n";
        result << "Predicted value: " << predicted_value1 << "\n";
        result << "Humidity:\n";
        result << "Predicted value: " << predicted_value2 << "\n";
        return result.str();
    }
};
