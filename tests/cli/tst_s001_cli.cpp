// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli/updater_runner.hpp"
#include "core/sha256.hpp"

#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using updater::core::CliOptions;
using updater::core::InstalledHitokoto;
using updater::core::InstallScope;
using updater::core::MsiInstallOptions;
using updater::core::Version;
using updater::platform::HttpDownloadResult;
using updater::platform::HttpTextResult;
using updater::platform::IGitHubClient;
using updater::platform::IInstalledLocator;
using updater::platform::IMsiInstaller;
using updater::platform::InstallResult;

class FakeLocator final : public IInstalledLocator {
public:
    std::optional<InstalledHitokoto> installed;

    std::optional<InstalledHitokoto> find_installed() const override { return installed; }
};

class FakeGitHubClient final : public IGitHubClient {
public:
    std::string release_json;
    std::string sums_text;
    std::string msi_payload = "fake-msi-binary-content";
    int download_calls = 0;

    HttpTextResult fetch_latest_release_json(const std::string& /*repo*/, const std::string& /*user_agent*/) override {
        return {.ok = true, .status_code = 200, .body = release_json, .error = {}};
    }

    HttpTextResult download_text(const std::string& /*url*/, const std::string& /*user_agent*/) override {
        return {.ok = true, .status_code = 200, .body = sums_text, .error = {}};
    }

    HttpDownloadResult download_file(const std::string& /*url*/, const std::filesystem::path& destination,
                                     const std::string& /*user_agent*/,
                                     const std::function<void(std::uint64_t, std::int64_t)>& /*progress_cb*/) override {
        ++download_calls;
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        out << msi_payload;
        out.close();
        return {.ok = true, .status_code = 200, .error = {}};
    }
};

class FakeMsiInstaller final : public IMsiInstaller {
public:
    mutable int install_calls = 0;
    mutable std::filesystem::path last_msi_path;
    mutable MsiInstallOptions last_options{};
    InstallResult result{.launched = true, .exit_code = 0, .error = {}};

    InstallResult install(const std::filesystem::path& msi_path, const MsiInstallOptions& options) const override {
        ++install_calls;
        last_msi_path = msi_path;
        last_options = options;
        return result;
    }
};

std::string make_release_json(const std::string& version, const std::string& sha256_hex) {
    return "{\"tag_name\":\"v" + version + "\",\"name\":\"hitokoto " + version +
           "\",\"draft\":false,\"prerelease\":false,\"assets\":["
           "{\"name\":\"hitokoto-" +
           version + "-x64.msi\",\"digest\":\"sha256:" + sha256_hex +
           "\",\"browser_download_url\":\"https://example.invalid/hitokoto-" + version +
           "-x64.msi\"},"
           "{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://example.invalid/SHA256SUMS\"}"
           "]}";
}

}  // namespace

class tst_s001_cli : public QObject {
    Q_OBJECT

private slots:
    void up_to_date_does_not_download_or_install() {
        FakeLocator locator;
        locator.installed = InstalledHitokoto{Version{0, 6, 2}, InstallScope::User, ""};

        FakeGitHubClient github;
        const std::string hash = updater::core::sha256_hex(github.msi_payload);
        github.release_json = make_release_json("0.6.2", hash);
        github.sums_text = hash + "  hitokoto-0.6.2-x64.msi\n";

        FakeMsiInstaller installer;
        std::ostringstream out;
        std::ostringstream err;
        updater::cli::RunnerDeps deps{
            .locator = locator,
            .github = github,
            .installer = installer,
            .out = out,
            .err = err,
            .confirm = [](const std::string&) { return false; },
            .app_version = "0.1.0",
            .temp_root = std::filesystem::temp_directory_path(),
        };

        const int code = updater::cli::run_cli(CliOptions{}, deps);
        QCOMPARE(code, 0);
        QCOMPARE(github.download_calls, 0);
        QCOMPARE(installer.install_calls, 0);
        QVERIFY(out.str().find("up to date") != std::string::npos);
    }

    void check_only_reports_update_without_downloading() {
        FakeLocator locator;
        locator.installed = InstalledHitokoto{Version{0, 6, 1}, InstallScope::Machine, ""};

        FakeGitHubClient github;
        const std::string hash = updater::core::sha256_hex(github.msi_payload);
        github.release_json = make_release_json("0.6.2", hash);
        github.sums_text = hash + "  hitokoto-0.6.2-x64.msi\n";

        FakeMsiInstaller installer;
        std::ostringstream out;
        std::ostringstream err;
        updater::cli::RunnerDeps deps{
            .locator = locator,
            .github = github,
            .installer = installer,
            .out = out,
            .err = err,
            .confirm = [](const std::string&) { return false; },
            .app_version = "0.1.0",
            .temp_root = std::filesystem::temp_directory_path(),
        };

        CliOptions opts;
        opts.check_only = true;
        const int code = updater::cli::run_cli(opts, deps);
        QCOMPARE(code, 0);
        QCOMPARE(github.download_calls, 0);
        QCOMPARE(installer.install_calls, 0);
        QVERIFY(out.str().find("0.6.1") != std::string::npos);
        QVERIFY(out.str().find("0.6.2") != std::string::npos);
    }

    void update_available_downloads_verifies_and_installs_preserving_scope() {
        FakeLocator locator;
        locator.installed = InstalledHitokoto{Version{0, 6, 1}, InstallScope::Machine, ""};

        FakeGitHubClient github;
        const std::string hash = updater::core::sha256_hex(github.msi_payload);
        github.release_json = make_release_json("0.6.2", hash);
        github.sums_text = hash + "  hitokoto-0.6.2-x64.msi\n";

        FakeMsiInstaller installer;
        std::ostringstream out;
        std::ostringstream err;
        updater::cli::RunnerDeps deps{
            .locator = locator,
            .github = github,
            .installer = installer,
            .out = out,
            .err = err,
            .confirm = [](const std::string&) { return true; },
            .app_version = "0.1.0",
            .temp_root = std::filesystem::temp_directory_path(),
        };

        const int code = updater::cli::run_cli(CliOptions{}, deps);
        QCOMPARE(code, 0);
        QCOMPARE(github.download_calls, 1);
        QCOMPARE(installer.install_calls, 1);
        QCOMPARE(installer.last_options.scope, InstallScope::Machine);
        QVERIFY(installer.last_options.is_upgrade);
        QVERIFY(!std::filesystem::exists(installer.last_msi_path));
    }

    void checksum_mismatch_aborts_install_and_cleans_up() {
        FakeLocator locator;
        locator.installed = InstalledHitokoto{Version{0, 6, 1}, InstallScope::User, ""};

        FakeGitHubClient github;
        const std::string bad_hash(64, 'a');
        github.release_json = make_release_json("0.6.2", bad_hash);
        github.sums_text = bad_hash + "  hitokoto-0.6.2-x64.msi\n";

        FakeMsiInstaller installer;
        std::ostringstream out;
        std::ostringstream err;
        updater::cli::RunnerDeps deps{
            .locator = locator,
            .github = github,
            .installer = installer,
            .out = out,
            .err = err,
            .confirm = [](const std::string&) { return true; },
            .app_version = "0.1.0",
            .temp_root = std::filesystem::temp_directory_path(),
        };

        const int code = updater::cli::run_cli(CliOptions{}, deps);
        QVERIFY(code != 0);
        QCOMPARE(github.download_calls, 1);
        QCOMPARE(installer.install_calls, 0);
        QVERIFY(!err.str().empty());
    }

    void not_installed_with_yes_installs_per_user_with_accept_license() {
        FakeLocator locator;
        locator.installed = std::nullopt;

        FakeGitHubClient github;
        const std::string hash = updater::core::sha256_hex(github.msi_payload);
        github.release_json = make_release_json("0.6.2", hash);
        github.sums_text = hash + "  hitokoto-0.6.2-x64.msi\n";

        FakeMsiInstaller installer;
        std::ostringstream out;
        std::ostringstream err;
        updater::cli::RunnerDeps deps{
            .locator = locator,
            .github = github,
            .installer = installer,
            .out = out,
            .err = err,
            .confirm = [](const std::string&) { return false; },
            .app_version = "0.1.0",
            .temp_root = std::filesystem::temp_directory_path(),
        };

        CliOptions opts;
        opts.yes = true;
        const int code = updater::cli::run_cli(opts, deps);
        QCOMPARE(code, 0);
        QCOMPARE(github.download_calls, 1);
        QCOMPARE(installer.install_calls, 1);
        QCOMPARE(installer.last_options.scope, InstallScope::User);
        QVERIFY(installer.last_options.accept_license);
        QVERIFY(!installer.last_options.is_upgrade);
    }
};

QTEST_MAIN(tst_s001_cli)
#include "tst_s001_cli.moc"
