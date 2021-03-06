/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/signencryptfilescontroller.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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

#ifndef __KLEOPATRA_CRYPTO_SIGNENCRYPTFILESCONTROLLER_H__
#define __KLEOPATRA_CRYPTO_SIGNENCRYPTFILESCONTROLLER_H__

#include <crypto/controller.h>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>
#include <kmime/kmime_header_parsing.h>

#include <memory>
#include <vector>

namespace Kleo
{
namespace Crypto
{

class SignEncryptFilesController : public Controller
{
    Q_OBJECT
public:
    explicit SignEncryptFilesController(QObject *parent = nullptr);
    explicit SignEncryptFilesController(const std::shared_ptr<const ExecutionContext> &ctx, QObject *parent = nullptr);
    ~SignEncryptFilesController() override;

    void setProtocol(GpgME::Protocol proto);
    GpgME::Protocol protocol() const;
    //const char * protocolAsString() const;

    enum Operation {
        SignDisallowed = 0,
        SignAllowed = 1,
        SignSelected  = 2,

        SignMask = SignAllowed | SignSelected,

        EncryptDisallowed = 0,
        EncryptAllowed = 4,
        EncryptSelected = 8,

        EncryptMask = EncryptAllowed | EncryptSelected,

        ArchiveDisallowed = 0,
        ArchiveAllowed = 16,
        ArchiveForced = 32,

        ArchiveMask = ArchiveAllowed | ArchiveForced
    };
    void setOperationMode(unsigned int mode);
    unsigned int operationMode() const;

    void setFiles(const QStringList &files);

    void start();

public Q_SLOTS:
    void cancel();

private:
    void doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &) override;

    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotWizardOperationPrepared())
    Q_PRIVATE_SLOT(d, void slotWizardCanceled())
    Q_PRIVATE_SLOT(d, void schedule())
};

}
}

#endif /* __KLEOPATRA_UISERVER_SIGNENCRYPTFILESCONTROLLER_H__ */

