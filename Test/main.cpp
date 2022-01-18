#include "Conc/Executor.h"
#include "Coro/Async.h"

std::atomic_int counter{ 0 };

ValueAsync<void> Work(IExecutor* next, int count = rand()) {
    co_await SwitchTo(next);
    volatile int ans = 0;
    const auto time = rand() % 10000;
    for (int i = 0; i < time; i++) ans += 42;
    (void)ans;
    const auto result = counter.fetch_add(1);
    co_return;
}

int main() {
    {
        auto exec = CreateScalingBagExecutor(1, 6, 1000);
        puts("start enqueue");
        for (int i = 0; i < 5000000; ++i) {
            srand(42);
            Work(exec.get());
        }
        puts("done enqueue");
    }
    printf("%d\n", counter.load());
}