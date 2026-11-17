#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>

#include "env/scheduling_env.h"

// 不依赖 gtest 的最小测试：验证 reset / step / done 的基本行为。
int main() {
    SchedulingEnv env(/*num_jobs=*/3, /*num_machines=*/2, /*seed=*/1);

    const std::vector<double> obs0 = env.reset();
    const std::size_t expected_dim =
        static_cast<std::size_t>(env.num_machines() + env.num_jobs() + 1);
    assert(obs0.size() == expected_dim);
    assert(!env.is_done());
    assert(env.baseline_makespan() >= 1.0);
    assert(env.current_makespan() == 0.0);
    assert(env.valid_actions().size() ==
           static_cast<std::size_t>(env.num_jobs()));

    // 非法 action 不改变状态。
    const StepResult bad = env.step(999);
    assert(bad.reward < 0.0);
    assert(!bad.done);
    assert(env.current_makespan() == 0.0);
    assert(env.valid_actions().size() ==
           static_cast<std::size_t>(env.num_jobs()));

    // 一直派工直到 episode 结束。
    int steps = 0;
    while (!env.is_done()) {
        const std::vector<int> actions = env.valid_actions();
        assert(!actions.empty());
        const StepResult result = env.step(actions.front());
        ++steps;
        assert(result.observation.size() == expected_dim);
        assert(std::isfinite(result.reward));
        if (result.done) assert(env.is_done());
    }
    assert(steps == env.total_operations());
    assert(env.current_makespan() > 0.0);
    assert(env.valid_actions().empty());

    // reset 之后可以重新跑一个 episode。
    const std::vector<double> obs1 = env.reset();
    assert(obs1.size() == expected_dim);
    assert(!env.is_done());
    assert(env.current_makespan() == 0.0);

    std::cout << "test_env: all checks passed\n";
    return 0;
}
