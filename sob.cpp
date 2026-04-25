#include "sob.hpp"
#include <array>
#include <string_view>
#include <tuple>

struct GxxContext
{
    static constexpr std::string_view cxx{"g++"};
    static constexpr sopho::StaticString obj_prefix{" -o "};
    static constexpr sopho::StaticString obj_postfix{".o"};
    static constexpr sopho::StaticString bin_prefix{" -o "};
    static constexpr sopho::StaticString build_prefix{"build/"};
    static constexpr std::array<std::string_view, 1> ldflags{"-lX11"};
    static constexpr std::array<std::string_view, 2> cxxflags{"-g","-Isrc"};
};

struct ClContext
{
    static constexpr std::string_view cxx{"cl"};
    static constexpr sopho::StaticString obj_prefix{" /Fo:"};
    static constexpr sopho::StaticString obj_postfix{".obj"};
    static constexpr sopho::StaticString bin_prefix{" /Fe:"};
    static constexpr sopho::StaticString build_prefix{"build/"};
    static constexpr std::array<std::string_view, 1> cxxflags{"/std:c++17"};
    static constexpr std::array<std::string_view, 2> ldflags{"user32.lib", "gdi32.lib"};
};

struct MainSource
{
#if defined(_MSC_VER)
    static constexpr sopho::StaticString source{"src/main/win_main.cpp"};
#elif defined(__GNUC__)
    static constexpr sopho::StaticString source{"src/main/linux_main.cpp"};
#else
#endif
};
struct Main
{
    using Dependent = std::tuple<MainSource>;
    static constexpr sopho::StaticString target{"main"};
};

#if defined(_MSC_VER)
using CxxContext = ClContext;
#elif defined(__GNUC__)
using CxxContext = GxxContext;
#else
#endif

int main()
{
    sopho::CxxToolchain<CxxContext>::CxxBuilder<Main>::build();
    return 0;
}
