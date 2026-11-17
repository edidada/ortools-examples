#pragma once

#include <cstdint>
#include <random>
#include <vector>

// 最小化的强化学习调度环境（流水车间派工，flow shop dispatching）。
//
// 设计取舍：CP-SAT 是精确求解器，每个 step 都完整求解太慢。
// 因此只在 reset()（episode 开始）用 CP-SAT 求一次最优 makespan 作为基准，
// RL 的 action 是"选择下一个要派工的 job"，step() 内部只做快速仿真，
// 不再调用求解器。reward 反映相对最优调度的遗憾（regret）。
//
// 状态空间（观测向量，长度 = num_machines + num_jobs + 1，均归一化到约 [0,1]）：
//   [0, M)        每台机器当前空闲时刻 / 基准 makespan
//   [M, M+J)      每个 job 已派工工序数 / 总工序数 M
//   [M+J]         已派工工序数 / 总工序数 J*M
struct StepResult {
    std::vector<double> observation;
    double reward;
    bool done;
};

class SchedulingEnv {
public:
    SchedulingEnv(int num_jobs, int num_machines, uint32_t seed = 42);

    // 生成新实例、重置状态、用 CP-SAT 求基准，返回初始观测。
    std::vector<double> reset();

    // action 为 job 编号：派工该 job 的下一道工序。
    // 非法 action（越界 / 该 job 已派完）不改变状态，返回负奖励。
    StepResult step(int action);

    bool is_done() const;

    int num_jobs() const { return num_jobs_; }
    int num_machines() const { return num_machines_; }
    int action_size() const { return num_jobs_; }
    int total_operations() const { return num_jobs_ * num_machines_; }
    double baseline_makespan() const { return baseline_makespan_; }

    // 当前状态下合法的 action（仍有未派工工序的 job）。
    std::vector<int> valid_actions() const;

    // 当前仿真调度得到的 makespan（各机器空闲时刻的最大值）。
    double current_makespan() const;

private:
    std::vector<double> observation() const;
    // 用 CP-SAT 求最优（或超时时可行）makespan。
    int64_t solve_baseline() const;

    int num_jobs_;
    int num_machines_;
    uint32_t seed_;
    uint32_t episode_ = 0;
    std::mt19937 rng_;

    std::vector<std::vector<int64_t>> durations_;  // [job][machine]
    std::vector<int> next_op_;                    // 每个 job 的下一道工序编号
    std::vector<int64_t> machine_free_;           // 每台机器变为空闲的时刻
    std::vector<int64_t> job_free_;               // 每个 job 上一道工序完工时刻
    int scheduled_count_ = 0;
    double baseline_makespan_ = 1.0;
};
