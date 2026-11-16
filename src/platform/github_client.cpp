// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/github_client.hpp"

#include <QByteArray>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSaveFile>
#include <QString>
#include <QUrl>

namespace updater::platform {

namespace {

constexpr int kHttpTimeoutMs = 30000;
constexpr int kDownloadTimeoutMs = 300000;

QNetworkRequest make_request(const QString& url, const std::string& user_agent, bool github_api) {
    QNetworkRequest req{QUrl{url}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromStdString(user_agent));
    if (github_api) {
        req.setRawHeader("Accept", "application/vnd.github+json");
        req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    }
    return req;
}

HttpTextResult execute_get_text(const QString& url, const std::string& user_agent, bool github_api) {
    QNetworkAccessManager manager;
    auto req = make_request(url, user_agent, github_api);
    req.setTransferTimeout(kHttpTimeoutMs);

    QNetworkReply* reply = manager.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray bytes = reply->readAll();
    const auto err = reply->error();
    const QString err_str = reply->errorString();
    reply->deleteLater();

    if (err != QNetworkReply::NoError || status < 200 || status >= 300) {
        return {
            .ok = false,
            .status_code = status,
            .body = bytes.toStdString(),
            .error = err_str.toStdString(),
        };
    }
    return {
        .ok = true,
        .status_code = status,
        .body = bytes.toStdString(),
        .error = {},
    };
}

}  // namespace

HttpTextResult QtGitHubClient::fetch_latest_release_json(const std::string& repo, const std::string& user_agent) {
    const QString url =
        QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QString::fromStdString(repo));
    return execute_get_text(url, user_agent, true);
}

HttpTextResult QtGitHubClient::download_text(const std::string& url, const std::string& user_agent) {
    return execute_get_text(QString::fromStdString(url), user_agent, false);
}

HttpDownloadResult QtGitHubClient::download_file(
    const std::string& url, const std::filesystem::path& destination, const std::string& user_agent,
    const std::function<void(std::uint64_t received, std::int64_t total)>& progress_cb) {
    QSaveFile file(QString::fromStdString(destination.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return {
            .ok = false,
            .status_code = 0,
            .error = "Failed to open destination file: " + file.errorString().toStdString(),
        };
    }

    QNetworkAccessManager manager;
    auto req = make_request(QString::fromStdString(url), user_agent, false);
    req.setTransferTimeout(kDownloadTimeoutMs);

    QNetworkReply* reply = manager.get(req);
    bool write_failed = false;
    std::string write_error;
    std::uint64_t received_bytes = 0;

    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&]() {
        const QByteArray chunk = reply->readAll();
        if (!chunk.isEmpty() && !write_failed) {
            if (file.write(chunk) != chunk.size()) {
                write_failed = true;
                write_error = file.errorString().toStdString();
                reply->abort();
            } else {
                received_bytes += static_cast<std::uint64_t>(chunk.size());
            }
        }
    });

    if (progress_cb) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [&](qint64 received, qint64 total) {
            progress_cb(static_cast<std::uint64_t>(received), static_cast<std::int64_t>(total));
        });
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray remaining = reply->readAll();
    if (!remaining.isEmpty() && !write_failed) {
        if (file.write(remaining) != remaining.size()) {
            write_failed = true;
            write_error = file.errorString().toStdString();
        } else {
            received_bytes += static_cast<std::uint64_t>(remaining.size());
        }
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto err = reply->error();
    const QString err_str = reply->errorString();
    reply->deleteLater();

    if (write_failed || err != QNetworkReply::NoError || status < 200 || status >= 300 || received_bytes == 0) {
        file.cancelWriting();
        return {
            .ok = false,
            .status_code = status,
            .error = write_failed ? write_error : err_str.toStdString(),
        };
    }

    if (!file.commit()) {
        return {
            .ok = false,
            .status_code = status,
            .error = "Failed to commit downloaded file: " + file.errorString().toStdString(),
        };
    }

    return {.ok = true, .status_code = status, .error = {}};
}

}  // namespace updater::platform
