/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gnupg-helper.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "gnupg-helper.h"

#include "utils/hex.h"

#include <gpgme++/engineinfo.h>
#include <gpgme++/error.h>

#include "kleopatra_debug.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QProcess>
#include <QByteArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <gpg-error.h>

#ifdef Q_OS_WIN
#include "gnupg-registry.h"
#endif // Q_OS_WIN

#include <algorithm>
#include <array>

using namespace GpgME;

QString Kleo::gnupgHomeDirectory()
{
#ifdef Q_OS_WIN
    return QFile::decodeName(default_homedir());
#else
    const QByteArray gnupgHome = qgetenv("GNUPGHOME");
    if (!gnupgHome.isEmpty()) {
        return QFile::decodeName(gnupgHome);
    } else {
        return QDir::homePath() + QLatin1String("/.gnupg");
    }
#endif
}

int Kleo::makeGnuPGError(int code)
{
    return gpg_error(static_cast<gpg_err_code_t>(code));
}

static QString findGpgExe(GpgME::Engine engine, const QString &exe)
{
    const GpgME::EngineInfo info = GpgME::engineInfo(engine);
    return info.fileName() ? QFile::decodeName(info.fileName()) : QStandardPaths::findExecutable(exe);
}

QString Kleo::gpgConfPath()
{
    return findGpgExe(GpgME::GpgConfEngine, QStringLiteral("gpgconf"));
}

QString Kleo::gpgSmPath()
{
    return findGpgExe(GpgME::GpgSMEngine, QStringLiteral("gpgsm"));
}

QString Kleo::gpgPath()
{
    return findGpgExe(GpgME::GpgEngine, QStringLiteral("gpg"));
}

QStringList Kleo::gnupgFileWhitelist()
{
    return QStringList()
           // The obvious pubring
           << QStringLiteral("pubring.gpg")
           // GnuPG 2.1 pubring
           << QStringLiteral("pubring.kbx")
           // Trust in X509 Certificates
           << QStringLiteral("trustlist.txt")
           // Trustdb controls ownertrust and thus WOT validity
           << QStringLiteral("trustdb.gpg")
           // We want to update when smartcard status changes
           << QStringLiteral("reader*.status")
           // No longer used in 2.1 but for 2.0 we want this
           << QStringLiteral("secring.gpg")
           // Changes to the trustmodel * because you can have different
           // configs for different versions.
           << QStringLiteral("gpg.conf*")
           ;
}

QString Kleo::gpg4winInstallPath()
{
#ifdef Q_OS_WIN
    // QApplication::applicationDirPath is only used as a fallback
    // to support the case where Kleopatra is not installed from
    // Gpg4win but Gpg4win is also installed.
    char *instDir = read_w32_registry_string("HKEY_LOCAL_MACHINE",
                                             "Software/GPG4Win",
                                             "Install Directory");
    if (!instDir) {
        // Fallback to HKCU
        instDir = read_w32_registry_string("HKEY_CURRENT_USER",
                                           "Software/GPG4Win",
                                           "Install Directory");
    }
    if (instDir) {
        QString ret = QString::fromLocal8Bit(instDir) + QStringLiteral("/bin");
        free(instDir);
        return ret;
    }
    qCDebug(KLEOPATRA_LOG) << "Gpg4win not found. Falling back to Kleopatra instdir.";
#endif
    return QCoreApplication::applicationDirPath();
}

QString Kleo::gnupgInstallPath()
{
    return gpgConfListDir("bindir");
}

QString Kleo::gpgConfListDir(const char *which)
{
    if (!which || !*which) {
        return QString();
    }
    const QString gpgConfPath = Kleo::gpgConfPath();
    if (gpgConfPath.isEmpty()) {
        return QString();
    }
    QProcess gpgConf;
    qCDebug(KLEOPATRA_LOG) << "gpgConfListDir: starting " << qPrintable(gpgConfPath) << " --list-dirs";
    gpgConf.start(gpgConfPath, QStringList() << QStringLiteral("--list-dirs"));
    if (!gpgConf.waitForFinished()) {
        qCDebug(KLEOPATRA_LOG) << "gpgConfListDir(): failed to execute gpgconf: " << qPrintable(gpgConf.errorString());
        qCDebug(KLEOPATRA_LOG) << "output was:" << endl << gpgConf.readAllStandardError().constData();
        return QString();
    }
    const QList<QByteArray> lines = gpgConf.readAllStandardOutput().split('\n');
    Q_FOREACH (const QByteArray &line, lines)
        if (line.startsWith(which) && line[qstrlen(which)] == ':') {
            const int begin = qstrlen(which) + 1;
            int end = line.size();
            while (end && (line[end - 1] == '\n' || line[end - 1] == '\r')) {
                --end;
            }
            const QString result = QDir::fromNativeSeparators(QFile::decodeName(hexdecode(line.mid(begin, end - begin))));
            qCDebug(KLEOPATRA_LOG) << "gpgConfListDir: found " << qPrintable(result)
                                   << " for '" << which << "'entry";
            return result;
        }
    qCDebug(KLEOPATRA_LOG) << "gpgConfListDir(): didn't find '" << which << "'"
                           << "entry in output:" << endl << gpgConf.readAllStandardError().constData();
    return QString();
}

bool Kleo::engineIsVersion(int major, int minor, int patch, Engine engine)
{
    static QMap<Engine, std::array<int, 3> > cachedVersions;
    const int required_version[] = {major, minor, patch};
    // Gpgconf means spawning processes which is expensive on windows.
    std::array<int, 3> actual_version;
    if (!cachedVersions.contains(engine)) {
        const Error err = checkEngine(engine);
        if (err.code() == GPG_ERR_INV_ENGINE) {
            qCDebug(KLEOPATRA_LOG) << "isVersion: invalid engine. '";
            return false;
        }

        const char *actual = GpgME::engineInfo(engine).version();
        QRegExp rx(QStringLiteral("(\\d+)\\.(\\d+)\\.(\\d+)(?:-svn\\d+)?.*"));
        if (!rx.exactMatch(QString::fromUtf8(actual))) {
            qCDebug(KLEOPATRA_LOG) << "Can't parse version " << actual;
            return false;
        }
        bool ok;
        for (int i = 0; i < 3; ++i) {
            ok = false;
            actual_version[i] = rx.cap(i + 1).toUInt(&ok);
            assert(ok);
        }

        qCDebug(KLEOPATRA_LOG) << "Parsed" << actual << "as: "
                               << actual_version[0] << '.'
                               << actual_version[1] << '.'
                               << actual_version[2] << '.';
        cachedVersions.insert(engine, actual_version);
    } else {
        actual_version = cachedVersions.value(engine);
    }

    // return ! ( actual_version < required_version )
    return !std::lexicographical_compare(std::begin(actual_version), std::end(actual_version),
                                         std::begin(required_version), std::end(required_version));
}