#include <Container.hpp>

#include <catch_amalgamated.hpp>

#include <vector>

using container::CircularBuffer;

TEST_CASE("capacity is rounded up to a power of two", "[container]")
{
    CHECK(CircularBuffer<float>(0).capacity() == 1);
    CHECK(CircularBuffer<float>(1).capacity() == 1);
    CHECK(CircularBuffer<float>(5).capacity() == 8);
    CHECK(CircularBuffer<float>(8).capacity() == 8);
    CHECK(CircularBuffer<float>(9).capacity() == 16);
    CHECK(CircularBuffer<float>(1000).capacity() == 1024);
}

TEST_CASE("free and full accounting sums to capacity", "[container]")
{
    CircularBuffer<float> ring(16);
    REQUIRE(ring.capacity() == 16);
    CHECK(ring.availableRead() == 0);
    CHECK(ring.availableWrite() == 16);

    const std::vector<float> block(6, 1.0f);
    REQUIRE(ring.write(block.data(), block.size()) == 6);
    CHECK(ring.availableRead() == 6);
    CHECK(ring.availableWrite() == 10);

    std::vector<float> drained(6, 0.0f);
    REQUIRE(ring.read(drained.data(), drained.size()) == 6);
    CHECK(ring.availableRead() == 0);
    CHECK(ring.availableWrite() == 16);
}

TEST_CASE("a round trip preserves order", "[container]")
{
    CircularBuffer<float> ring(8);
    const std::vector<float> in{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    REQUIRE(ring.write(in.data(), in.size()) == in.size());

    std::vector<float> out(in.size(), 0.0f);
    REQUIRE(ring.read(out.data(), out.size()) == out.size());
    CHECK(out == in);
}

TEST_CASE("a full buffer takes nothing more and never grows", "[container]")
{
    CircularBuffer<float> ring(4);
    const std::vector<float> full{1.0f, 2.0f, 3.0f, 4.0f};
    REQUIRE(ring.write(full.data(), full.size()) == 4);
    CHECK(ring.capacity() == 4);

    const std::vector<float> extra{9.0f, 9.0f};
    CHECK(ring.write(extra.data(), extra.size()) == 0);
    CHECK(ring.availableWrite() == 0);

    std::vector<float> out(4, 0.0f);
    REQUIRE(ring.read(out.data(), out.size()) == 4);
    CHECK(out == full);
}

TEST_CASE("a write larger than the free space takes a prefix", "[container]")
{
    CircularBuffer<float> ring(8);
    std::vector<float> space(3, 0.0f);
    REQUIRE(ring.write(space.data(), space.size()) == 3);
    REQUIRE(ring.availableWrite() == 5);

    const std::vector<float> in{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    CHECK(ring.write(in.data(), in.size()) == 5);
    CHECK(ring.availableRead() == 8);

    std::vector<float> out(8, 0.0f);
    REQUIRE(ring.read(out.data(), out.size()) == 8);
    CHECK(out == std::vector<float>{0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
}

TEST_CASE("a read larger than the content delivers a prefix", "[container]")
{
    CircularBuffer<float> ring(8);
    const std::vector<float> in{1.0f, 2.0f, 3.0f};
    REQUIRE(ring.write(in.data(), in.size()) == in.size());

    std::vector<float> out(8, -1.0f);
    CHECK(ring.read(out.data(), out.size()) == 3);
    CHECK(out[0] == 1.0f);
    CHECK(out[1] == 2.0f);
    CHECK(out[2] == 3.0f);
    CHECK(out[3] == -1.0f);
}

TEST_CASE("data survives wrap-around with mismatched chunk sizes", "[container]")
{
    CircularBuffer<float> ring(16);
    const int total = 1000;
    const int chunkSizes[] = {5, 3, 17, 1, 8, 12, 2};
    constexpr int chunkCount = 7;

    int written = 0;
    int read = 0;
    int chunk = 0;
    std::vector<float> out(17, 0.0f);

    auto value = [](int index) { return static_cast<float>(index) * 0.25f; };

    while (read < total)
    {
        const int inChunk = chunkSizes[chunk % chunkCount];
        const int toWrite = std::min(inChunk, total - written);
        if (toWrite > 0 && ring.availableWrite() >= static_cast<std::size_t>(toWrite))
        {
            std::vector<float> batch(static_cast<std::size_t>(toWrite));
            for (int i = 0; i < toWrite; ++i)
                batch[static_cast<std::size_t>(i)] = value(written + i);
            REQUIRE(ring.write(batch.data(), batch.size()) == static_cast<std::size_t>(toWrite));
            written += toWrite;
        }

        const int toRead = std::min(inChunk, total - read);
        const std::size_t got = ring.read(out.data(), static_cast<std::size_t>(toRead));
        for (std::size_t i = 0; i < got; ++i)
            CHECK(out[i] == value(read + static_cast<int>(i)));
        read += static_cast<int>(got);
        ++chunk;
    }

    CHECK(written == total);
    CHECK(read == total);
}

TEST_CASE("monotonic cursors keep working after many wraps", "[container]")
{
    CircularBuffer<float> ring(4);
    std::vector<float> block{1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> out(4, 0.0f);

    for (int cycle = 0; cycle < 5000; ++cycle)
    {
        REQUIRE(ring.write(block.data(), block.size()) == 4);
        REQUIRE(ring.read(out.data(), out.size()) == 4);
        CHECK(out == block);
        CHECK(ring.availableRead() == 0);
        CHECK(ring.availableWrite() == 4);
        block[0] = static_cast<float>(cycle % 7);
    }
}

TEST_CASE("clear empties the buffer and keeps its capacity", "[container]")
{
    CircularBuffer<float> ring(8);
    const std::vector<float> in{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    REQUIRE(ring.write(in.data(), in.size()) == in.size());

    ring.clear();
    CHECK(ring.availableRead() == 0);
    CHECK(ring.availableWrite() == 8);
    CHECK(ring.capacity() == 8);

    std::vector<float> out(1, 0.0f);
    CHECK(ring.read(out.data(), out.size()) == 0);

    const std::vector<float> refill{7.0f, 8.0f};
    CHECK(ring.write(refill.data(), refill.size()) == 2);
    REQUIRE(ring.read(out.data(), out.size()) == 1);
    CHECK(out[0] == 7.0f);
}
