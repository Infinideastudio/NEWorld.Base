#include "Conc/Executor.h"
#include "Coro/Coro.h"
#include "Conc/BlockingAsContext.h"
#include "IO/Block.h"
#include "IO/Stream.h"

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

/*ValueAsync<void> FileIO() {
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
}*/

ValueAsync<void> ServerOnceEcho() {
    auto accept = IO::CreateAcceptor(IO::Address::CreateIPv4("0.0.0.0").value(), 30080, 128);
    auto&& [stat, address, stream] = co_await accept->Once();
    char buffer[1000];
    auto resultA = co_await stream->Read(reinterpret_cast<uintptr_t>(buffer), 1000);
    co_await stream->Write(reinterpret_cast<uintptr_t>(buffer), resultA.result());
    co_await stream->Close();
    accept->Close();
}

ValueAsync<void> ClientOnce() {
    auto conn = co_await IO::Connect(IO::Address::CreateIPv4("127.0.0.1").value(), 30080);
    const auto data = "Hello World\n";
    char buffer[1000];
    co_await conn->Write(reinterpret_cast<uintptr_t>(data), 13);
    auto resultA = co_await conn->Read(reinterpret_cast<uintptr_t>(buffer), 1000);
    puts(buffer);
    co_await conn->Close();
}

ValueAsync<void> Network() {
    co_await Await(ServerOnceEcho(), ClientOnce());
}

int main() {
    BlockingAsContext asCtx{};
    asCtx.Await(Network());
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

/*int main() {
    BlockingAsContext asCtx{};
    asCtx.Await(FileIO());
}*/