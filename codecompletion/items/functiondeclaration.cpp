/*****************************************************************************
 * Copyright (c) 2011 Sven Brauch <svenbrauch@googlemail.com>                *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU General Public License as            *
 * published by the Free Software Foundation; either version 2 of            *
 * the License, or (at your option) any later version.                       *
 *                                                                           *           
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *   
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************
 */

#include "functiondeclaration.h"

#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/aliasdeclaration.h>
#include <language/duchain/types/containertypes.h>
#include <shell/partcontroller.h>

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KLocalizedString>

#include "duchain/navigation/navigationwidget.h"
#include "codecompletion/helpers.h"
#include "declaration.h"
#include "declarations/functiondeclaration.h"
#include "duchain/helpers.h"

#include <QDebug>
#include "../codecompletiondebug.h"

using namespace KDevelop;
using namespace KTextEditor;

namespace Python {

FunctionDeclarationCompletionItem::FunctionDeclarationCompletionItem(DeclarationPointer decl, CodeCompletionContext::Ptr context) 
    : PythonDeclarationCompletionItem(decl, context)
    , m_atArgument(-1)
    , m_depth(0)
    , m_doNotCall(false)
{

}

int FunctionDeclarationCompletionItem::atArgument() const
{
    return m_atArgument;
}

void FunctionDeclarationCompletionItem::setDepth(int d)
{
    m_depth = d;
}

void FunctionDeclarationCompletionItem::setAtArgument(int d)
{
    m_atArgument = d;
}

int FunctionDeclarationCompletionItem::argumentHintDepth() const
{
    return m_depth;
}

QVariant FunctionDeclarationCompletionItem::data(const QModelIndex& index, int role, const KDevelop::CodeCompletionModel* model) const
{
    DUChainReadLocker lock;
    FunctionDeclaration* dec = dynamic_cast<FunctionDeclaration*>(m_declaration.data());
    switch ( role ) {
        case Qt::DisplayRole: {
            if ( ! dec ) {
                break; // use the default
            }
            if ( index.column() == KDevelop::CodeCompletionModel::Arguments ) {
                if (FunctionType::Ptr functionType = dec->type<FunctionType>()) {
                    QString ret;
                    createArgumentList(dec, ret, 0, 0, false);
                    return ret;
                }
            }
            if ( index.column() == KDevelop::CodeCompletionModel::Prefix ) {
                FunctionType::Ptr type = dec->type<FunctionType>();
                if ( type && type->returnType() ) {
                    return i18n("function") + " -> " + type->returnType()->toString();
                }
            }
            break;
        }
        case KDevelop::CodeCompletionModel::HighlightingMethod: {
            if ( index.column() == KDevelop::CodeCompletionModel::Arguments )
                return QVariant(KDevelop::CodeCompletionModel::CustomHighlighting);
            break;
        }
        case KDevelop::CodeCompletionModel::CustomHighlight: {
            if ( index.column() == KDevelop::CodeCompletionModel::Arguments ) {
                if ( ! dec ) return QVariant();
                QString ret;
                QList<QVariant> highlight;
                if ( atArgument() ) {
                    createArgumentList(dec, ret, &highlight, atArgument(), false);
                }
                else {
                    createArgumentList(dec, ret, 0, false);
                }
                return QVariant(highlight);
            }
        }
        case KDevelop::CodeCompletionModel::MatchQuality: {
            if (    m_typeHint == PythonCodeCompletionContext::IterableRequested
                 && dec && dec->type<FunctionType>()
                 && dynamic_cast<ListType*>(dec->type<FunctionType>()->returnType().data()) )
            {
                return 2 + PythonDeclarationCompletionItem::data(index, role, model).toInt();
            }
            return PythonDeclarationCompletionItem::data(index, role, model);
        }
    }
    return Python::PythonDeclarationCompletionItem::data(index, role, model);
}

void FunctionDeclarationCompletionItem::setDoNotCall(bool doNotCall)
{
    m_doNotCall = doNotCall;
}

void FunctionDeclarationCompletionItem::executed(KTextEditor::View* view, const KTextEditor::Range& word)
{
    qCDebug(KDEV_PYTHON_CODECOMPLETION) << "FunctionDeclarationCompletionItem executed";
    KTextEditor::Document* document = view->document();
    DeclarationPointer resolvedDecl(Helper::resolveAliasDeclaration(declaration().data()));
    DUChainReadLocker lock;
    QPair<FunctionDeclarationPointer, bool> fdecl = Helper::functionDeclarationForCalledDeclaration(resolvedDecl);
    lock.unlock();
    if ( ! fdecl.first && (! resolvedDecl || ! resolvedDecl->abstractType()
                           || resolvedDecl->abstractType()->whichType() != AbstractType::TypeStructure) ) {
        qCritical(KDEV_PYTHON_CODECOMPLETION) << "ERROR: could not get declaration data, not executing completion item!";
        return;
    }
    QString suffix = "()";
    KTextEditor::Range checkPrefix(word.start().line(), 0, word.start().line(), word.start().column());
    KTextEditor::Range checkSuffix(word.end().line(), word.end().column(), word.end().line(), document->lineLength(word.end().line()));
    if ( m_doNotCall || document->text(checkSuffix).trimmed().startsWith('(')
         || document->text(checkPrefix).trimmed().endsWith('@')
         || (fdecl.first && Helper::findDecoratorByName(fdecl.first.data(), QLatin1String("property"))) )
    {
        // don't insert brackets if they're already there,
        // the item is a decorator, or if it's an import item.
        suffix.clear();
    }
    // place cursor behind bracktes by default
    int skip = 2;
    if ( fdecl.first ) {
        bool needsArguments = false;
        int argumentCount = fdecl.first->type<FunctionType>()->arguments().length();
        if ( fdecl.first->context()->type() == KDevelop::DUContext::Class ) {
            // it's a member function, so it has the implicit self
            // TODO static methods
            needsArguments = argumentCount > 1;
        }
        else {
            // it's a free function
            needsArguments = argumentCount > 0;
        }
        if ( needsArguments ) {
            // place cursor in brackets if there's parameters
            skip = 1;
        }
    }
    document->replaceText(word, declaration()->identifier().toString() + suffix);
    view->setCursorPosition( Cursor(word.end().line(), word.end().column() + skip) );
}

FunctionDeclarationCompletionItem::~FunctionDeclarationCompletionItem() { }

}
