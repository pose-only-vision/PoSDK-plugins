#include "method_superpoint_lightglue.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

using namespace PluginMethods::SuperPointLightGlueDetail;

class TemporaryResult
{
public:
    TemporaryResult()
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
                     ("posdk-superpoint-lightglue-test-" + std::to_string(::getpid()) +
                      "-" + std::to_string(nonce));
        std::filesystem::create_directories(directory_);
        path_ = directory_ / "result.bin";
    }

    ~TemporaryResult()
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    const std::filesystem::path &path() const { return path_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path path_;
};

void WriteU32(std::ostream &stream, std::uint32_t value)
{
    const std::array<unsigned char, 4> bytes = {
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8U) & 0xffU),
        static_cast<unsigned char>((value >> 16U) & 0xffU),
        static_cast<unsigned char>((value >> 24U) & 0xffU)};
    stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

void WriteU64(std::ostream &stream, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
    {
        stream.put(static_cast<char>((value >> shift) & 0xffU));
    }
}

void WriteFloat(std::ostream &stream, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(stream, bits);
}

void WriteString(std::ostream &stream, const std::string &value)
{
    WriteU32(stream, static_cast<std::uint32_t>(value.size()));
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
}

void WriteIdentity(std::ostream &stream, const std::string &extractor = kExtractorName,
                   const std::string &requested_device = "auto",
                   const std::string &device = "cpu")
{
    const std::string matcher_device = device == "cpu+mps" ? "mps" : "cpu";
    const std::vector<std::string> values = {
        extractor,
        kMatcherName,
        requested_device,
        device,
        "cpu",
        matcher_device,
        kPythonVersion,
        kTorchVersion,
        kTorchvisionVersion,
        kKorniaVersion,
        kImageDecoderIdentity,
        kLightGlueCommit,
        kLightGlueArchiveSha256,
        kLightGlueTreeSha256,
        kSuperPointCommit,
        kSuperPointModelSha256,
        kLightGlueModelSha256};
    for (const std::string &value : values)
    {
        WriteString(stream, value);
    }
}

void WriteHeader(std::ostream &stream, std::uint32_t images,
                 std::uint32_t pairs, const std::string &extractor = kExtractorName,
                 const std::string &requested_device = "auto",
                 const std::string &device = "cpu")
{
    stream.write("PSLGRES1", 8);
    WriteU32(stream, kProtocolVersion);
    WriteU32(stream, images);
    WriteU32(stream, pairs);
    WriteU64(stream, 10);
    WriteU64(stream, 20);
    WriteU64(stream, 30);
    WriteU64(stream, 60);
    WriteU64(stream, 4096);
    WriteIdentity(stream, extractor, requested_device, device);
}

void WriteTwoImagePayload(std::ostream &stream, std::uint32_t feature_j = 1)
{
    WriteU32(stream, 2);
    WriteFloat(stream, 10.0F);
    WriteFloat(stream, 20.0F);
    WriteFloat(stream, 0.9F);
    WriteFloat(stream, 30.0F);
    WriteFloat(stream, 40.0F);
    WriteFloat(stream, 0.8F);

    WriteU32(stream, 2);
    WriteFloat(stream, 11.0F);
    WriteFloat(stream, 21.0F);
    WriteFloat(stream, 0.7F);
    WriteFloat(stream, 31.0F);
    WriteFloat(stream, 41.0F);
    WriteFloat(stream, 0.6F);

    WriteU32(stream, 0);
    WriteU32(stream, 1);
    WriteU32(stream, 2);
    WriteU32(stream, 0);
    WriteU32(stream, 0);
    WriteFloat(stream, 0.95F);
    WriteU32(stream, 1);
    WriteU32(stream, feature_j);
    WriteFloat(stream, 0.85F);
}

TEST(SuperPointLightGlueProtocolTest, ParsesBoundedStableFeatureIndices)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1);
        WriteTwoImagePayload(stream);
    }

    WorkerResult result;
    std::string error;
    ASSERT_TRUE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error))
        << error;
    ASSERT_EQ(result.keypoints.size(), 2U);
    ASSERT_EQ(result.pairs.size(), 1U);
    EXPECT_EQ(result.pairs[0].matches.size(), 2U);
    EXPECT_EQ(result.pairs[0].matches[0].feature_i, 0U);
    EXPECT_EQ(result.pairs[0].matches[1].feature_i, 1U);
    EXPECT_EQ(result.pairs[0].matches[1].feature_j, 1U);
    EXPECT_EQ(result.lightglue_commit, kLightGlueCommit);
    EXPECT_EQ(result.requested_device, "auto");
    EXPECT_EQ(result.device, "cpu");
    EXPECT_EQ(result.extractor_device, "cpu");
    EXPECT_EQ(result.matcher_device, "cpu");
}

TEST(SuperPointLightGlueProtocolTest, AcceptsExplicitExperimentalMpsIdentity)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1, kExtractorName, "mps", "cpu+mps");
        WriteTwoImagePayload(stream);
    }

    WorkerResult result;
    std::string error;
    ASSERT_TRUE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error))
        << error;
    EXPECT_EQ(result.requested_device, "mps");
    EXPECT_EQ(result.device, "cpu+mps");
    EXPECT_EQ(result.extractor_device, "cpu");
    EXPECT_EQ(result.matcher_device, "mps");
}

TEST(SuperPointLightGlueProtocolTest, RejectsAutomaticMpsSelection)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1, kExtractorName, "auto", "cpu+mps");
        WriteTwoImagePayload(stream);
    }

    WorkerResult result;
    std::string error;
    EXPECT_FALSE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error));
    EXPECT_NE(error.find("identity"), std::string::npos);
}

TEST(SuperPointLightGlueProtocolTest, RejectsOutOfRangeMatchIndex)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1);
        WriteTwoImagePayload(stream, 2);
    }

    WorkerResult result;
    std::string error;
    EXPECT_FALSE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error));
    EXPECT_NE(error.find("invalid feature match"), std::string::npos);
}

TEST(SuperPointLightGlueProtocolTest, RejectsRuntimeIdentityMismatch)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1, "sift");
        WriteTwoImagePayload(stream);
    }

    WorkerResult result;
    std::string error;
    EXPECT_FALSE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error));
    EXPECT_NE(error.find("identity"), std::string::npos);
}

TEST(SuperPointLightGlueProtocolTest, RejectsTrailingBytes)
{
    TemporaryResult temporary;
    {
        std::ofstream stream(temporary.path(), std::ios::binary);
        ASSERT_TRUE(stream.good());
        WriteHeader(stream, 2, 1);
        WriteTwoImagePayload(stream);
        stream.put('x');
    }

    WorkerResult result;
    std::string error;
    EXPECT_FALSE(ParseWorkerResult(temporary.path(), 2, 2, 1, &result, &error));
    EXPECT_NE(error.find("trailing"), std::string::npos);
}

} // namespace
