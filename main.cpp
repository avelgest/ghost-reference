#include <exception>
#include <iostream>
#include <stdexcept>

#include "app.h"

void report_exception(const std::exception &e) noexcept
{
    try
    {
        std::cerr << e.what();
    }
    catch (const std::exception &e)
    {
        return;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        const App app(argc, argv);
        return App::exec();
    }
    catch (const std::exception &e)
    {
        report_exception(e);
        return EXIT_SUCCESS;
    }
    catch (...)
    {
        report_exception(std::runtime_error("Unknown exception"));
        return EXIT_FAILURE;
    }
    return 0;
}