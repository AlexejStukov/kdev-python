/***************************************************************************
 *   This file is part of KDevelop                                         *
 *   Copyright 2012 Sven Brauch <svenbrauch@googlemail.com>                *
 *   Copyright 2014 Miquel Sabaté <mikisabate@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/


#include "refactoring.h"
#include "duchain/helpers.h"


namespace Python {

RefactoringCollector::RefactoringCollector(const IndexedDeclaration &decl)
    : BasicRefactoringCollector(decl)
{
    /* There's nothing to do in here.*/
}

void RefactoringCollector::processUses(KDevelop::ReferencedTopDUContext topContext)
{
    if (topContext != Helper::getDocumentationFileContext())
        RefactoringCollector::processUses(topContext);
}

Refactoring::Refactoring(QObject *parent)
    : BasicRefactoring(parent)
{
    /* There's nothing to do in here.*/
}

bool Refactoring::acceptForContextMenu(const KDevelop::Declaration* decl)
{
    if (decl->topContext() == Helper::getDocumentationFileContext()) {
        kDebug() << "in doc file, not offering rename action";
        return false;
    }
    return true;
}

}
