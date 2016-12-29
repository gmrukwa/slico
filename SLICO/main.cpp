// SLICO.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include "SLICO.h"
#include <memory>
#include <string>

enum ErrorCode
{
    NO_ERROR,
    WRONG_NUMBER_OF_INPUTS,
    NON_POSITIVE_NUMBER_OF_CLUSTERS,
    SEGMENTATION_FAILURE
};

ErrorCode check_input(int argc, char* argv[])
{
    if (argc != 2)
        return WRONG_NUMBER_OF_INPUTS;

    try
    {
        auto no_clusters = std::stoi(argv[1]);
        if (no_clusters < 1)
            return NON_POSITIVE_NUMBER_OF_CLUSTERS;
    }
    catch (std::invalid_argument)
    {
        return NON_POSITIVE_NUMBER_OF_CLUSTERS;
    }
    catch (std::out_of_range)
    {
        return NON_POSITIVE_NUMBER_OF_CLUSTERS;
    }

    return NO_ERROR;
}

std::string build_error_message(const ErrorCode code)
{
    std::string message;
    switch (code)
    {
    case NO_ERROR:
        break;
    case WRONG_NUMBER_OF_INPUTS:
        message = "Wrong number of arguments.";
        break;
    case NON_POSITIVE_NUMBER_OF_CLUSTERS:
        message = "Number of clusters should be positive integer.";
        break;
    case SEGMENTATION_FAILURE:
        message = "Segmentation failed.";
        break;
    default:
        throw std::exception("Unknown error code.");
    }
    return message;
}

std::unique_ptr<unsigned> get_image(int& width, int& height,
    std::istream& in = std::cin)
{
    in >> width >> height;
    std::unique_ptr<unsigned> image(new unsigned[width*height]);
    for (auto i = 0; i<height; ++i)
    {
        for (auto j = 0; j<width; ++j)
        {
            in >> image.get()[i * width + j];
        }
    }
    return move(image);
}

void output_labels(const std::vector<int>& labels, const int width, 
    const int height, std::ostream& out = std::cout)
{
    for (auto i = 0; i<height; ++i)
    {
        for (auto j = 0; j<width; ++j)
        {
            out << labels[i * width + j] << " ";
        }
        out << "\n";
    }
}

int main(int argc, char *argv[])
{
    auto error_code = check_input(argc, argv);

    if (NO_ERROR != error_code)
    {
        auto error_message = build_error_message(error_code);
        std::cout << error_message.c_str() << "\nUsage: " << argv[0]
            << " NO_CLUSTERS\n";
        return 0;
    }

    auto no_clusters = std::stoi(argv[1]);

    int width, height;
    auto image = get_image(width, height);

    auto labels = imaging::slico(image.get(), width, height, no_clusters);

    output_labels(labels, width, height);
    
    return 0;
}

