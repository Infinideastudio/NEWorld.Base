#include "Conc/Executor.h"
#include "Coro/Coro.h"
#include "Conc/BlockingAsContext.h"
#include "IO/Block.h"

std::atomic_int counter{ 0 };

ValueAsync<void> Load(int count) {
    volatile int ans = 0;
    const auto time = count % 10000;
    for (int i = 0; i < time; i++) ans += 42;
    (void)ans;
    co_return;
}

ValueAsync<void> Work(IExecutor* next, int count = rand()) {
    co_await SwitchTo(next);
    co_await Load(count);
    const auto result = counter.fetch_add(1);
    co_return;
}

ValueAsync<void> FileIO() {
    auto file = co_await IO::OpenBlock("/usr/bin/prime-run", IO::Block::F_READ);
    char buffer[1001];
    auto result = co_await file->Read(reinterpret_cast<uintptr_t>(buffer), 1000, 0);
    if (result.success()) {
        buffer[result.result()] = 0;
        puts(buffer);
    }
    else {
        printf("%d\n", result.error());
    }
    co_await file->Close();
}


/*int main() {
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
}*/

int main() {
    BlockingAsContext asCtx{};
    asCtx.Await(FileIO());
}