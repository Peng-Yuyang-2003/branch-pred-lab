/*
 * Multi-branch microbenchmark for gem5 branch-predictor labs.
 *
 * The program intentionally keeps the working set and branch behavior under
 * command-line control so students can relate application behavior to gem5
 * branch-predictor statistics and overall CPU performance.
 */

#include <cstdint>
#include <cstdlib>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline, optimize("no-if-conversion")))
#else
#define NOINLINE
#endif

namespace
{

constexpr std::uint64_t TraceOutcomes = 100;
constexpr std::size_t BranchSites = 8;

enum class Scenario
{
    Predictable,
    Tournament,
    Bimode,
    Tage,
};

struct Options
{
    std::uint64_t iterations = 200000;
    std::uint64_t period = 16;
    std::uint64_t threshold = 8;
    std::uint64_t history_len = 32;
    bool trace = false;
    std::size_t working_set = 4096;
    std::uint64_t seed = 1;
    Scenario scenario = Scenario::Predictable;
    std::string scenario_name = "predictable";
};

struct Trace
{
    explicit Trace(bool enabled)
        : enabled(enabled)
    {
    }

    void add(std::size_t site, bool taken)
    {
        if (!enabled || printed == TraceOutcomes) {
            return;
        }
        outcomes[site].push_back(taken ? 'Y' : 'N');
        ++printed;
    }

    void finish() const
    {
        if (!enabled) {
            return;
        }
        std::cout << "outcomes_first_" << TraceOutcomes
                  << "_dynamic_branches_by_site:\n";
        for (std::size_t site = 0; site < BranchSites; ++site) {
            std::cout << "branch" << site << "=" << outcomes[site] << "\n";
        }
    }

    std::array<std::string, BranchSites> outcomes;
    std::uint64_t printed = 0;
    bool enabled;
};

void
usage(const char *prog)
{
    std::cerr
        << "Usage: " << prog << " [options]\n"
        << "Options:\n"
        << "  --iterations N   loop iterations, default 200000\n"
        << "  --scenario NAME  predictable | tournament | bimode | tage\n"
        << "  --period N       chunk length for scenario state, default 16\n"
        << "  --threshold N    kept for compatibility with older lab commands\n"
        << "  --history-len N  dependency distance for tage scenario, default 32\n"
        << "  --trace          print the first 100 dynamic branch outcomes by branch site\n"
        << "  --working-set N  number of int elements touched, default 4096\n"
        << "  --seed N         data initialization seed, default 1\n";
}

std::uint64_t
parse_u64(const char *text, const char *name)
{
    char *end = nullptr;
    const auto value = std::strtoull(text, &end, 0);
    if (end == text || *end != '\0') {
        std::cerr << "Invalid integer for " << name << ": " << text << "\n";
        std::exit(2);
    }
    return value;
}

Scenario
parse_scenario(const std::string &name)
{
    if (name == "predictable" || name == "biased" || name == "periodic") {
        return Scenario::Predictable;
    }
    if (name == "tournament" || name == "alternating") {
        return Scenario::Tournament;
    }
    if (name == "bimode") {
        return Scenario::Bimode;
    }
    if (name == "tage" || name == "history") {
        return Scenario::Tage;
    }
    std::cerr << "Unsupported scenario: " << name << "\n";
    std::exit(2);
}

Options
parse_args(int argc, char **argv)
{
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                usage(argv[0]);
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--iterations") {
            opts.iterations = parse_u64(require_value("--iterations"), "--iterations");
        } else if (arg == "--scenario") {
            opts.scenario_name = require_value("--scenario");
            opts.scenario = parse_scenario(opts.scenario_name);
        } else if (arg == "--pattern") {
            opts.scenario_name = require_value("--pattern");
            opts.scenario = parse_scenario(opts.scenario_name);
        } else if (arg == "--period") {
            opts.period = parse_u64(require_value("--period"), "--period");
        } else if (arg == "--threshold") {
            opts.threshold = parse_u64(require_value("--threshold"), "--threshold");
        } else if (arg == "--history-len") {
            opts.history_len = parse_u64(require_value("--history-len"), "--history-len");
        } else if (arg == "--trace") {
            opts.trace = true;
        } else if (arg == "--working-set") {
            opts.working_set = static_cast<std::size_t>(
                parse_u64(require_value("--working-set"), "--working-set"));
        } else if (arg == "--seed") {
            opts.seed = parse_u64(require_value("--seed"), "--seed");
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            usage(argv[0]);
            std::exit(2);
        }
    }

    if (opts.period == 0 || opts.history_len == 0 || opts.working_set == 0) {
        std::cerr << "--period, --history-len and --working-set must be non-zero\n";
        std::exit(2);
    }
    if ((opts.working_set & (opts.working_set - 1)) != 0) {
        std::cerr << "--working-set must be a power of two\n";
        std::exit(2);
    }
    return opts;
}

#define BRANCH_SITE(N)                                                        \
    NOINLINE void branch_site_##N(                                            \
        bool taken, std::vector<int> &data, std::size_t idx,                  \
        volatile std::uint64_t &sink)                                         \
    {                                                                         \
        if (taken) {                                                          \
            data[idx] = data[idx] * (3 + N) + (1 + N);                        \
            sink += static_cast<std::uint64_t>(data[idx] & (0xff + N));       \
        } else {                                                              \
            data[idx] = data[idx] / (3 + N) + (7 + N);                        \
            sink ^= static_cast<std::uint64_t>(data[idx] & (0xff + N))        \
                    << (N & 7);                                               \
        }                                                                     \
    }

BRANCH_SITE(0)
BRANCH_SITE(1)
BRANCH_SITE(2)
BRANCH_SITE(3)
BRANCH_SITE(4)
BRANCH_SITE(5)
BRANCH_SITE(6)
BRANCH_SITE(7)

#undef BRANCH_SITE

#define RUN_SITE(N, TAKEN_EXPR)                                               \
    do {                                                                      \
        const bool taken_##N = (TAKEN_EXPR);                                  \
        trace.add(N, taken_##N);                                              \
        branch_site_##N(                                                      \
            taken_##N, data, (base_idx + (N * 257U)) & index_mask, sink);     \
    } while (false)

void
run_predictable(const Options &opts, std::vector<int> &data, Trace &trace)
{
    volatile std::uint64_t sink = 0;
    const std::size_t index_mask = data.size() - 1;
    std::uint64_t phase = 0;

    for (std::uint64_t i = 0; i < opts.iterations; ++i) {
        const std::size_t base_idx = (i * 1103515245ULL + opts.seed) & index_mask;
        const bool rare = phase == 0;
        ++phase;
        if (phase == opts.period) {
            phase = 0;
        }

        RUN_SITE(0, true);
        RUN_SITE(1, false);
        RUN_SITE(2, true);
        RUN_SITE(3, false);
        RUN_SITE(4, !rare);
        RUN_SITE(5, rare);
        RUN_SITE(6, true);
        RUN_SITE(7, false);
    }

    std::cout << "sink=" << sink << "\n";
}

void
run_tournament(const Options &opts, std::vector<int> &data, Trace &trace)
{
    volatile std::uint64_t sink = 0;
    const std::size_t index_mask = data.size() - 1;
    std::uint64_t phase = 0;

    for (std::uint64_t i = 0; i < opts.iterations; ++i) {
        const std::size_t base_idx = (i * 1103515245ULL + opts.seed) & index_mask;
        phase = (phase + 1) & 7U;

        RUN_SITE(0, (phase & 1U) != 0);
        RUN_SITE(1, (phase & 1U) == 0);
        RUN_SITE(2, (phase & 2U) != 0);
        RUN_SITE(3, (phase & 2U) == 0);
        RUN_SITE(4, (phase & 3U) == 0);
        RUN_SITE(5, (phase & 3U) == 1);
        RUN_SITE(6, (phase & 4U) != 0);
        RUN_SITE(7, (phase & 4U) == 0);
    }

    std::cout << "sink=" << sink << "\n";
}

void
run_bimode(const Options &opts, std::vector<int> &data, Trace &trace)
{
    volatile std::uint64_t sink = 0;
    const std::size_t index_mask = data.size() - 1;
    std::uint64_t phase = 0;
    bool mode = true;

    for (std::uint64_t i = 0; i < opts.iterations; ++i) {
        const std::size_t base_idx = (i * 1103515245ULL + opts.seed) & index_mask;
        const bool rare = (i & 15U) == 0;
        ++phase;
        if (phase == opts.period) {
            phase = 0;
            mode = !mode;
        }

        RUN_SITE(0, mode);
        RUN_SITE(1, mode ? !rare : rare);
        RUN_SITE(2, mode ? !rare : rare);
        RUN_SITE(3, mode ? rare : !rare);
        RUN_SITE(4, mode ? rare : !rare);
        RUN_SITE(5, mode ? !rare : rare);
        RUN_SITE(6, mode ? rare : !rare);
        RUN_SITE(7, !mode);
    }

    std::cout << "sink=" << sink << "\n";
}

void
run_tage(const Options &opts, std::vector<int> &data, Trace &trace)
{
    volatile std::uint64_t sink = 0;
    const std::size_t index_mask = data.size() - 1;
    const std::uint64_t patterns[4] = {
        0x9696969669696969ULL,
        0xe1e1e1e11e1e1e1eULL,
        0xf0f00f0f0f0ff0f0ULL,
        0xcc33cc3333cc33ccULL,
    };
    std::uint64_t phase = 0;

    auto decide = [&](std::uint64_t salt) {
        const std::uint64_t pattern = patterns[salt & 3U];
        const std::uint64_t bit = (phase + salt * 9U) & 63U;
        return ((pattern >> bit) & 1U) != 0;
    };

    for (std::uint64_t i = 0; i < opts.iterations; ++i) {
        const std::size_t base_idx = (i * 1103515245ULL + opts.seed) & index_mask;
        phase = (phase + 1) & 127U;

        RUN_SITE(0, decide(0));
        RUN_SITE(1, decide(1));
        RUN_SITE(2, decide(2));
        RUN_SITE(3, decide(3));
        RUN_SITE(4, decide(4));
        RUN_SITE(5, decide(5));
        RUN_SITE(6, decide(6));
        RUN_SITE(7, decide(7));
    }

    std::cout << "sink=" << sink << "\n";
}

#undef RUN_SITE

} // namespace

int
main(int argc, char **argv)
{
    const auto opts = parse_args(argc, argv);
    std::vector<int> data(opts.working_set);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<int>((i * 1315423911ULL + opts.seed) & 0x7fffffff);
    }

    Trace trace(opts.trace);
    switch (opts.scenario) {
      case Scenario::Predictable:
        run_predictable(opts, data, trace);
        break;
      case Scenario::Tournament:
        run_tournament(opts, data, trace);
        break;
      case Scenario::Bimode:
        run_bimode(opts, data, trace);
        break;
      case Scenario::Tage:
        run_tage(opts, data, trace);
        break;
    }
    trace.finish();

    std::uint64_t checksum = 0;
    for (std::size_t i = 0; i < data.size(); i += 64) {
        checksum += static_cast<std::uint64_t>(data[i]);
    }

    std::cout << "scenario=" << opts.scenario_name
              << " iterations=" << opts.iterations
              << " period=" << opts.period
              << " history_len=" << opts.history_len
              << " working_set=" << opts.working_set
              << " checksum=" << checksum << "\n";
    return 0;
}
