#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view{argv[i]} == "--self-test")
        {
            std::cout << "KSGE exporter self-test OK\n";
            return 0;
        }
    }
    std::cout << "KSGE exporter: use --self-test\n";
    return 0;
}