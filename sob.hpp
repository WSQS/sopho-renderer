// include/sob.hpp
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
// include/diag.hpp
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <variant>
// include/meta.hpp
namespace sopho
{
    namespace detail
    {
        // Declaration: Map takes a Mapper template and a Tuple type
        template <template <typename> class Mapper, typename List>
        struct MapImpl;
        // Specialization: Unpack the tuple types (Ts...), apply Mapper to each, repack.
        template <template <typename> class Mapper, typename... Ts>
        struct MapImpl<Mapper, std::tuple<Ts...>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = std::tuple<typename Mapper<Ts>::type...>;
        };
        template <template <typename> class Mapper, typename... Ts>
        struct MapImpl<Mapper, std::variant<Ts...>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = std::variant<typename Mapper<Ts>::type...>;
        };
    } // namespace detail
    // Helper alias for cleaner syntax (C++14 style aliases)
    template <template <typename> class Mapper, typename List>
    using Map = typename detail::MapImpl<Mapper, List>::type;
    namespace detail
    {
        template <template <typename, typename> typename Folder, typename Value, typename List>
        struct FoldlImpl;
        template <template <typename, typename> typename Folder, typename Value, typename T, typename... Ts>
        struct FoldlImpl<Folder, Value, std::tuple<T, Ts...>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = typename FoldlImpl<Folder, typename Folder<Value, T>::type, std::tuple<Ts...>>::type;
        };
        template <template <typename, typename> typename Folder, typename Value>
        struct FoldlImpl<Folder, Value, std::tuple<>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = Value;
        };
        template <template <typename, typename> typename Folder, typename Value, typename T, typename... Ts>
        struct FoldlImpl<Folder, Value, std::variant<T, Ts...>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = typename FoldlImpl<Folder, typename Folder<Value, T>::type, std::variant<Ts...>>::type;
        };
        template <template <typename, typename> typename Folder, typename Value>
        struct FoldlImpl<Folder, Value, std::variant<>>
        {
            // The "return value" of a metafunction is usually defined as 'type'
            using type = Value;
        };
    } // namespace detail
    template <template <typename, typename> class Folder, typename Value, typename List>
    using Foldl = typename detail::FoldlImpl<Folder, Value, List>::type;
} // namespace sopho
// include/diag.hpp
#define SOPHO_DETAIL_JOIN2_IMPL(a, b) a##b
#define SOPHO_DETAIL_JOIN2(a, b) SOPHO_DETAIL_JOIN2_IMPL(a, b)
#define SOPHO_STACK()                                                                                                  \
    ::sopho::StackScope SOPHO_DETAIL_JOIN2(_sopho_stack_scope_, __COUNTER__) { __FILE__, __func__, __LINE__ }
#define SOPHO_VALUE(value)                                                                                             \
    ::sopho::StackValue SOPHO_DETAIL_JOIN2(_sopho_stack_value_, __COUNTER__) { std::string(#value), value }
#define SOPHO_SOURCE_LOCATION                                                                                          \
    ::sopho::SourceLocation { __FILE__, __func__, static_cast<std::uint32_t>(__LINE__) }
// expr + variadic msg parts (at least one msg token is required by your convention)
#define SOPHO_ASSERT(expr, ...)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            ::sopho::assert_fail(#expr, ::sopho::build_message(__VA_ARGS__), SOPHO_SOURCE_LOCATION);                   \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)
namespace sopho
{
    struct SourceLocation
    {
        const char* file_name{};
        const char* function_name{};
        std::uint32_t line_number{};
    };
    template <typename T>
    struct ReferenceWrapper
    {
        using type = std::reference_wrapper<const T>;
    };
    [[noreturn]] inline void assert_fail(std::string_view expr, std::string msg, SourceLocation loc);
    // Build message only on failure path.
    template <class... Args>
    inline std::string build_message(Args&&... args)
    {
        std::ostringstream os;
        (os << ... << std::forward<Args>(args));
        return os.str();
    }
    using StackValueReference =
        Map<ReferenceWrapper, std::variant<std::int64_t, std::uint64_t, double, std::string, std::filesystem::path>>;
    struct StackInfo
    {
        SourceLocation source_location{};
        std::map<std::string, StackValueReference> stack_values{};
    };
    inline std::string stack_value_to_string(const StackValueReference& v)
    {
        return std::visit(
            [](const auto& ref) -> std::string
            {
                const auto& x = ref.get();
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, std::string>)
                {
                    return x;
                }
                else if constexpr (std::is_same_v<T, std::filesystem::path>)
                {
                    // Note: string() is platform/native encoding; for cross-platform stable output you might prefer
                    // generic_u8string().
                    return x.string();
                }
                else
                {
                    // Covers int64_t / uint64_t / double
                    return std::to_string(x);
                }
            },
            v);
    }
    struct StackInfoInstance
    {
        std::list<StackInfo> stack_infos;
        static StackInfoInstance& get()
        {
            static thread_local StackInfoInstance instance{};
            return instance;
        }
    };
    struct StackScope
    {
        StackInfo* p_stack_info{};
        StackScope(const char* file_name, const char* function_name, std::uint32_t line_number)
        {
            StackInfo stack_info{};
            stack_info.source_location.file_name = file_name;
            stack_info.source_location.function_name = function_name;
            stack_info.source_location.line_number = line_number;
            p_stack_info = &StackInfoInstance::get().stack_infos.emplace_front(stack_info);
        }
        ~StackScope()
        {
            SOPHO_ASSERT(!StackInfoInstance::get().stack_infos.empty(), "Stack has been cleared");
            SOPHO_ASSERT(&StackInfoInstance::get().stack_infos.front() == p_stack_info, "Stack Corrupted");
            StackInfoInstance::get().stack_infos.pop_front();
        }
        StackScope(const StackScope&) = delete;
        StackScope& operator=(const StackScope&) = delete;
        StackScope(StackScope&&) = delete;
        StackScope& operator=(StackScope&&) = delete;
    };
    struct StackValue
    {
        std::string name{};
        StackValue(std::string value_name, StackValueReference value)
        {
            name = value_name;
            StackInfoInstance::get().stack_infos.front().stack_values.insert_or_assign(value_name, value);
        }
        ~StackValue() { StackInfoInstance::get().stack_infos.front().stack_values.erase(name); }
        StackValue(const StackValue&) = delete;
        StackValue& operator=(const StackValue&) = delete;
        StackValue(StackValue&&) = delete;
        StackValue& operator=(StackValue&&) = delete;
    };
    void dump_callstack(std::ostringstream& ss)
    {
        auto& infos = StackInfoInstance::get().stack_infos;
        std::uint32_t size{0};
        for (const auto& info : infos)
        {
            ss << "stack" << size << ": " << info.source_location.file_name << ":" << info.source_location.line_number
               << "@" << info.source_location.function_name << std::endl;
            for (const auto& [key, value] : info.stack_values)
            {
                ss << "name:" << key << " value:" << stack_value_to_string(value) << std::endl;
            }
            ++size;
        }
    }
    [[noreturn]] inline void assert_fail(std::string_view expr, std::string msg, SourceLocation loc)
    {
        std::ostringstream ss;
        ss << "SOPHO_ASSERT failed: (" << expr << ")\n";
        if (!msg.empty())
        {
            ss << "Message: " << msg << "\n";
        }
        ss << "Location: " << (loc.file_name ? loc.file_name : "<unknown>") << ":" << loc.line_number << " @ "
           << (loc.function_name ? loc.function_name : "<unknown>") << "\n";
        dump_callstack(ss);
        std::cerr << ss.str();
        std::cerr.flush();
        std::abort();
    }
} // namespace sopho
// include/sob.hpp
// include/file_generator.hpp
#include <deque>
#include <fstream>
#include <memory>
#include <set>
#include <vector>
// include/file_generator.hpp
namespace sopho
{
    std::string read_file(std::filesystem::path fs_path)
    {
        std::ifstream file_stream(fs_path, std::ios::binary);
        SOPHO_ASSERT(file_stream.is_open(), "open file failed, file name:", fs_path.string());
        const auto file_size = std::filesystem::file_size(fs_path);
        std::string content;
        content.resize(file_size);
        file_stream.read(content.data(), file_size);
        return content;
    }
    std::vector<std::string_view> split_lines(std::string_view str)
    {
        std::vector<std::string_view> result;
        size_t start = 0;
        while (start < str.size())
        {
            size_t end = str.find('\n', start);
            if (end == std::string_view::npos)
            {
                result.emplace_back(str.substr(start));
                break;
            }
            size_t line_end = end;
            if (line_end > start && str[line_end - 1] == '\r')
            {
                line_end -= 1;
            }
            result.emplace_back(str.substr(start, line_end - start));
            start = end + 1;
        }
        return result;
    }
    std::string_view ltrim(std::string_view sv)
    {
        size_t i = 0;
        while (i < sv.size() && (sv[i] == ' ' || sv[i] == '\t'))
        {
            ++i;
        }
        return sv.substr(i);
    }
    bool starts_with(std::string_view sv, std::string_view prefix)
    {
        return sv.size() >= prefix.size() && sv.compare(0, prefix.size(), prefix) == 0;
    }
    struct FileEntry
    {
        std::string name{};
        std::uint64_t size{};
        std::uint64_t hash{};
        std::unique_ptr<std::string> content{};
        friend bool operator<(const FileEntry& a, const FileEntry& b)
        {
            if (a.name != b.name)
                return a.name < b.name;
            if (a.size != b.size)
                return a.size < b.size;
            if (a.hash != b.hash)
                return a.hash < b.hash;
            return *a.content < *b.content;
        }
    };
    FileEntry make_entry(std::filesystem::path fs_path)
    {
        auto file_content = read_file(fs_path);
        FileEntry entry{};
        entry.name = fs_path.filename().string();
        entry.size = file_content.size();
        entry.hash = std::hash<std::string>{}(file_content);
        entry.content = std::make_unique<std::string>(std::move(file_content));
        return entry;
    }
    struct Context
    {
        std::filesystem::path include_path{};
        std::deque<std::string> file_content{};
        std::set<FileEntry> file_entries{};
        std::set<std::string> std_header{};
    };
    std::vector<std::string_view> collect_file(std::string_view file_path, Context& context)
    {
        std::vector<std::string_view> result{};
        std::string file_name_comment = "// " + std::string(file_path);
        auto& comment = context.file_content.emplace_back(file_name_comment);
        result.emplace_back(comment);
        std::filesystem::path fs_path = file_path;
        SOPHO_ASSERT(std::filesystem::exists(fs_path), "file not exist ", fs_path.string());
        auto [iter, inserted] = context.file_entries.emplace(make_entry(fs_path));
        if (!inserted)
        {
            return {};
        }
        auto lines = split_lines(std::string_view(*iter->content));
        for (const auto& line : lines)
        {
            auto line_content = ltrim(line);
            if (line_content.length() == 0)
            {
                continue;
            }
            if (line_content[0] != '#')
            {
                result.emplace_back(line);
                continue;
            }
            line_content = line_content.substr(1);
            line_content = ltrim(line_content);
            if (starts_with(line_content, "include"))
            {
                line_content = line_content.substr(7);
                line_content = ltrim(line_content);
                if (line_content[0] == '<')
                {
                    line_content = line_content.substr(1);
                    auto index = line_content.find('>');
                    SOPHO_ASSERT(index != std::string_view::npos, "find > failed");
                    auto file_name = line_content.substr(0, index);
                    if (context.std_header.find(std::string(file_name)) != context.std_header.end())
                    {
                        continue;
                    }
                    context.std_header.emplace(file_name);
                    result.emplace_back(line);
                    continue;
                }
                else if (line_content[0] == '"')
                {
                    line_content = line_content.substr(1);
                    auto index = line_content.find('"');
                    SOPHO_ASSERT(index != std::string_view::npos, "find \" failed");
                    std::string file_name{line_content.substr(0, index)};
                    std::filesystem::path new_fs_path = fs_path.parent_path() / file_name;
                    if (!std::filesystem::exists(new_fs_path))
                    {
                        new_fs_path = context.include_path / file_name;
                    }
                    auto file_content = collect_file(std::string_view(new_fs_path.string()), context);
                    result.insert(result.end(), file_content.begin(), file_content.end());
                    result.emplace_back(comment);
                }
                else
                {
                    result.emplace_back(line);
                }
            }
            else if (starts_with(line_content, "pragma"))
            {
                line_content = line_content.substr(6);
                line_content = ltrim(line_content);
                if (starts_with(line_content, "once"))
                {
                    continue;
                }
                result.emplace_back(line);
            }
            else
            {
                result.emplace_back(line);
            }
        }
        return result;
    }
    void single_header_generator(std::string_view file_path)
    {
        SOPHO_STACK();
        Context context{};
        std::filesystem::path fs_path = file_path;
        SOPHO_VALUE(fs_path);
        SOPHO_ASSERT(std::filesystem::exists(fs_path), "file not exist");
        context.include_path = fs_path.parent_path();
        auto lines = collect_file(file_path, context);
        std::ofstream out("sob.hpp", std::ios::binary);
        SOPHO_ASSERT(out.is_open(), "open file failed");
        for (auto sv : lines)
        {
            out.write(sv.data(), sv.size());
            out.put('\n');
        }
    }
} // namespace sopho
// include/sob.hpp
// include/sob.hpp
// include/static_string.hpp
#include <array>
namespace sopho
{
    template <std::size_t Size>
    struct StaticString
    {
        std::array<char, Size> raw{};
        constexpr StaticString() = default;
        constexpr StaticString(const char (&str)[Size + 1])
        {
            for (std::size_t i = 0; i < Size; ++i)
            {
                raw[i] = str[i];
            }
        }
        constexpr std::size_t size() const { return Size; }
        constexpr std::string_view view() const { return std::string_view{raw.data(), size()}; }
        constexpr char operator[](std::size_t idx) const { return raw[idx]; }
        template <std::size_t M>
        constexpr bool has_suffix(const StaticString<M>& suffix) const
        {
            // Case: Suffix is longer than the string -> definitely false
            if (M > Size)
            {
                return false;
            }
            // Architecture Note: We implement manual loop instead of string_view::ends_with
            // to ensure maximum compatibility with older C++ standards (C++14/17)
            // and guarantee constexpr execution.
            for (std::size_t i = 0; i < M; ++i)
            {
                if (raw[Size - M + i] != suffix[i])
                {
                    return false;
                }
            }
            return true;
        }
        template <std::size_t M>
        constexpr StaticString<Size - M> strip_suffix() const
        {
            static_assert(M <= Size, "Suffix is longer than the string itself");
            StaticString<Size - M> result{};
            for (std::size_t i = 0; i < Size - M; ++i)
            {
                result.raw[i] = raw[i];
            }
            return result;
        }
        template <std::size_t M>
        constexpr StaticString<Size + M> append(StaticString<M> suffix) const
        {
            StaticString<Size + M> result{};
            for (std::size_t i = 0; i < Size; ++i)
            {
                result.raw[i] = raw[i];
            }
            for (std::size_t i = 0; i < M; ++i)
            {
                result.raw[i + Size] = suffix[i];
            }
            return result;
        }
    };
    template <std::size_t N>
    StaticString(const char (&)[N]) -> StaticString<N - 1>;
} // namespace sopho
// include/sob.hpp
template <class T>
constexpr std::string_view type_name()
{
#if defined(__clang__) || defined(__GNUC__)
    // __PRETTY_FUNCTION__ contains the type name in a compiler-specific format.
    std::string_view p = __PRETTY_FUNCTION__;
    // Example (clang): "constexpr std::string_view type_name() [T = int]"
    auto start = p.find("T = ");
    start = (start == std::string_view::npos) ? 0 : start + 4;
    // GCC/Clang format contains "; ..." after T = Type
    auto end = p.find(';', start);
    if (end == std::string_view::npos)
        end = p.find(']', start);
    return p.substr(start, end - start);
#elif defined(_MSC_VER)
    // __FUNCSIG__ contains the type name in a compiler-specific format.
    std::string_view p = __FUNCSIG__;
    // Example (MSVC): "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl type_name<int>(void)"
    auto start = p.find("type_name<");
    start = (start == std::string_view::npos) ? 0 : start + 10;
    auto end = p.find(">(void)", start);
    return p.substr(start, end - start);
#else
    return "unknown";
#endif
}
namespace sopho
{
    // Generic detection idiom core
    template <typename, template <typename> class, typename = void>
    struct is_detected : std::false_type
    {
    };
    template <typename T, template <typename> class Op>
    struct is_detected<T, Op, std::void_t<Op<T>>> : std::true_type
    {
    };
    template <typename T, template <typename> class Op>
    inline constexpr bool is_detected_v = is_detected<T, Op>::value;
    // Expression template: get type of 'T::source' (works for static and non-static)
    template <typename T>
    using detect_source = decltype(std::declval<T&>().source);
    // Final trait: has_source_v<T>
    template <typename T>
    inline constexpr bool has_source_v = is_detected_v<T, detect_source>;
    template <typename T>
    using detect_ldflags = decltype(std::declval<T&>().ldflags);
    template <typename T>
    inline constexpr bool has_ldflags_v = is_detected_v<T, detect_ldflags>;
    template <typename T>
    using detect_cxxflags = decltype(std::declval<T&>().cxxflags);
    template <typename T>
    inline constexpr bool has_cxxflags_v = is_detected_v<T, detect_cxxflags>;
    template <typename T>
    using detect_dependent_type = typename T::Dependent;
    template <typename T>
    inline constexpr bool has_dependent_v = is_detected_v<T, detect_dependent_type>;
    template <typename T, typename = void>
    struct dependent_or_empty
    {
        using type = std::tuple<>;
    };
    template <typename T>
    struct dependent_or_empty<T, std::void_t<typename T::Dependent>>
    {
        using type = typename T::Dependent;
    };
    template <typename T>
    using dependent_or_empty_t = typename dependent_or_empty<T>::type;
    template <typename Context>
    struct CxxToolchain
    {
        template <typename Target>
        struct CxxBuilder;
        template <size_t Size>
        constexpr static auto source_to_target(StaticString<Size> source)
        {
            return Context::build_prefix.append(source.template strip_suffix<4>().append(Context::obj_postfix));
        }
        template <typename Target>
        struct CxxBuilderWrapper
        {
            using type = CxxBuilder<Target>;
        };
        template <typename Target>
        struct CxxBuilder
        {
            template <typename L, typename R>
            struct BuildFolder
            {
                struct Builder
                {
                    static void build()
                    {
                        L::build();
                        R::build();
                    }
                };
                using type = Builder;
            };
            struct DumbBuilder
            {
                static void build() {}
            };
            using DependentBuilder =
                Foldl<BuildFolder, DumbBuilder, Map<CxxBuilderWrapper, dependent_or_empty_t<Target>>>;
            template <typename T>
            struct SourceToTarget
            {
                struct Result
                {
                    constexpr static auto target = source_to_target(T::source);
                };
                using type = Result;
            };
            template <typename L, typename R>
            struct TargetStringFolder
            {
                struct TargetString
                {
                    static constexpr auto target = L::target.append(StaticString(" ")).append(R::target);
                };
                using type = TargetString;
            };
            struct DumbTargetString
            {
                static constexpr StaticString target{""};
            };
            using DependentNameCollector =
                Foldl<TargetStringFolder, DumbTargetString, Map<SourceToTarget, dependent_or_empty_t<Target>>>;
            static void build()
            {
                DependentBuilder::build();
                std::string command{};
                std::stringstream ss{};
                ss << Context::cxx;
                if constexpr (has_source_v<Target>)
                {
                    static_assert(!Target::source.view().empty(), "Source file cannot be empty");
                    auto target = source_to_target(Target::source);
                    ss << " -c " << Target::source.view() << Context::obj_prefix.view() << target.view();
                    std::filesystem::path target_path{target.view()};
                    std::filesystem::create_directories(target_path.parent_path());
                    if constexpr (has_cxxflags_v<Context>)
                    {
                        for (const auto& flag : Context::cxxflags)
                        {
                            ss << " " << flag;
                        }
                    }
                }
                else
                {
                    static_assert(std::tuple_size_v<typename Target::Dependent> > 0,
                                  "Link target must have dependencies (object files)");
                    ss << DependentNameCollector::target.view();
                    ss << Context::bin_prefix.view() << Target::target.view();
                    if constexpr (has_ldflags_v<Context>)
                    {
                        for (const auto& flag : Context::ldflags)
                        {
                            ss << " " << flag;
                        }
                    }
                }
                command = ss.str();
                std::cout << type_name<Target>() << ":" << command << std::endl;
                std::system(command.data());
                std::cout << type_name<Target>() << ":finished" << std::endl;
            }
        };
    };
} // namespace sopho
