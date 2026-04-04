/**
 * benchmark.cpp — Runtime and memory benchmarking for polygon simplification.
 *
 * For each test case, forks the simplifier as a child process, measures
 * wall-clock time via std::chrono and peak RSS via wait4()/getrusage.
 * Outputs two Vega-Lite charts (runtime and memory) to an HTML file.
 *
 * Usage:
 *   build/benchmark [trials]       (default: 5 trials per test case)
 *
 * Assumes build/simplify exists in the current working directory.
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ─── Test case definitions ──────────────────────────────────────────────────

struct TestCase {
    std::string name;
    std::string input_file;
    int         target;
};

static const std::vector<TestCase> ALL_CASES = {
    {"rectangle_with_two_holes",      "test_cases/input_rectangle_with_two_holes.csv",      7},
    {"cushion_with_hexagonal_hole",   "test_cases/input_cushion_with_hexagonal_hole.csv",  13},
    {"blob_with_two_holes",           "test_cases/input_blob_with_two_holes.csv",          17},
    {"wavy_with_three_holes",         "test_cases/input_wavy_with_three_holes.csv",        21},
    {"lake_with_two_islands",         "test_cases/input_lake_with_two_islands.csv",        17},
    {"original_01",                   "test_cases/input_original_01.csv",                  99},
    {"original_02",                   "test_cases/input_original_02.csv",                  99},
    {"original_03",                   "test_cases/input_original_03.csv",                  99},
    {"original_04",                   "test_cases/input_original_04.csv",                  99},
    {"original_05",                   "test_cases/input_original_05.csv",                  99},
    {"original_06",                   "test_cases/input_original_06.csv",                  99},
    {"original_07",                   "test_cases/input_original_07.csv",                  99},
    {"original_08",                   "test_cases/input_original_08.csv",                  99},
    {"original_09",                   "test_cases/input_original_09.csv",                  99},
    {"original_10",                   "test_cases/input_original_10.csv",                  99},
};

// ─── Helpers ────────────────────────────────────────────────────────────────

static int count_vertices(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return 0;
    std::string line;
    std::getline(f, line); // skip header
    int count = 0;
    while (std::getline(f, line))
        if (!line.empty()) ++count;
    return count;
}

struct BenchResult {
    std::string name;
    int         input_vertices;
    long long   time_us;   // median wall-clock time in microseconds
    long long   memory_kb; // peak RSS in kilobytes
};

static BenchResult run_benchmark(const std::string& simplify_path,
                                 const TestCase& tc, int trials) {
    int n_verts = count_vertices(tc.input_file);

    std::vector<long long> times;
    times.reserve(trials);
    long long peak_mem_kb = 0;

    std::string target_str = std::to_string(tc.target);

    for (int t = 0; t < trials; ++t) {
        auto start = std::chrono::steady_clock::now();

        std::cout.flush();
        pid_t pid = fork();
        if (pid == 0) {
            // Child: suppress output
            if (!freopen("/dev/null", "w", stdout)) _exit(127);
            if (!freopen("/dev/null", "w", stderr)) _exit(127);
            execl(simplify_path.c_str(), simplify_path.c_str(),
                  tc.input_file.c_str(), target_str.c_str(), nullptr);
            _exit(127);
        }

        int status;
        struct rusage ru{};
        wait4(pid, &status, 0, &ru);

        auto end = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(
                           end - start)
                           .count();
        times.push_back(us);

        // ru_maxrss is in KB on Linux
        if (ru.ru_maxrss > peak_mem_kb)
            peak_mem_kb = ru.ru_maxrss;
    }

    std::sort(times.begin(), times.end());
    long long median_us = times[trials / 2];

    return {tc.name, n_verts, median_us, peak_mem_kb};
}

// ─── Vega-Lite HTML generation ──────────────────────────────────────────────

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string make_runtime_spec(const std::vector<BenchResult>& results) {
    std::ostringstream values;
    values << '[';
    bool first = true;
    for (auto& r : results) {
        if (!first) values << ',';
        first = false;
        values << '{'
               << "\"Test Case\":\"" << r.name << "\","
               << "\"Input Vertices\":" << r.input_vertices << ','
               << "\"Running Time (\\u00b5s)\":" << r.time_us
               << '}';
    }
    values << ']';

    std::string enc_x =
        "\"x\":{\"field\":\"Test Case\",\"type\":\"nominal\","
        "\"sort\":{\"field\":\"Input Vertices\"},"
        "\"axis\":{\"titleFontSize\":16,\"labelFontSize\":11,\"labelAngle\":-45}}";
    std::string enc_y =
        "\"y\":{\"field\":\"Running Time (\\u00b5s)\",\"type\":\"quantitative\","
        "\"title\":\"Median running time (\\u00b5s)\","
        "\"axis\":{\"titleFontSize\":16,\"labelFontSize\":14}}";

    std::ostringstream spec;
    spec << '{'
         << "\"$schema\":\"https://vega.github.io/schema/vega-lite/v5.json\","
         << "\"title\":{\"text\":\"Polygon Simplification — Running Time\",\"fontSize\":20},"
         << "\"width\":800,\"height\":400,"
         << "\"data\":{\"values\":" << values.str() << "},"
         << "\"layer\":["
         <<   "{"
         <<     "\"mark\":{\"type\":\"bar\",\"tooltip\":true},"
         <<     "\"encoding\":{"
         <<       enc_x << "," << enc_y << ","
         <<       "\"color\":{\"field\":\"Input Vertices\",\"type\":\"quantitative\","
                    "\"scale\":{\"scheme\":\"viridis\"},"
                    "\"legend\":{\"titleFontSize\":14,\"labelFontSize\":12}},"
         <<       "\"tooltip\":["
                    "{\"field\":\"Test Case\",\"type\":\"nominal\"},"
                    "{\"field\":\"Input Vertices\",\"type\":\"quantitative\"},"
                    "{\"field\":\"Running Time (\\u00b5s)\",\"type\":\"quantitative\"}"
                  "]"
         <<     "}"
         <<   "},"
         <<   "{"
         <<     "\"mark\":{\"type\":\"text\",\"dy\":-8,\"fontSize\":11},"
         <<     "\"encoding\":{"
         <<       enc_x << "," << enc_y << ","
         <<       "\"text\":{\"field\":\"Running Time (\\u00b5s)\",\"type\":\"quantitative\"}"
         <<     "}"
         <<   "}"
         << "]"
         << '}';

    return spec.str();
}

static std::string make_memory_spec(const std::vector<BenchResult>& results) {
    std::ostringstream values;
    values << '[';
    bool first = true;
    for (auto& r : results) {
        if (!first) values << ',';
        first = false;
        values << '{'
               << "\"Test Case\":\"" << r.name << "\","
               << "\"Input Vertices\":" << r.input_vertices << ','
               << "\"Peak Memory (KB)\":" << r.memory_kb
               << '}';
    }
    values << ']';

    std::string enc_x =
        "\"x\":{\"field\":\"Test Case\",\"type\":\"nominal\","
        "\"sort\":{\"field\":\"Input Vertices\"},"
        "\"axis\":{\"titleFontSize\":16,\"labelFontSize\":11,\"labelAngle\":-45}}";
    std::string enc_y =
        "\"y\":{\"field\":\"Peak Memory (KB)\",\"type\":\"quantitative\","
        "\"title\":\"Peak RSS (KB)\","
        "\"axis\":{\"titleFontSize\":16,\"labelFontSize\":14}}";

    std::ostringstream spec;
    spec << '{'
         << "\"$schema\":\"https://vega.github.io/schema/vega-lite/v5.json\","
         << "\"title\":{\"text\":\"Polygon Simplification — Peak Memory Usage\",\"fontSize\":20},"
         << "\"width\":800,\"height\":400,"
         << "\"data\":{\"values\":" << values.str() << "},"
         << "\"layer\":["
         <<   "{"
         <<     "\"mark\":{\"type\":\"bar\",\"tooltip\":true},"
         <<     "\"encoding\":{"
         <<       enc_x << "," << enc_y << ","
         <<       "\"color\":{\"field\":\"Input Vertices\",\"type\":\"quantitative\","
                    "\"scale\":{\"scheme\":\"viridis\"},"
                    "\"legend\":{\"titleFontSize\":14,\"labelFontSize\":12}},"
         <<       "\"tooltip\":["
                    "{\"field\":\"Test Case\",\"type\":\"nominal\"},"
                    "{\"field\":\"Input Vertices\",\"type\":\"quantitative\"},"
                    "{\"field\":\"Peak Memory (KB)\",\"type\":\"quantitative\"}"
                  "]"
         <<     "}"
         <<   "},"
         <<   "{"
         <<     "\"mark\":{\"type\":\"text\",\"dy\":-8,\"fontSize\":11},"
         <<     "\"encoding\":{"
         <<       enc_x << "," << enc_y << ","
         <<       "\"text\":{\"field\":\"Peak Memory (KB)\",\"type\":\"quantitative\"}"
         <<     "}"
         <<   "}"
         << "]"
         << '}';

    return spec.str();
}

static void generate_html(const std::string& runtime_spec,
                           const std::string& memory_spec,
                           const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Could not open file: " + filename);

    f << "<!DOCTYPE html>\n"
         "<html>\n"
         "  <head>\n"
         "    <meta charset=\"utf-8\"/>\n"
         "    <title>Polygon Simplification Benchmark</title>\n"
         "    <script src=\"https://cdn.jsdelivr.net/npm/vega@5\"></script>\n"
         "    <script src=\"https://cdn.jsdelivr.net/npm/vega-lite@5\"></script>\n"
         "    <script src=\"https://cdn.jsdelivr.net/npm/vega-embed@6\"></script>\n"
         "    <style>\n"
         "      body { font-family: sans-serif; max-width: 960px; margin: 2em auto; }\n"
         "      h1 { text-align: center; }\n"
         "      .chart { margin: 2em 0; }\n"
         "    </style>\n"
         "  </head>\n"
         "  <body>\n"
         "    <h1>Polygon Simplification Benchmark</h1>\n"
         "    <div id=\"runtime\" class=\"chart\"></div>\n"
         "    <div id=\"memory\" class=\"chart\"></div>\n"
         "    <script type=\"text/javascript\">\n"
         "      const runtimeSpec = JSON.parse(\""
      << json_escape(runtime_spec)
      << "\");\n"
         "      const memorySpec = JSON.parse(\""
      << json_escape(memory_spec)
      << "\");\n"
         "      vegaEmbed('#runtime', runtimeSpec);\n"
         "      vegaEmbed('#memory', memorySpec);\n"
         "    </script>\n"
         "  </body>\n"
         "</html>\n";
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    int trials = 5;
    if (argc >= 2) {
        try {
            trials = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Usage: benchmark [trials]\n";
            return 1;
        }
        if (trials < 1) trials = 1;
    }

    const std::string simplify = "build/simplify";

    // Verify simplify binary exists
    if (access(simplify.c_str(), X_OK) != 0) {
        std::cerr << "Error: " << simplify << " not found. Run 'make' first.\n";
        return 1;
    }

    std::cout << "Polygon simplification benchmark (median of " << trials
              << " trials)\n\n";
    std::cout << std::left
              << std::setw(35) << "Test Case"
              << std::setw(12) << "Vertices"
              << std::setw(16) << "Time (\u00b5s)"
              << std::setw(14) << "Memory (KB)"
              << '\n';
    std::cout << std::string(77, '-') << '\n';

    std::vector<BenchResult> results;
    results.reserve(ALL_CASES.size());

    for (auto& tc : ALL_CASES) {
        BenchResult r = run_benchmark(simplify, tc, trials);
        results.push_back(r);

        std::cout << std::left
                  << std::setw(35) << r.name
                  << std::setw(12) << r.input_vertices
                  << std::setw(16) << r.time_us
                  << std::setw(14) << r.memory_kb
                  << '\n';
    }

    // Generate HTML report
    std::string runtime_spec = make_runtime_spec(results);
    std::string memory_spec  = make_memory_spec(results);

    std::string out_dir  = "my_output";
    std::string html_file = out_dir + "/benchmark.html";

    generate_html(runtime_spec, memory_spec, html_file);
    std::cout << "\nWrote benchmark charts to " << html_file << '\n';

    return 0;
}
