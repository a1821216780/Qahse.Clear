#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../../src/IO/BinaryFileHandler.h"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_BinaryFileTests")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~ScopedTempRoot()
    {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    std::filesystem::path path(const std::string &relative) const
    {
        return root_ / relative;
    }

private:
    std::filesystem::path root_;
};
}

TEST(BinaryFileTest, ReadWritePrimitiveAndStringRoundTrip)
{
    const ScopedTempRoot root;
    const auto file = root.path("basic.bin").string();

    {
        BinaryFile writer(file, "write");
        writer.WriteData<int>(42);
        writer.WriteData<double>(3.14159);
        writer.WriteData<bool>(true);
        writer.WriteData<std::string>("hello");
        writer.Close();
    }

    {
        BinaryFile reader(file, "read");
        EXPECT_EQ(reader.ReadData<int>(), 42);
        EXPECT_NEAR(reader.ReadData<double>(), 3.14159, 1e-12);
        EXPECT_TRUE(reader.ReadData<bool>());
        EXPECT_EQ(reader.ReadData<std::string>(), "hello");
        EXPECT_TRUE(reader.Read2last());
        reader.Close();
    }
}

TEST(BinaryFileTest, VectorCharRoundTripUsesConsistentProtocol)
{
    const ScopedTempRoot root;
    const auto file = root.path("bytes.bin").string();

    const std::vector<char> payload = {
        static_cast<char>(1),
        static_cast<char>(0),
        static_cast<char>(0),
        static_cast<char>(0),
        'A'
    };

    {
        BinaryFile writer(file, "write");
        writer.WriteData<std::vector<char>>(payload);
        writer.Close();
    }

    {
        BinaryFile reader(file, "read");
        const auto readBack = reader.ReadData<std::vector<char>>();
        EXPECT_EQ(readBack, payload);
        reader.Close();
    }
}
