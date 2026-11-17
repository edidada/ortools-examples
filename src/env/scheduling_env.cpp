#include "env/scheduling_env.h"

#include <algorithm>
#include <cmath>

#include "ortools/sat/cp_model.h"

namespace sat = operations_research::sat;

namespace {
constexpr int64_t kMinDuration = 1;
constexpr int64_t kMaxDuration = 9;
constexpr double kTimeLimitSeconds = 5.0;
constexpr double kStepCost = 0.01;
}  // namespace

SchedulingEnv::SchedulingEnv(int num_jobs, int num_machines, uint32_t seed)
    : num_jobs_(num_jobs),
      num_machines_(num_machines),
      seed_(seed),
      rng_(seed),
      durations_(num_jobs, std::vector<int64_t>(num_machines, kMinDuration)),
      next_op_(num_jobs, 0),
      machine_free_(num_machines, 0),
      job_free_(num_jobs, 0) {}

std::vector<double> SchedulingEnv::reset() {
    // 每个 episode 用不同但可复现的种子，保证实例有变化又能重放。
    rng_.seed(seed_ + episode_++);
    std::uniform_int_distribution<int64_t> dist(kMinDuration, kMaxDuration);
    for (int j = 0; j < num_jobs_; ++j) {
        for (int m = 0; m < num_machines_; ++m) {
            durations_[j][m] = dist(rng_);
        }
    }

    std::fill(next_op_.begin(), next_op_.end(), 0);
    std::fill(machine_free_.begin(), machine_free_.end(), 0);
    std::fill(job_free_.begin(), job_free_.end(), 0);
    scheduled_count_ = 0;

    baseline_makespan_ = static_cast<double>(solve_baseline());
    return observation();
}

int64_t SchedulingEnv::solve_baseline() const {
    int64_t horizon = 0;
    for (const auto& row : durations_) {
        for (int64_t d : row) horizon += d;
    }

    sat::CpModelBuilder model;
    std::vector<std::vector<sat::IntVar>> starts(num_jobs_);
    std::vector<std::vector<sat::IntVar>> ends(num_jobs_);
    std::vector<std::vector<sat::IntervalVar>> intervals(num_jobs_);

    // 工序 (j, m)：固定工时，起点/终点为整数变量。
    for (int j = 0; j < num_jobs_; ++j) {
        starts[j].reserve(num_machines_);
        ends[j].reserve(num_machines_);
        intervals[j].reserve(num_machines_);
        for (int m = 0; m < num_machines_; ++m) {
            const sat::IntVar start = model.NewIntVar({0, horizon});
            const sat::IntVar end = model.NewIntVar({0, horizon});
            const sat::IntervalVar task =
                model.NewIntervalVar(start, durations_[j][m], end);
            starts[j].push_back(start);
            ends[j].push_back(end);
            intervals[j].push_back(task);
        }
    }

    // 同一台机器上所有工序互不重叠。
    for (int m = 0; m < num_machines_; ++m) {
        std::vector<sat::IntervalVar> on_machine;
        on_machine.reserve(num_jobs_);
        for (int j = 0; j < num_jobs_; ++j) {
            on_machine.push_back(intervals[j][m]);
        }
        model.AddNoOverlap(on_machine);
    }

    // 工序顺序约束：job 的第 m 道工序必须先于第 m+1 道。
    for (int j = 0; j < num_jobs_; ++j) {
        for (int m = 0; m + 1 < num_machines_; ++m) {
            model.AddLessOrEqual(ends[j][m], starts[j][m + 1]);
        }
    }

    // makespan = 所有 job 最后一道工序完工时刻的最大值。
    std::vector<sat::IntVar> last_ends;
    last_ends.reserve(num_jobs_);
    for (int j = 0; j < num_jobs_; ++j) {
        last_ends.push_back(ends[j][num_machines_ - 1]);
    }
    const sat::IntVar makespan = model.NewIntVar({0, horizon});
    model.AddMaxEquality(makespan, last_ends);
    model.Minimize(makespan);

    // 单 worker + 时间上限：确定性、可控耗时。
    sat::SatParameters params;
    params.set_max_time_in_seconds(kTimeLimitSeconds);
    params.set_num_search_workers(1);
    const sat::CpSolverResponse response =
        sat::SolveWithParameters(model.Build(), params);

    if (response.status() == sat::OPTIMAL ||
        response.status() == sat::FEASIBLE) {
        return static_cast<int64_t>(std::llround(response.objective_value()));
    }
    // 兜底：未找到可行解时，用工时总和作为保守上界。
    return horizon;
}

std::vector<double> SchedulingEnv::observation() const {
    std::vector<double> obs;
    obs.reserve(num_machines_ + num_jobs_ + 1);
    for (int m = 0; m < num_machines_; ++m) {
        obs.push_back(static_cast<double>(machine_free_[m]) /
                      baseline_makespan_);
    }
    for (int j = 0; j < num_jobs_; ++j) {
        obs.push_back(static_cast<double>(next_op_[j]) / num_machines_);
    }
    obs.push_back(static_cast<double>(scheduled_count_) / total_operations());
    return obs;
}

StepResult SchedulingEnv::step(int action) {
    const bool valid = action >= 0 && action < num_jobs_ &&
                       next_op_[action] < num_machines_;
    if (is_done() || !valid) {
        return StepResult{observation(), -kStepCost, is_done()};
    }

    const int m = next_op_[action];
    const int64_t start = std::max(machine_free_[m], job_free_[action]);
    const int64_t end = start + durations_[action][m];
    machine_free_[m] = end;
    job_free_[action] = end;
    ++next_op_[action];
    ++scheduled_count_;

    double reward = -kStepCost;
    const bool done = is_done();
    if (done) {
        // 终局奖励：相对 CP-SAT 基准的归一化遗憾（<= 0，越接近 0 越好）。
        reward += (baseline_makespan_ - current_makespan()) /
                  baseline_makespan_;
    }
    return StepResult{observation(), reward, done};
}

bool SchedulingEnv::is_done() const {
    return scheduled_count_ >= total_operations();
}

std::vector<int> SchedulingEnv::valid_actions() const {
    std::vector<int> actions;
    actions.reserve(num_jobs_);
    for (int j = 0; j < num_jobs_; ++j) {
        if (next_op_[j] < num_machines_) actions.push_back(j);
    }
    return actions;
}

double SchedulingEnv::current_makespan() const {
    return static_cast<double>(
        *std::max_element(machine_free_.begin(), machine_free_.end()));
}
