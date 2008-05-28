/* KDevelop xUnit plugin
 *
 * Copyright 2008 Manuel Breugelmans <mbr.nxi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef QXQTEST_QTESTSUITE_H
#define QXQTEST_QTESTSUITE_H

#include <QString>
#include <QFileInfo>
#include <QList>

#include "qtestbase.h"
#include "qtestcase.h"

namespace QxQTest
{

class QTestSuite : public QTestBase
{
Q_OBJECT
public:
    QTestSuite();
    QTestSuite(const QString&, const QFileInfo&, QTestBase* parent);
    virtual ~QTestSuite();

    void addTest(QTestCase* test);
    QTestCase* getTestAt(unsigned i);

    unsigned nrofChildren();
    QFileInfo path();

    void setPath(const QFileInfo&);

private:
    QFileInfo m_path;
    QList<QTestCase*> m_children;
};

} // end namespace QxQTest

#endif // QXQTEST_QTESTSUITE_H