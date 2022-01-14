#include "Conc/Executor.h"

int main() {
    std::atomic_int counter{0};
    {
        auto exec = CreateScalingBagExecutor(1, 6, 0);
        puts("start enqueue");
        for (int i = 0; i < 5000000; ++i) {
            exec->Enqueue([&counter]() {
                /*volatile int ans;
                for (int i = 0; i < 100000; i++) ans = 42;
                (void)ans;*/
                const auto result = counter.fetch_add(1);
                if (result % 100000 == 99999) printf("%d\n", result + 1);
            });
        }
        puts("done enqueue");
    }
    printf("%d\n", counter.load());
}