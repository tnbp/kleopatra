/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/resultitemwidget.h

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

#ifndef __KLEOPATRA_CRYPTO_GUI_RESULTITEMWIDGET_H__
#define __KLEOPATRA_CRYPTO_GUI_RESULTITEMWIDGET_H__

#include <QWidget>

#include <crypto/task.h>

#include <utils/pimpl_ptr.h>

#include <memory>

class QString;

namespace Kleo
{
namespace Crypto
{

class Task;

namespace Gui
{

class ResultItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ResultItemWidget(const std::shared_ptr<const Task::Result> &result, QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~ResultItemWidget();

    bool hasErrorResult() const;

    void showCloseButton(bool show);

public Q_SLOTS:
    void showAuditLog();

Q_SIGNALS:
    void linkActivated(const QString &link);
    void closeButtonClicked();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotLinkActivated(QString))
};
}
}
}

#endif // __KLEOPATRA_CRYPTO_GUI_RESULTITEMWIDGET_H__
