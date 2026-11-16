// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/cli_options.hpp"
#include "core/github_release.hpp"
#include "core/install_info.hpp"
#include "core/json.hpp"
#include "core/sha256.hpp"
#include "core/update_plan.hpp"
#include "core/version.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using updater::core::InstalledHitokoto;
using updater::core::InstallScope;
using updater::core::MsiInstallOptions;
using updater::core::UpdateStatus;
using updater::core::Version;

constexpr const char* kSampleReleaseJson = R"json({
  "tag_name": "v0.6.2",
  "name": "hitokoto 0.6.2",
  "draft": false,
  "prerelease": false,
  "assets": [
    {
      "name": "hitokoto-0.6.2-x64.msi",
      "digest": "sha256:b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0",
      "browser_download_url": "https://github.com/Daniel-Fuerst/hitokoto/releases/download/v0.6.2/hitokoto-0.6.2-x64.msi"
    },
    {
      "name": "hitokoto_0.6.2-1_amd64.deb",
      "digest": "sha256:788ddfdabbdaae242c1499c85610b58b350d7d7d7c9001a4e695d16cc85bd3ce",
      "browser_download_url": "https://github.com/Daniel-Fuerst/hitokoto/releases/download/v0.6.2/hitokoto_0.6.2-1_amd64.deb"
    },
    {
      "name": "SHA256SUMS",
      "digest": "sha256:755d8859fd5ba34c805f586f683ead99d89fd627ec1ab188be09bdfe7b4ab05e",
      "browser_download_url": "https://github.com/Daniel-Fuerst/hitokoto/releases/download/v0.6.2/SHA256SUMS"
    }
  ]
})json";

TEST(S001_Version, ParsesValidVersionsWithAndWithoutVPrefix) {
    auto v1 = updater::core::parse_version("0.6.2");
    ASSERT_TRUE(v1.has_value());
    if (!v1.has_value()) {
        return;
    }
    EXPECT_EQ(*v1, (Version{0, 6, 2}));
    EXPECT_EQ(updater::core::to_string(*v1), "0.6.2");

    auto v2 = updater::core::parse_version("v1.12.3");
    ASSERT_TRUE(v2.has_value());
    if (!v2.has_value()) {
        return;
    }
    EXPECT_EQ(*v2, (Version{1, 12, 3}));

    auto v3 = updater::core::parse_version("V0.0.0");
    ASSERT_TRUE(v3.has_value());
    if (!v3.has_value()) {
        return;
    }
    EXPECT_EQ(*v3, (Version{0, 0, 0}));
}

TEST(S001_Version, RejectsInvalidVersions) {
    EXPECT_FALSE(updater::core::parse_version("").has_value());
    EXPECT_FALSE(updater::core::parse_version("0.6").has_value());
    EXPECT_FALSE(updater::core::parse_version("01.2.3").has_value());
    EXPECT_FALSE(updater::core::parse_version("0.6.2-rc1").has_value());
    EXPECT_FALSE(updater::core::parse_version("0.6.2+dirty").has_value());
    EXPECT_FALSE(updater::core::parse_version("v0.6.2 ").has_value());
}

TEST(S001_Version, ComparesVersionsNumerically) {
    EXPECT_LT((Version{0, 6, 2}), (Version{0, 6, 10}));
    EXPECT_LT((Version{0, 6, 2}), (Version{0, 7, 0}));
    EXPECT_LT((Version{0, 9, 9}), (Version{1, 0, 0}));
    EXPECT_EQ((Version{0, 6, 2}), (Version{0, 6, 2}));
}

TEST(S001_Version, ParsesHitokotoCliVersionOutput) {
    auto v = updater::core::parse_hitokoto_cli_version("hitokoto 0.6.2\r\n");
    ASSERT_TRUE(v.has_value());
    if (!v.has_value()) {
        return;
    }
    EXPECT_EQ(*v, (Version{0, 6, 2}));

    EXPECT_FALSE(updater::core::parse_hitokoto_cli_version("other 0.6.2\n").has_value());
    EXPECT_FALSE(updater::core::parse_hitokoto_cli_version("").has_value());
}

TEST(S001_Json, ParsesObjectsArraysStringsEscapesAndBools) {
    auto doc = updater::core::json::parse_json(R"({"key":"a\"b\\c\n\u0041","ok":true,"arr":[1,false,null]})");
    ASSERT_TRUE(doc.has_value());
    if (!doc.has_value()) {
        return;
    }
    const auto* str = doc->find_string("key");
    ASSERT_NE(str, nullptr);
    EXPECT_EQ(*str, "a\"b\\c\nA");
    EXPECT_EQ(doc->find_bool("ok"), std::optional<bool>{true});
    const auto* arr = doc->find_array("arr");
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->size(), 3U);
}

TEST(S001_Json, RejectsMalformedJson) {
    EXPECT_FALSE(updater::core::json::parse_json("").has_value());
    EXPECT_FALSE(updater::core::json::parse_json("{unquoted: 1}").has_value());
    EXPECT_FALSE(updater::core::json::parse_json("[1, 2,]").has_value());
    EXPECT_FALSE(updater::core::json::parse_json(R"({"a":1} trailing)").has_value());
}

TEST(S001_GitHubRelease, ParsesLatestReleaseAndLocatesMsiAndChecksums) {
    auto parsed = updater::core::parse_github_release_json(kSampleReleaseJson);
    ASSERT_TRUE(parsed.release.has_value()) << parsed.error;
    if (!parsed.release.has_value()) {
        return;
    }
    EXPECT_EQ(parsed.release->version, (Version{0, 6, 2}));
    EXPECT_EQ(parsed.release->tag_name, "v0.6.2");
    EXPECT_FALSE(parsed.release->prerelease);
    EXPECT_FALSE(parsed.release->draft);

    auto msi = updater::core::find_msi_asset(*parsed.release);
    ASSERT_TRUE(msi.has_value());
    if (!msi.has_value()) {
        return;
    }
    EXPECT_EQ(msi->name, "hitokoto-0.6.2-x64.msi");
    EXPECT_EQ(msi->download_url,
              "https://github.com/Daniel-Fuerst/hitokoto/releases/download/v0.6.2/hitokoto-0.6.2-x64.msi");
    EXPECT_EQ(msi->sha256_digest,
              std::optional<std::string>{"b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0"});

    auto sums = updater::core::find_sha256sums_asset(*parsed.release);
    ASSERT_TRUE(sums.has_value());
    if (!sums.has_value()) {
        return;
    }
    EXPECT_EQ(sums->name, "SHA256SUMS");
}

TEST(S001_GitHubRelease, RejectsNonVersionTag) {
    auto parsed = updater::core::parse_github_release_json(R"({"tag_name":"v0.3.0-tulip","assets":[]})");
    EXPECT_FALSE(parsed.release.has_value());
    EXPECT_FALSE(parsed.error.empty());
}

TEST(S001_Sha256, ComputesStandardNistVectors) {
    EXPECT_EQ(updater::core::sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(updater::core::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(updater::core::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(S001_Sha256, HashesFileOnDisk) {
    const auto temp_dir = std::filesystem::temp_directory_path() / "hitokoto_win_updater_test_s001";
    std::filesystem::create_directories(temp_dir);
    const auto file_path = temp_dir / "sample.bin";
    {
        std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
        out << "abc";
    }
    auto digest = updater::core::sha256_file_hex(file_path);
    std::filesystem::remove_all(temp_dir);

    ASSERT_TRUE(digest.has_value());
    if (!digest.has_value()) {
        return;
    }
    EXPECT_EQ(*digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(S001_Sha256, ParsesSha256SumsFile) {
    const std::string sums =
        "788ddfdabbdaae242c1499c85610b58b350d7d7d7c9001a4e695d16cc85bd3ce  hitokoto_0.6.2-1_amd64.deb\r\n"
        "B6C15AFF693114ECEAD777D9FF8CA911CD337961C72971D4EDDB090BF4E216A0  hitokoto-0.6.2-x64.msi\r\n";
    auto hash = updater::core::parse_sha256sums(sums, "hitokoto-0.6.2-x64.msi");
    ASSERT_TRUE(hash.has_value());
    if (!hash.has_value()) {
        return;
    }
    EXPECT_EQ(*hash, "b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0");
    EXPECT_FALSE(updater::core::parse_sha256sums(sums, "missing.msi").has_value());
}

TEST(S001_UpdatePlan, DetectsUpToDateUpdateAvailableAndNotInstalled) {
    auto parsed = updater::core::parse_github_release_json(kSampleReleaseJson);
    ASSERT_TRUE(parsed.release.has_value());
    if (!parsed.release.has_value()) {
        return;
    }

    InstalledHitokoto current_same{Version{0, 6, 2}, InstallScope::User, ""};
    auto up_to_date = updater::core::plan_update(current_same, *parsed.release);
    EXPECT_EQ(up_to_date.status, UpdateStatus::UpToDate);

    InstalledHitokoto current_older{Version{0, 6, 1}, InstallScope::Machine, ""};
    auto available = updater::core::plan_update(current_older, *parsed.release);
    EXPECT_EQ(available.status, UpdateStatus::UpdateAvailable);
    ASSERT_TRUE(available.msi_asset.has_value());
    if (!available.msi_asset.has_value()) {
        return;
    }
    EXPECT_EQ(available.msi_asset->name, "hitokoto-0.6.2-x64.msi");

    auto not_installed = updater::core::plan_update(std::nullopt, *parsed.release);
    EXPECT_EQ(not_installed.status, UpdateStatus::NotInstalled);
    ASSERT_TRUE(not_installed.msi_asset.has_value());
}

TEST(S001_UpdatePlan, VerifiesChecksumsStrictly) {
    const std::string sums =
        "b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0  hitokoto-0.6.2-x64.msi\n";
    std::string err;

    EXPECT_TRUE(updater::core::verify_expected_sha256(
        "b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0", sums, "hitokoto-0.6.2-x64.msi",
        std::string{"b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0"}, err));

    EXPECT_FALSE(
        updater::core::verify_expected_sha256("0000000000000000000000000000000000000000000000000000000000000000", sums,
                                              "hitokoto-0.6.2-x64.msi", std::nullopt, err));
    EXPECT_FALSE(err.empty());

    EXPECT_FALSE(
        updater::core::verify_expected_sha256("b6c15aff693114ecead777d9ff8ca911cd337961c72971d4eddb090bf4e216a0",
                                              std::nullopt, "hitokoto-0.6.2-x64.msi", std::nullopt, err));
}

TEST(S001_InstallInfo, BuildsMsiexecArgumentsPreservingScope) {
    MsiInstallOptions user_upgrade{
        .silent = false, .accept_license = false, .is_upgrade = true, .scope = InstallScope::User};
    auto user_args = updater::core::build_msiexec_arguments(R"(C:\Temp\hitokoto-0.6.2-x64.msi)", user_upgrade);
    EXPECT_EQ(user_args, (std::vector<std::string>{
                             "/i",
                             R"(C:\Temp\hitokoto-0.6.2-x64.msi)",
                             "MSIINSTALLPERUSER=1",
                             "/qb",
                             "/norestart",
                         }));

    MsiInstallOptions machine_silent{
        .silent = true, .accept_license = true, .is_upgrade = false, .scope = InstallScope::Machine};
    auto machine_args = updater::core::build_msiexec_arguments(R"(C:\Temp\hitokoto-0.6.2-x64.msi)", machine_silent);
    EXPECT_EQ(machine_args, (std::vector<std::string>{
                                "/i",
                                R"(C:\Temp\hitokoto-0.6.2-x64.msi)",
                                "ALLUSERS=1",
                                "ACCEPT_LICENSE=yes",
                                "/qn",
                                "/norestart",
                            }));
}

TEST(S001_InstallInfo, SelectsPreferredInstallFromMultipleCandidates) {
    std::vector<InstalledHitokoto> candidates{
        {Version{0, 6, 0}, InstallScope::User, R"(C:\Users\a\AppData\Local\Programs\hitokoto)"},
        {Version{0, 6, 2}, InstallScope::Machine, R"(C:\Program Files\hitokoto)"},
    };
    auto chosen = updater::core::select_preferred_install(candidates);
    ASSERT_TRUE(chosen.has_value());
    if (!chosen.has_value()) {
        return;
    }
    EXPECT_EQ(chosen->version, (Version{0, 6, 2}));
    EXPECT_EQ(chosen->scope, InstallScope::Machine);
}

TEST(S001_CliOptions, ParsesKnownFlagsAndRejectsUnknownFlags) {
    auto ok = updater::core::parse_cli_args({"--check", "--silent", "--yes"});
    ASSERT_TRUE(ok.options.has_value()) << ok.error;
    if (!ok.options.has_value()) {
        return;
    }
    EXPECT_TRUE(ok.options->check_only);
    EXPECT_TRUE(ok.options->silent);
    EXPECT_TRUE(ok.options->yes);

    auto bad = updater::core::parse_cli_args({"--bogus"});
    EXPECT_FALSE(bad.options.has_value());
    EXPECT_FALSE(bad.error.empty());
}

}  // namespace
