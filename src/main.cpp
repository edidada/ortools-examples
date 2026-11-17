#include <cstddef>
#include <iostream>
#include <random>
#include <vector>

#include "env/scheduling_env.h"

// 演示：一个最简单的 RL step 循环（随机策略），跑几个 episode 打印观测与奖励。
int main() {
    SchedulingEnv env(/*num_jobs=*/4, /*num_machines=*/3, /*seed=*/2026);
    std::mt19937 rng(7);
    double total_reward = 0.0;

    for (int episode = 0; episode < 3; ++episode) {
        const std::vector<double> obs = env.reset();
        std::cout << "episode " << episode
                  << " | obs_dim=" << obs.size()
                  << " | cp_sat_baseline=" << env.baseline_makespan()
                  << " | operations=" << env.total_operations() << "\n";

        double episode_reward = 0.0;
        int steps = 0;
        while (!env.is_done()) {
            const std::vector<int> actions = env.valid_actions();
            std::uniform_int_distribution<std::size_t> pick(
                0, actions.size() - 1);
            const StepResult result = env.step(actions[pick(rng)]);
            episode_reward += result.reward;
            ++steps;
        }

        std::cout << "  steps=" << steps
                  << " | makespan=" << env.current_makespan()
                  << " | episode_reward=" << episode_reward << "\n";
        total_reward += episode_reward;
    }

    std::cout << "total_reward=" << total_reward << "\n";
    return 0;
}
