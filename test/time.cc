#include <chrono>
#include <format>
#include <iostream>

int main()
{
    auto now = std::chrono::system_clock::now();
    std::string time_str = std::format("{:%Y-%m-%d %H:%M:%S\t}", now);
    std::cout << "Current time: " << time_str << std::endl;
    return 0;
}
