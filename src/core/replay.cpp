#include "core/replay.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <system_error>
#include <utility>

#include "core/text.h"

namespace aim::core {

namespace {

constexpr std::array<std::string_view, 5> kButtonWords{"left", "middle", "right", "x1", "x2"};

std::string_view button_word(MouseButton b) {
    return kButtonWords.at(static_cast<std::size_t>(b));
}

std::optional<MouseButton> parse_button(std::string_view word) {
    for (std::size_t i = 0; i < kButtonWords.size(); ++i)
        if (kButtonWords.at(i) == word) return static_cast<MouseButton>(i);
    return std::nullopt;
}

std::optional<std::int64_t> parse_i64(std::string_view text) {
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// Строка шага; не шаг — пусто.
std::optional<ReplayStep> parse_step(std::string_view line) {
    const std::vector<std::string_view> f = split(line, ',');
    if (f.size() < 3) return std::nullopt;
    ReplayStep step;
    const std::optional<std::int64_t> at = parse_i64(f[1]);
    if (!at) return std::nullopt;
    step.at = *at;
    if (f[0] == "u" && f.size() == 4) {
        const auto x = parse_real(f[2]);
        const auto y = parse_real(f[3]);
        if (!x || !y) return std::nullopt;
        step.kind = ReplayStepKind::Update;
        step.time = step.at;
        step.point = {*x, *y};
        return step;
    }
    const std::optional<std::int64_t> time = parse_i64(f[2]);
    if (!time) return std::nullopt;
    step.time = *time;
    if (f[0] == "p" && f.size() == 3) {
        step.kind = ReplayStepKind::Shown;
        return step;
    }
    if (f[0] == "c" && f.size() == 6) {
        const auto button = parse_button(f[3]);
        const auto x = parse_real(f[4]);
        const auto y = parse_real(f[5]);
        if (!button || !x || !y) return std::nullopt;
        step.kind = ReplayStepKind::Click;
        step.button = *button;
        step.point = {*x, *y};
        return step;
    }
    if (f[0] == "e" && f.size() == 4) {
        const auto reason = parse_end(f[3]);
        if (!reason) return std::nullopt;
        step.kind = ReplayStepKind::Finish;
        step.reason = *reason;
        return step;
    }
    return std::nullopt;
}

}  // namespace

std::string replay_file_name(int profile, const MeasureOptions& options) {
    std::string key = options_key(options);
    std::replace(key.begin(), key.end(), ';', '_');
    return std::format("profile{}_{}.replay", profile, key);
}

bool is_replay_file(std::string_view name) {
    if (!name.starts_with("profile") || !name.ends_with(".replay")) return false;
    const std::string_view rest = name.substr(7);
    const std::size_t sep = rest.find('_');
    if (sep == 0 || sep == std::string_view::npos) return false;
    return std::all_of(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(sep),
                       [](char c) { return c >= '0' && c <= '9'; });
}

std::string write_replay(const Replay& replay, std::string_view newline) {
    std::string out;
    const auto line = [&](std::string_view text) {
        out += text;
        out += newline;
    };
    line(kReplayVersionLine);
    line(kRunsHeader);
    line(run_line(replay.run));
    // Вещественные — кратчайшей записью, которая читается в то же число:
    // курсор и точки щелчков повторяются точно.
    for (const ReplayStep& s : replay.steps) {
        switch (s.kind) {
            case ReplayStepKind::Click:
                line(std::format("c,{},{},{},{},{}", s.at, s.time, button_word(s.button), s.point.x,
                                 s.point.y));
                break;
            case ReplayStepKind::Update:
                line(std::format("u,{},{},{}", s.at, s.point.x, s.point.y));
                break;
            case ReplayStepKind::Shown: line(std::format("p,{},{}", s.at, s.time)); break;
            case ReplayStepKind::Finish:
                line(std::format("e,{},{},{}", s.at, s.time, end_word(s.reason)));
                break;
        }
    }
    return out;
}

std::optional<Replay> read_replay(std::string_view text) {
    const std::vector<std::string_view> lines = split_lines(text);
    if (lines.size() < 3 || lines[0] != kReplayVersionLine || lines[1] != kRunsHeader)
        return std::nullopt;
    std::optional<RunRecord> run = parse_run_line(lines[2]);
    if (!run) return std::nullopt;
    Replay replay{.run = std::move(*run), .steps = {}};
    replay.steps.reserve(lines.size() - 3);
    for (std::size_t i = 3; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        std::optional<ReplayStep> step = parse_step(lines[i]);
        if (!step) return std::nullopt;
        replay.steps.push_back(*step);
    }
    return replay;
}

ReplayRecorder::ReplayRecorder(const MeasureOptions& options, std::uint64_t seed, Nanoseconds start)
    : replay_(std::make_shared<Replay>()), start_(start) {
    replay_->run.summary.options = normalized(options);
    replay_->run.summary.seed = seed;
}

std::int64_t ReplayRecorder::offset(Nanoseconds t) const {
    // Разность по модулю 2^64: щелчок до начала — меньше нуля.
    return static_cast<std::int64_t>(t - start_);
}

void ReplayRecorder::click(MouseButton button, Vec2 point, Nanoseconds t, Nanoseconds frame) {
    replay_->steps.push_back({.kind = ReplayStepKind::Click,
                              .at = offset(frame),
                              .time = offset(t),
                              .point = point,
                              .button = button});
}

void ReplayRecorder::update(Nanoseconds now, Vec2 cursor) {
    replay_->steps.push_back(
        {.kind = ReplayStepKind::Update, .at = offset(now), .time = offset(now), .point = cursor});
}

void ReplayRecorder::shown(Nanoseconds frame_time, Nanoseconds presented_at) {
    replay_->steps.push_back({.kind = ReplayStepKind::Shown,
                              .at = offset(frame_time),
                              .time = offset(presented_at),
                              .point = {}});
}

void ReplayRecorder::finish(EndReason why, Nanoseconds t, Nanoseconds frame) {
    replay_->steps.push_back({.kind = ReplayStepKind::Finish,
                              .at = offset(frame),
                              .time = offset(t),
                              .point = {},
                              .button = MouseButton::Left,
                              .reason = why});
}

std::shared_ptr<const Replay> ReplayRecorder::take(const RunRecord& run) {
    replay_->run = run;
    return std::exchange(replay_, std::make_shared<Replay>());
}

ReplayPlayer::ReplayPlayer(std::shared_ptr<const Replay> replay, const RandomFactory& make_random,
                           Nanoseconds start)
    : replay_(std::move(replay)),
      random_(make_random(replay_->run.summary.seed)),
      start_(start),
      game_(replay_->run.summary.options, *random_, start),
      frame_time_(start) {}

Nanoseconds ReplayPlayer::clock(std::int64_t offset) const {
    // Сдвиг по модулю 2^64: отрицательный — раньше начала.
    return start_ + static_cast<Nanoseconds>(offset);
}

void ReplayPlayer::advance(Nanoseconds now) {
    const std::vector<ReplayStep>& steps = replay_->steps;
    // Кадр шага — не раньше начала: запись так не делает, а испорченный файл
    // с отрицательным кадром иначе остановил бы проигрывание.
    while (next_ < steps.size() && clock(std::max<std::int64_t>(steps[next_].at, 0)) <= now) {
        const ReplayStep& s = steps[next_++];
        switch (s.kind) {
            case ReplayStepKind::Click: game_.click(s.button, s.point, clock(s.time)); break;
            case ReplayStepKind::Update:
                game_.update(clock(s.at), s.point);
                cursor_ = s.point;
                frame_time_ = clock(s.at);
                break;
            case ReplayStepKind::Shown: game_.frame_presented(clock(s.at), clock(s.time)); break;
            case ReplayStepKind::Finish: game_.finish(s.reason, clock(s.time)); break;
        }
    }
}

}  // namespace aim::core
