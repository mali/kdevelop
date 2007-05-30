/* This file is part of KDevelop
    Copyright (C) 2006 Hamish Rodda <rodda@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef TESTDUCHAIN_H
#define TESTDUCHAIN_H

#include <QObject>
#include <QByteArray>

#include <kurl.h>

#include <ktexteditor/cursor.h>

#include "parser.h"
#include "control.h"

#include "typerepository.h"
#include "identifier.h"
#include "dumpchain.h"

namespace KDevelop
{
class Declaration;
class TopDUContext;
}

class TestDUChain : public QObject
{
  Q_OBJECT

public:
  TestDUChain();

  KDevelop::Declaration* findDeclaration(KDevelop::DUContext* context, const KDevelop::Identifier& id, const KTextEditor::Cursor& position = KTextEditor::Cursor::invalid());
  KDevelop::Declaration* findDeclaration(KDevelop::DUContext* context, const KDevelop::QualifiedIdentifier& id, const KTextEditor::Cursor& position = KTextEditor::Cursor::invalid());

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testIdentifiers();
  void testContextRelationships();
  void testDeclareInt();
  void testIntegralTypes();
  void testArrayType();
  void testDeclareFor();
  void testDeclareStruct();
  void testDeclareClass();
  void testDeclareNamespace();
  void testDeclareUsingNamespace();
  void testFileParse();

public:
  enum DumpArea {
    DumpNone = 0,
    DumpAST = 1,
    DumpDUChain = 2,
    DumpType = 4
  };
  Q_DECLARE_FLAGS(DumpAreas, DumpArea)

private:
  KDevelop::DUContext* parse(const QByteArray& unit, DumpAreas dump = static_cast<DumpAreas>(DumpAST | DumpDUChain | DumpType));

  void release(KDevelop::DUContext* top);

  // Parser
  Control control;
  DumpChain dumper;

  // Declaration - use chain
  KDevelop::Declaration* noDef;
  KUrl file1, file2;
  KDevelop::TopDUContext* topContext;

  KDevelop::AbstractType::Ptr typeVoid;
  KDevelop::AbstractType::Ptr typeInt;

  bool testFileParseOnly;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TestDUChain::DumpAreas)

#endif
