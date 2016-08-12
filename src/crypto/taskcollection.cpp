/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/taskcollection.cpp

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

#include "kleopatra_debug.h"

#include "taskcollection.h"

#include "crypto/task.h"
#include "utils/gnupg-helper.h"

#include <boost/bind.hpp>

#include <algorithm>
#include <map>

#include <cassert>
#include <cmath>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace boost;

class TaskCollection::Private
{
    TaskCollection *const q;
public:
    explicit Private(TaskCollection *qq);

    void taskProgress(const QString &, int, int);
    void taskResult(const shared_ptr<const Task::Result> &);
    void taskStarted();
    void calculateAndEmitProgress();

    std::map<int, shared_ptr<Task> > m_tasks;
    mutable quint64 m_totalProgress;
    mutable quint64 m_progress;
    unsigned int m_nCompleted;
    QString m_lastProgressMessage;
    bool m_errorOccurred;
    bool m_doneEmitted;
};

TaskCollection::Private::Private(TaskCollection *qq) : q(qq), m_totalProgress(0), m_progress(0), m_nCompleted(0), m_errorOccurred(false), m_doneEmitted(false)
{
}

int TaskCollection::numberOfCompletedTasks() const
{
    return d->m_nCompleted;
}

size_t TaskCollection::size() const
{
    return d->m_tasks.size();
}

bool TaskCollection::allTasksCompleted() const
{
    assert(d->m_nCompleted <= d->m_tasks.size());
    return d->m_nCompleted == d->m_tasks.size();
}

void TaskCollection::Private::taskProgress(const QString &msg, int, int)
{
    m_lastProgressMessage = msg;
    calculateAndEmitProgress();
}

void TaskCollection::Private::taskResult(const shared_ptr<const Task::Result> &result)
{
    assert(result);
    ++m_nCompleted;
    m_errorOccurred = m_errorOccurred || result->hasError();
    m_lastProgressMessage.clear();
    calculateAndEmitProgress();
    Q_EMIT q->result(result);
    if (!m_doneEmitted && q->allTasksCompleted()) {
        Q_EMIT q->done();
        m_doneEmitted = true;
    }
}

void TaskCollection::Private::taskStarted()
{
    const Task *const task = qobject_cast<Task *>(q->sender());
    assert(task);
    assert(m_tasks.find(task->id()) != m_tasks.end());
    Q_EMIT q->started(m_tasks[task->id()]);
    calculateAndEmitProgress(); // start Knight-Rider-Mode right away (gpgsm doesn't report _any_ progress).
}

void TaskCollection::Private::calculateAndEmitProgress()
{
    typedef std::map<int, shared_ptr<Task> >::const_iterator ConstIterator;

    quint64 total = 0;
    quint64 processed = 0;

    static bool haveWorkingProgress = hasFeature(0, GpgME::ProgressForCallbacks) && engineIsVersion(2, 1, 15);
    if (!haveWorkingProgress) {
        // GnuPG before 2.1.15 would overflow on progress values > max int.
        // and did not emit a total for our Qt data types.
        // As we can't know if it overflowed or what the total is we just knight
        // rider in that case
        if (m_doneEmitted) {
            Q_EMIT q->progress(m_lastProgressMessage, 1, 1);
        } else {
            Q_EMIT q->progress(m_lastProgressMessage, 0, 0);
        }
        return;
    }

    bool unknowable = false;
    for (ConstIterator it = m_tasks.begin(), end = m_tasks.end(); it != end; ++it) {
        // Sum up progress and totals
        const shared_ptr<Task> &i = it->second;
        assert(i);
        if (!i->totalProgress()) {
            // There still might be jobs for which we don't know the progress.
            qCDebug(KLEOPATRA_LOG) << "Task: " << i->label() << " has no total progress set. ";
            unknowable = true;
            break;
        }
        processed += i->currentProgress();
        total += i->totalProgress();
    }

    m_totalProgress = total;
    m_progress = processed;
    if (!unknowable && processed && total >= processed) {
        // Scale down to a promille value to avoid range issues.
        int scaled = 1000 * (m_progress / static_cast<double>(m_totalProgress));
        qCDebug(KLEOPATRA_LOG) << "Collection Progress: " << scaled << " total: " << 1000;
        Q_EMIT q->progress(m_lastProgressMessage, scaled, 1000);
    } else {
        if (total < processed) {
            qCDebug(KLEOPATRA_LOG) << "Total progress is smaller then current progress.";
        }
        // Knight rider.
        Q_EMIT q->progress(m_lastProgressMessage, 0, 0);
    }
}

TaskCollection::TaskCollection(QObject *parent) : QObject(parent), d(new Private(this))
{
}

TaskCollection::~TaskCollection()
{
}

bool TaskCollection::isEmpty() const
{
    return d->m_tasks.empty();
}

bool TaskCollection::errorOccurred() const
{
    return d->m_errorOccurred;
}

shared_ptr<Task> TaskCollection::taskById(int id) const
{
    const std::map<int, shared_ptr<Task> >::const_iterator it = d->m_tasks.find(id);
    return it != d->m_tasks.end() ? it->second : shared_ptr<Task>();
}

std::vector<shared_ptr<Task> > TaskCollection::tasks() const
{
    typedef std::map<int, shared_ptr<Task> >::const_iterator ConstIterator;
    std::vector<shared_ptr<Task> > res;
    res.reserve(d->m_tasks.size());
    for (ConstIterator it = d->m_tasks.begin(), end = d->m_tasks.end(); it != end; ++it) {
        res.push_back(it->second);
    }
    return res;
}

void TaskCollection::setTasks(const std::vector<shared_ptr<Task> > &tasks)
{
    Q_FOREACH (const shared_ptr<Task> &i, tasks) {
        assert(i);
        d->m_tasks[i->id()] = i;
        connect(i.get(), SIGNAL(progress(QString,int,int)),
                this, SLOT(taskProgress(QString,int,int)));
        connect(i.get(), SIGNAL(result(boost::shared_ptr<const Kleo::Crypto::Task::Result>)),
                this, SLOT(taskResult(boost::shared_ptr<const Kleo::Crypto::Task::Result>)));
        connect(i.get(), SIGNAL(started()),
                this, SLOT(taskStarted()));
    }
}

#include "moc_taskcollection.cpp"

