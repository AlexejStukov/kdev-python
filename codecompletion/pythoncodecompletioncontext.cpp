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

#include <math.h>

#include <QProcess>
#include <QRegExp>
#include <KStandardDirs>
#include <KTextEditor/View>

#include <language/duchain/duchainpointer.h>
#include <language/duchain/declaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/codecompletion/codecompletionitem.h>
#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include <language/codecompletion/abstractincludefilecompletionitem.h>
#include <language/codecompletion/codecompletionitem.h>
#include <language/util/includeitem.h>
#include <language/codecompletion/codecompletionitemgrouper.h>
#include <language/duchain/aliasdeclaration.h>
#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/idocumentcontroller.h>
#include <project/projectmodel.h>

#include "keyworditem.h"
#include "pythoncodecompletionworker.h"
#include "astbuilder.h"
#include "expressionvisitor.h"
#include "navigation/navigationwidget.h"
#include "importfileitem.h"
#include "functiondeclarationcompletionitem.h"
#include "pythoncodecompletioncontext.h"
#include "pythoneditorintegrator.h"
#include "duchain/declarationbuilder.h"
#include "implementfunctioncompletionitem.h"
#include "types/unsuretype.h"
#include "duchain/helpers.h"

using namespace KTextEditor;
using namespace KDevelop;

typedef QStringList implementFunctionDescription;

namespace Python {

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::completionItems(bool& abort, bool fullCompletion)
{
    if ( abort ) 
        return QList<CompletionTreeItemPointer>();
    
    QList<CompletionTreeItemPointer> items;
    DUChainReadLocker lock(DUChain::lock());
    
    kDebug() << "Line: " << m_position.line;
//     if ( m_position.line == 0 ) { // TODO group those correctly so they appear at the top
//         items << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python"));
//         items << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python2.6"));
//         items << CompletionTreeItemPointer(new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "#!/usr/bin/env python2.7"));
//     }
    
    if ( m_operation == PythonCodeCompletionContext::NoCompletion ) {
        
    }
    else if ( m_operation == PythonCodeCompletionContext::GeneratorVariableCompletion ) {
        QList<KeywordItem*> completionItems;
        KDevPG::MemoryPool pool;
        AstBuilder* builder = new AstBuilder(&pool);
        CodeAst* tmpAst = builder->parse(KUrl(), m_remainingExpression);
        if ( tmpAst ) {
            DUChainReadLocker lock(DUChain::lock());
            ExpressionVisitor* v = new ExpressionVisitor(m_context.data());
            v->m_forceGlobalSearching = true;
            v->m_reportUnknownNames = true;
            v->visitCode(tmpAst);
            lock.unlock();
            if ( not v->m_unknownNames.isEmpty() ) {
                if ( v->m_unknownNames.size() >= 2 ) {
                    // we only take the first two, and only two. It gets too much items otherwise.
                    QStringList combinations;
                    combinations << v->m_unknownNames.at(0) + ", " + v->m_unknownNames.at(1);
                    combinations << v->m_unknownNames.at(1) + ", " + v->m_unknownNames.at(0);
                    foreach ( const QString& c, combinations ) {
                        completionItems << new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "" + c + " in ");
                    }
                }
                foreach ( const QString& n, v->m_unknownNames ) {
                    completionItems << new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), "" + n + " in ");
                }
            }
            else {
                kWarning() << "No unknown names in generator completion; nothing to do.";
            }
            delete v;
        }
        delete builder;
        
        foreach ( KeywordItem* item, completionItems ) {
            items << CompletionTreeItemPointer(item);
        }
        
//         IDocument* doc = KDevelop::ICore::self()->documentController()->documentForUrl(m_context->topContext()->url().toUrl());
//         QMetaObject::invokeMethod(doc->textDocument()->activeView(), "userInvokedCompletion");
    }
    else if ( m_operation == PythonCodeCompletionContext::DefineCompletion ) {
        QList<implementFunctionDescription> funcs;
        // well, duh. I didn't think it's that many functions. TODO think of a more sane way to do this
        {
        funcs << ( implementFunctionDescription() << "__init__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__new__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__del__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__repr__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__str__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__lt__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__gt__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__le__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__eq__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ne__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__gt__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ge__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__cmp__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__hash__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__nonzero__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__unicode__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__getattr__" << "self, <string> name" << "self, name" );
        funcs << ( implementFunctionDescription() << "__setattr__" << "self, <string> name, <any object> value" << "self, name, value" );
        funcs << ( implementFunctionDescription() << "__delattr__" << "self, <string> name" << "self, name" );
        funcs << ( implementFunctionDescription() << "__getattribute__" << "self, <string> name" << "self, name" );
        funcs << ( implementFunctionDescription() << "__get__" << "self, <any object> instance, <class> owner" << "self, instance, owner" );
        funcs << ( implementFunctionDescription() << "__set__" << "self, <any object> instance, <any object> value" << "self, instance, value" );
        funcs << ( implementFunctionDescription() << "__delete__" << "self, <any object> instance" << "self, instance" );
        funcs << ( implementFunctionDescription() << "__instancecheck__" << "self, <any object> instance" << "self, instance" );
        funcs << ( implementFunctionDescription() << "__subclasscheck__" << "self, <any object> subclass" << "self, subclass" );
        funcs << ( implementFunctionDescription() << "__call__" << "self, [...args]" << "self" );
        funcs << ( implementFunctionDescription() << "__len__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__getitem__" << "self, <string> key" << "self, key" );
        funcs << ( implementFunctionDescription() << "__setitem__" << "self, <string> key, <any object> value" << "self, key, value" );
        funcs << ( implementFunctionDescription() << "__delitem__" << "self, <string> key" << "self, key" );
        funcs << ( implementFunctionDescription() << "__iter__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__reversed__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__contains__" << "self, <any object> item" << "self, item" );
        funcs << ( implementFunctionDescription() << "__getslice__" << "self, <int> i, <int> j" << "self, i, j" );
        funcs << ( implementFunctionDescription() << "__delslice__" << "self, <int> i, <int> j" << "self, i, j" );
        funcs << ( implementFunctionDescription() << "__add__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__sub__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__mul__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__floordiv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__mod__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__divmod__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__pow__" << "self, <any object> other, [modulo]" << "self, other" );
        funcs << ( implementFunctionDescription() << "__lshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__and__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__xor__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__or__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__div__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__truediv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__radd__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rsub__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rmul__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rtruediv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rfloordiv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rmod__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rdivmod__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rpow__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rlshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rrshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rand__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__rxor__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ror__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__iadd__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__isub__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__imul__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__idiv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__itruediv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ifloordiv__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__imod__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ipow__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ilshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__irshift__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__iand__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ixor__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__ior__" << "self, <any object> other" << "self, other" );
        funcs << ( implementFunctionDescription() << "__neg__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__pos__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__abs__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__invert__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__complex__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__int__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__long__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__float__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__oct__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__hex__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__index__" << "self" << "self" );
        funcs << ( implementFunctionDescription() << "__coerce__" << "self, <any object> other" << "self, other" );
        }

        foreach ( implementFunctionDescription func, funcs ) {
            items << CompletionTreeItemPointer(new ImplementFunctionCompletionItem(func.at(0), func.at(1), func.at(2), m_indent));
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::ImportFileCompletion ) {
        kDebug() << "Preparing to do autocompletion for import...";
        m_maxFolderScanDepth = 1;
        foreach ( ImportFileItem* item, includeFileItems(Helper::getSearchPaths(m_workingOnDocument)) ) {
            Q_ASSERT(item);
            QString relativeUrl = KUrl::relativeUrl(m_workingOnDocument, item->includeItem.basePath);
            QString absoluteUrl = item->includeItem.basePath.path();
            // use whichever one is shorter
            QString useUrl = relativeUrl.length() < absoluteUrl.length() ? relativeUrl : absoluteUrl;
            item->includeItem.name = QString(item->moduleName + " (from " + useUrl + ")");
            items << CompletionTreeItemPointer( item );
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::RaiseExceptionCompletion ) {
        kDebug() << "Finding items for raise statement";
        ReferencedTopDUContext ctx = Helper::getDocumentationFileContext();
        QList< Declaration* > declarations = ctx->findDeclarations(QualifiedIdentifier("BaseException"));
        if ( declarations.isEmpty() || ! declarations.first()->abstractType() ) {
            kDebug() << "No valid exception classes found, aborting";
        }
        else {
            Declaration* base = declarations.first();
            IndexedType baseType = base->abstractType()->indexed();
            QList<DeclarationDepthPair> validDeclarations;
            ClassDeclaration* current = 0;
            StructureType::Ptr type;
            foreach ( DeclarationDepthPair d, m_context->topContext()->allDeclarations(CursorInRevision::invalid(), m_context->topContext()) ) {
                if ( ( current = dynamic_cast<ClassDeclaration*>(d.first) ) ) {
                    if ( current->baseClassesSize() ) {
                        FOREACH_FUNCTION( const BaseClassInstance& base, current->baseClasses ) {
                            if ( base.baseClass == baseType ) {
                                validDeclarations << d;
                            }
                        }
                    }
                }
            }
            items.append(declarationListToItemList(validDeclarations));
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::ImportSubCompletion ) {
        kDebug() << "Finding items for submodule: " << m_subForModule;
        foreach ( ImportFileItem* item, includeFileItemsForSubmodule(m_subForModule) ) {
            Q_ASSERT(item);
            item->includeItem.name = QString(item->moduleName + " (from " + KUrl::relativeUrl(m_workingOnDocument, item->includeItem.basePath) + ")");
            items << CompletionTreeItemPointer( item );
        }
    }
    else if ( m_operation == PythonCodeCompletionContext::InheritanceCompletion ) {
        kDebug() << "InheritanceCompletion";
        QList<DeclarationDepthPair> declarations = m_duContext->allDeclarations(m_position, m_duContext->topContext());
        QList<DeclarationDepthPair> remainingDeclarations;
        foreach ( DeclarationDepthPair d, declarations ) {
            Declaration* r = Helper::resolveAliasDeclaration(d.first);
            if ( r and r->identifier().identifier().str().contains("__kdevpythondocumentation_builtin") ) {
                continue;
            }
            if ( r && dynamic_cast<ClassDeclaration*>(r) ) {
                remainingDeclarations << d;
            }
        }
        items.append(declarationListToItemList(remainingDeclarations));
    }
    else if ( m_operation == PythonCodeCompletionContext::MemberAccessCompletion ) {
        KDevPG::MemoryPool pool;
        AstBuilder* builder = new AstBuilder(&pool);
        CodeAst* tmpAst = builder->parse(KUrl(), m_guessTypeOfExpression);
        if ( tmpAst ) {
            DUChainReadLocker lock(DUChain::lock());
            ExpressionVisitor* v = new ExpressionVisitor(m_context.data());
            v->m_forceGlobalSearching = true;
            v->visitCode(tmpAst);
            lock.unlock();
            if ( v->lastType() ) {
                kDebug() << v->lastType()->toString();
                items = getCompletionItemsForType(v->lastType());
            }
            else {
                kWarning() << "Did not receive a type from expression visitor! Not offering autocompletion.";
            }
            delete v;
        }
        else {
            kWarning() << "Completion requested for syntactically invalid expression, not offering anything";
        }
        delete builder;
    }
    else {
        // it's stupid to display a 3-letter completion item on manually invoked code completion and makes everything look crowded
        if ( m_operation == PythonCodeCompletionContext::NewStatementCompletion && ! fullCompletion ) {
            QStringList keywordItems;
            keywordItems << "def" << "class" << "lambda" << "global" << "print" << "import" << "from" << "while" << "for" << "yield" << "return";
            foreach ( const QString& current, keywordItems ) {
                KeywordItem* k = new KeywordItem(KDevelop::CodeCompletionContext::Ptr(this), current + " ");
                items << CompletionTreeItemPointer(k);
            }
        }
        if ( abort ) {
            return QList<CompletionTreeItemPointer>();
        }
        if ( m_operation == PythonCodeCompletionContext::FunctionCallCompletion ) {
            // gather additional items to show above the real ones (for parameters, and stuff)
            QList<Declaration*> calltips;
            KDevPG::MemoryPool pool;
            AstBuilder* builder = new AstBuilder(&pool);
            CodeAst* tmpAst = builder->parse(KUrl(), m_guessTypeOfExpression);
            if ( tmpAst ) {
                DUChainReadLocker lock(DUChain::lock());
                ExpressionVisitor* v = new ExpressionVisitor(m_context.data());
                v->m_forceGlobalSearching = true;
                v->visitCode(tmpAst);
                lock.unlock();
                if ( v->lastDeclaration().data() ) {
                    calltips << v->lastDeclaration().data();
                }
                else {
                    kWarning() << "Did not receive a function declaration from expression visitor! Not offering call tips.";
                }
                delete v;
            }
            
            QList<DeclarationDepthPair> realCalltips_withDepth;
            foreach ( Declaration* current, calltips ) {
                if ( ! dynamic_cast<FunctionDeclaration*>(current) ) {
                    kDebug() << "Not a function declaration: " << current->toString();
                    continue;
                }
                realCalltips_withDepth.append(DeclarationDepthPair(current, 0));
            }
            
            QList<CompletionTreeItemPointer> calltipItems = declarationListToItemList(realCalltips_withDepth);
            foreach ( CompletionTreeItemPointer current, calltipItems ) {
                kDebug() << "Adding calltip item, at argument:" << m_alreadyGivenParametersCount+1; 
                static_cast<FunctionDeclarationCompletionItem*>(current.data())->setAtArgument(m_alreadyGivenParametersCount + 1);
            }
            
            items.append(calltipItems);
        }
        QList<DeclarationDepthPair> declarations = m_duContext->allDeclarations(m_position, m_duContext->topContext());
        foreach ( DeclarationDepthPair d, declarations ) {
            if ( d.first and d.first->context()->type() == DUContext::Class ) {
                declarations.removeAll(d);
            }
            if ( d.first and d.first->identifier().identifier().str().contains("__kdevpythondocumentation_builtin") ) {
                declarations.removeAll(d);
            }
        }
        items.append(declarationListToItemList(declarations));
    }
    
    m_searchingForModule.clear();
    m_subForModule.clear();
    
    return items;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::declarationListToItemList(QList<DeclarationDepthPair> declarations, int maxDepth)
{
    QList<CompletionTreeItemPointer> items;
    
    DeclarationPointer currentDeclaration;
    DUChainPointer<Declaration> checkDeclaration;
    int count = declarations.length();
    for ( int i = 0; i < count; i++ ) {
        if ( maxDepth && maxDepth > declarations.at(i).second ) {
            kDebug() << "Skipped completion item because of its depth";
            continue;
        }
        currentDeclaration = DeclarationPointer(declarations.at(i).first);
        kDebug() << declarations.first().first->comment();
        
        PythonDeclarationCompletionItem* item;
        AliasDeclaration* alias = dynamic_cast<AliasDeclaration*>(currentDeclaration.data());
        if ( alias ) {
            DUChainReadLocker lock(DUChain::lock());
            checkDeclaration = DUChainPointer<Declaration>(alias->aliasedDeclaration().declaration());
        }
        else {
            checkDeclaration = currentDeclaration;
        }
        if ( checkDeclaration ) {
            AbstractType::Ptr type = checkDeclaration->abstractType();
            if ( type && ( type->whichType() == AbstractType::TypeFunction || type->whichType() == AbstractType::TypeStructure ) ) {
                item = new FunctionDeclarationCompletionItem(currentDeclaration);
            }
            else {
                item = new PythonDeclarationCompletionItem(currentDeclaration, KDevelop::CodeCompletionContext::Ptr(this));
            }
            items << CompletionTreeItemPointer(item);
        }
    }
    return items;
}

QList< CompletionTreeItemPointer > PythonCodeCompletionContext::getCompletionItemsForType(AbstractType::Ptr type)
{
    QList<CompletionTreeItemPointer> result;
    type = Helper::resolveType(type);
    if ( type->whichType() == AbstractType::TypeUnsure ) {
        UnsureType::Ptr unsure = type.cast<UnsureType>();
        int count = unsure->typesSize();
        kDebug() << "Getting completion items for " << count << "types of unsure type " << unsure;
        for ( int i = 0; i < count; i++ ) {
            result.append(getCompletionItemsForOneType(unsure->types()[i].abstractType()));
        }
    }
    else {
        result = getCompletionItemsForOneType(type);
    }
    return result;
}

QList<CompletionTreeItemPointer> PythonCodeCompletionContext::getCompletionItemsForOneType(AbstractType::Ptr type)
{
    type = Helper::resolveType(type);
    if ( type->whichType() == AbstractType::TypeStructure ) {
        // find properties of class declaration
        TypePtr<StructureType> cls = StructureType::Ptr::dynamicCast(type);
        kDebug() << "Finding completion items for class type";
        if ( ! cls || ! cls->internalContext(m_context->topContext()) ) {
            kWarning() << "No class type available, no completion offered";
            kDebug() << cls;
            return QList<CompletionTreeItemPointer>();
        }
        QList<DUContext*> searchContexts = Helper::internalContextsForClass(cls, m_context->topContext());
        QList<DeclarationDepthPair> keepDeclarations;
        foreach ( const DUContext* currentlySearchedContext, searchContexts ) {
            kDebug() << "searching context " << currentlySearchedContext->scopeIdentifier() << "for autocompletion items";
            QList<DeclarationDepthPair> declarations = currentlySearchedContext->allDeclarations(CursorInRevision::invalid(), m_context->topContext(), false);
            kDebug() << "found" << declarations.length() << "declarations";
            
            // filter out those which are builtin functions, and those which were imported; we don't want those here
            // TODO rework this, it's maybe not the most elegant solution possible
            KUrl url = KUrl(KStandardDirs::locate("data", "kdevpythonsupport/documentation_files/builtindocumentation.py"));
            url.cleanPath();
            QString u = url.path();
            foreach ( DeclarationDepthPair current, declarations ) {
                if ( current.first->context() != DUChain::self()->chainForDocument(url) ) {
                    kDebug() << "Keeping declaration" << current.first->toString();
                    keepDeclarations.append(current);
                }
                else {
                    kDebug() << "Discarding declaration " << current.first->toString();
                }
            }
        }
        return declarationListToItemList(keepDeclarations);
    }
    
    QList<CompletionTreeItemPointer> items;
    return items;
}

QList< ImportFileItem* > PythonCodeCompletionContext::includeFileItemsForSubmodule(QString submodule)
{
    QList<ImportFileItem*> items;
    QList<KUrl> searchPaths = Helper::getSearchPaths(m_workingOnDocument);
    QStringList subdirs = submodule.split(".");
    QList<KUrl> foundPaths;
    
    // this is a bit tricky. We need to find every path formed like /.../foo/bar for
    // a query string ("submodule" variable) like foo.bar
    // we also need paths like /foo.py, because then bar is probably a module in that file.
    // Thus, we first generate a list of possible paths, then match them against those which actually exist
    // and then gather all the items in those paths.
    
    foreach (KUrl currentPath, searchPaths) {
        bool exists = true;
        kDebug() << "Searching: " << currentPath;
        foreach ( QString subdir, subdirs ) {
            exists = currentPath.cd(subdir);
            kDebug() << currentPath;
            if ( ! exists ) break;
        }
        if ( exists ) {
            foundPaths.append(currentPath);
            kDebug() << "Found path: exists";
        }
    }
    return includeFileItems(foundPaths);
}

QList<ImportFileItem*> PythonCodeCompletionContext::includeFileItems(QList<KUrl> searchPaths) {
    QList<ImportFileItem*> items;
    
    kDebug() << "Gathering include file autocompletions...";
    
    foreach (KUrl currentPath, searchPaths) {
        currentPath.cleanPath();
        if ( currentPath.path(KUrl::AddTrailingSlash).startsWith(Helper::getDataDir()) ) {
            // don't suggest stuff from the docfiles directory
            continue;
        }
        kDebug() << "Processing path: " << currentPath;
        QDir currentDir(currentPath.path());
        QFileInfoList files = currentDir.entryInfoList();
        QStringList alreadyFound;
        foreach (QFileInfo file, files) {
            kDebug() << "Scanning file: " << file.absoluteFilePath();
            if ( file.fileName() == "." || file.fileName() == ".." ) continue;
            if ( file.fileName().endsWith(".py") || file.fileName().endsWith(".pyc") || file.isDir() ) {
                IncludeItem includeItem;
                includeItem.basePath = currentPath.path(KUrl::AddTrailingSlash) + file.baseName();
                includeItem.name = file.fileName();
                includeItem.isDirectory = file.isDir();
                ImportFileItem* item = new ImportFileItem(includeItem);
                item->moduleName = file.fileName().replace(".pyc", "").replace(".pyo", "").replace(".py", "");
                if ( alreadyFound.contains(item->moduleName) ) {
                    continue;
                }
                else {
                    alreadyFound << item->moduleName;
                }
                items.append(item);
                kDebug() << "FOUND: " << file.absoluteFilePath();
            }
        }
    }
    
    return items;
}

// decide what kind of completion will be offered based on the code before the current cursor position
// lazy as we are, we use regular expression matching for this
PythonCodeCompletionContext::PythonCodeCompletionContext(DUContextPointer context, const QString& text, const KDevelop::CursorInRevision& position, 
                                                         int depth, const PythonCodeCompletionWorker* parent)
    : CodeCompletionContext(context, text, position, depth)
    , m_operation(PythonCodeCompletionContext::DefaultCompletion)
    , parent(parent)
    , m_position(position)
    , m_context(context)
{
    kDebug() << "Text: " << text;
    m_workingOnDocument = parent->parent->m_currentDocument;
    QString currentLine = "\n" + text.split("\n").last(); // we'll only look at the last line for now, as 
                                                          // 99% of python statements are limited to one line TODO fix this
    int atLine = position.line;
    kDebug() << "Doing auto-completion context scan for: " << currentLine << "@line" << atLine << "@context" << context->range().castToSimpleRange();
    
//     bool currentLineIsEmpty = true;
//     {
//         QChar c;
//         for ( int i = currentLine.length() - 1; i >= 0; i-- ) {
//             c = currentLine.at(i);
//             if ( ! c.isSpace() ) {
//                 currentLineIsEmpty = false;
//                 break;
//             }
//         }
//     }
    
    // check if the current position is inside a multi-line comment / string
    bool insideSingleQuotes = false;
    bool insideDoubleQuotes = false;
    bool insideMultiLineComment = false;
    bool insideSingleLineComment = false;
    const int max_len = text.length();
    kDebug() << "Checking for comment line or string literal...";
    for ( int atChar = 0; atChar < max_len; atChar++ ) {
        const QChar& c = text.at(atChar);
        QString t("");
        if ( max_len - atChar > 2 ) {
            for ( int i = 0; i < 3; i++ ) {
                t.append(text.at(atChar+i));
            }
        }
        kDebug() << atChar << t;
        if ( c == '#' ) {
            insideSingleLineComment = true;
            continue;
        }
        if ( c == '\n' ) {
            insideSingleLineComment = false;
            continue;
        }
        if ( t == "\"\"\"" ) {
            insideMultiLineComment = !insideMultiLineComment;
            continue;
        }
        if ( c == '\'' ) {
            insideSingleQuotes = !insideSingleQuotes;
            continue;
        }
        if ( c == '"' ) {
            insideDoubleQuotes = !insideDoubleQuotes;
            continue;
        }
    }
    
    if ( insideSingleLineComment || insideSingleQuotes || insideMultiLineComment || insideDoubleQuotes ) {
        m_operation = PythonCodeCompletionContext::NoCompletion;
    }
    
    // Our contexts end too early. They end at the last valid token of a function or such,
    // but not at the DEDENT token. This means, if a function ends with 5 empty but indented lines, and you
    // place your cursor in the 3rd one and start typing, there's no completion for variables local to the function.
    // Thus, we walk back in the text line-by-line, searching for some context which has the same
    // indent like the current one and is directly before it in the code.
    DUContext* currentlyChecked = context.data();
    int currentlyCheckedLine = atLine;
    {
        DUChainReadLocker lock(DUChain::lock());
        while ( currentlyChecked == context.data() && currentlyCheckedLine >= 0 ) {
            currentlyChecked = context->topContext()->findContextAt(CursorInRevision(currentlyCheckedLine, 0));
            currentlyCheckedLine -= 1;
        }
    }
    
    // cool, we found something. Now we need to compare its indent to the current one; if they don't match, then 
    // we ignore it and use the one provided as an argument to this function. Otherwise, we use what we found.
    if ( currentlyChecked and currentlyChecked->range().start.line < currentlyChecked->range().end.line ) {
        int previousStartsAtLine = currentlyChecked->range().castToSimpleRange().start.line;
        if ( currentlyChecked->type() != DUContext::Class ) {
            previousStartsAtLine += 1;
        }
        kDebug() << "Previous context starts at line" << previousStartsAtLine;
        kDebug() << "Previous / Current context ranges: " << currentlyChecked->range().castToSimpleRange() << context->range().castToSimpleRange();
        int skipLinesBack = atLine - previousStartsAtLine; // how many lines to skip backwards
        int i = text.length();
        QChar newline('\n');
        
        // init indents array
        QMap<int, int> indentForLine;
        const int invalid = -1;
        indentForLine[atLine] = invalid; indentForLine[previousStartsAtLine] = invalid;
        
        // check if the previous context uses the same indent like the current line
        int currentIndent = 0;
        int skippedLines = 0;
        QChar c;
        while ( i > 0 ) {
            i -= 1;
            c = text.at(i);
            if ( c == newline ) {
                skippedLines += 1;
                indentForLine[atLine - skippedLines + 1] = currentIndent;
                kDebug() << "Indent for line" << atLine - skippedLines + 1 << " : " << currentIndent;
                currentIndent = 0;
            }
            else if ( c.isSpace() ) {
                currentIndent += 1;
            }
            else {
                // reset if non-space, so we don't count whitespaces within the line
                currentIndent = 0;
            }
            if ( skippedLines > ( skipLinesBack + 2 ) ) {
                break;
            }
        }
        kDebug() << indentForLine << skipLinesBack << skippedLines << "Previous ends at: " << previousStartsAtLine;
        
        // if the indents match, use the context which was found.
        // if those are still "invalid", then the scanner has not reached them, meaning it aborted scanning because
        // even a match would not have meant that the context has to be replaced
        if ( previousStartsAtLine != atLine && ( indentForLine[previousStartsAtLine] != invalid ) && 
             ( indentForLine[previousStartsAtLine] == indentForLine[atLine] ) ) {
            kDebug() << "Indents match, replacing context by" << currentlyChecked;
            context = DUContextPointer(currentlyChecked);
            m_duContext = context;
            DUChainReadLocker lock(DUChain::lock()); // TODO remove debug
            kDebug() << "New context: " << context->scopeIdentifier().toString() << context->range().castToSimpleRange();
        }
        else {
            kDebug() << "Indents mismatch, so the given context is correct.";
        }
    }
    
    QRegExp raise("^[\\s]*raise(.*)$");
    raise.setMinimal(true);
    bool isRaise = raise.exactMatch(currentLine);
    if ( isRaise ) {
        m_operation = PythonCodeCompletionContext::RaiseExceptionCompletion;
        return;
    }
    
    //                                                 v   v   v   v   v allow comma seperated list of imports
    QRegExp importsub("^[\\s]*from(.*)import[\\s]*(.*[\\s]*,[\\s]*)*$");
    importsub.setMinimal(true);
    bool is_importSub = importsub.exactMatch(currentLine);
    QRegExp importsub2("^[\\s]*(from|import)[\\s]*(.*)\\.$");
    importsub2.setMinimal(true);
    bool is_importSub2 = importsub2.exactMatch(currentLine);
    if ( is_importSub || is_importSub2 ) {
        QStringList for_module_match;
        if ( is_importSub ) for_module_match = importsub.capturedTexts();
        else for_module_match = importsub2.capturedTexts();
        
        kDebug() << for_module_match;
        
        QString for_module;
        if ( is_importSub ) for_module = for_module_match[1].replace(" ", "");
        else for_module = for_module_match[2].replace(" ", "");
        
        kDebug() << "Matching against module name: " << for_module << is_importSub << is_importSub2;
        m_operation = PythonCodeCompletionContext::ImportSubCompletion;
        m_subForModule = for_module;
        kDebug() << "submodule: " << for_module;
        return;
    }
    
    QRegExp newStatementCompletion("(.*)\n[\\s]*$");
    newStatementCompletion.setMinimal(true);
    bool isNewStatementCompletion = newStatementCompletion.exactMatch(currentLine);
    if ( isNewStatementCompletion ) {
        m_operation = PythonCodeCompletionContext::NewStatementCompletion;
        return;
    }
    
    //                                          v   v   v   v   v allow comma seperated list of imports
    QRegExp importfile("^[\\s]*import[\\s]*(.*[\\s]*,[\\s]*)*$");
    importfile.setMinimal(true);
    bool is_importfile = importfile.exactMatch(currentLine);
    QRegExp fromimport("^[\\s]*from[\\s]*$");
    fromimport.setMinimal(true);
    bool is_fromimport = fromimport.exactMatch(currentLine);
    if ( is_importfile || is_fromimport ) {
        kDebug() << "Autocompletion type: import completion";
        m_operation = PythonCodeCompletionContext::ImportFileCompletion;
        return;
    }
    
    QRegExp replaceStrings("(\".*\"|\'.*\'|\"\"\".*\"\"\"|\'\'\'.*\'\'\')");
    replaceStrings.setMinimal(true);
    QString strippedLine = currentLine.replace(replaceStrings, "\"S\""); // we don't need string contents, cut them out
    
    bool is_attributeAccess = scanExpressionBackwards(strippedLine, QStringList(), QStringList() << ".", QStringList() << ".", QStringList());
    if ( is_attributeAccess ) {
        m_operation = PythonCodeCompletionContext::MemberAccessCompletion;
        return;
    }
    
    if ( context->type() == DUContext::Class ) {
        QRegExp defcompletion("(.*)\n([\\s]*)(def)[\\s]*[\\D]*$");
        defcompletion.setMinimal(true);
        bool is_defcompletion = defcompletion.exactMatch(currentLine);
        if ( is_defcompletion ) {
            m_indent = defcompletion.capturedTexts().at(2);
            m_operation = PythonCodeCompletionContext::DefineCompletion;
            return;
        }
    }
    
    QRegExp inheritanceCompletion("(.*)\n[\\s]*class[\\s]*(.*)[\\s]*\\([\\s]*$");
    inheritanceCompletion.setMinimal(true);
    bool is_inheritance = inheritanceCompletion.exactMatch(currentLine);
    if ( is_inheritance ) {
        m_operation = PythonCodeCompletionContext::InheritanceCompletion;
        return;
    }
    
    kDebug() << "Scanning for function call";
    bool is_FunctionCall = scanExpressionBackwards(strippedLine, QStringList(), QStringList() << "." << ",", QStringList() << "," << "(", QStringList());
    if ( is_FunctionCall ) {
        m_operation = PythonCodeCompletionContext::FunctionCallCompletion;
        m_alreadyGivenParametersCount = m_guessTypeOfExpression.count(','); // strings are already replaced by "S", so no risk to count commas
        
        scanExpressionBackwards(m_remainingExpression, QStringList(), QStringList() << "." << ",", QStringList(), QStringList() << "("); // get the next item in a chain of calls
                // for "a(b(c(), d, e" (we want autocompletion for b) the first call will give us "a(b" and "c(), d, e", but we want "b". so we call it again on the first result.
        kDebug() << "Found function call completion item, called function is " << m_guessTypeOfExpression << ", currently at parameter: " << m_alreadyGivenParametersCount;
        return;
    }
    
    QRegExp nocompletion("(.*)\n[\\s]*(class|def)[\\s]*$");
    nocompletion.setMinimal(true);
    bool is_nocompletion = nocompletion.exactMatch(currentLine);
    if ( is_nocompletion ) {
        m_operation = PythonCodeCompletionContext::NoCompletion;
        return;
    }
    
    QRegExp couldBeGeneratorCompletion("(.*)[\\[\\{](.*)[\\s]*for[\\s]*$");
    couldBeGeneratorCompletion.setMinimal(true);
    bool is_couldBeGeneratorCompletion = couldBeGeneratorCompletion.exactMatch(currentLine);
    if ( is_couldBeGeneratorCompletion ) {
        bool is_generatorCompletion = scanExpressionBackwards(strippedLine, QStringList(), QStringList(), QStringList(), QStringList() << "{" << "[" << "(", true);
        if ( is_generatorCompletion ) {
            kDebug() << "remaining expression for GeneratorVariableCompletion: " << m_remainingExpression;
            m_remainingExpression = strippedLine.remove(0, m_remainingExpression.length());
            if ( m_remainingExpression.length() >= 3 ) {
                m_remainingExpression = m_remainingExpression.trimmed();
                m_remainingExpression.remove(m_remainingExpression.length()-3, 3);
            }
            m_remainingExpression = '{' + m_remainingExpression + '}';
            kDebug() << "use unknown names in: " << m_remainingExpression;
            m_operation = PythonCodeCompletionContext::GeneratorVariableCompletion;
            return;
        }
    }
}

bool PythonCodeCompletionContext::scanExpressionBackwards(QString line, QStringList stopTokens, QStringList stopAtSpaceWithout, QStringList mustEndWithToken, QStringList ignoreAtEnd, bool ignoreWhitespace)
{
    int i = line.length() - 1;
    bool success = false;
    bool atEnd = true;
    bool previousWasSpace = false;
    QChar c;
    QChar colon('.'); QChar lbrace('('); QChar rbrace(')'); QChar lbracket('['); QChar rbracket(']'); QChar ldic('{'); QChar rdic('}');
    QList<QChar> openingBrackets; QList<QChar> closingBrackets;
    openingBrackets << lbrace << lbracket << ldic;
    closingBrackets << rbrace << rbracket << rdic;
    QStack<QChar> searchingForMatching;
    QString scanned = "";
    while ( i > 0 ) {
        c = line.at(i);
        if ( atEnd && ignoreAtEnd.contains(c) ) {
            i -= 1;
            continue;
        }
        scanned.insert(0, c);
        if ( atEnd ) previousWasSpace = false; // ignore trailing spaces
        if ( atEnd && ! c.isSpace() ) {
            if ( ! mustEndWithToken.count() || mustEndWithToken.contains(c) ) success = true;
            else success = false;
            atEnd = false;
        }
        if ( ! searchingForMatching.isEmpty() && c == searchingForMatching.top() ) {
            kDebug() << "Found matching " << c << "token";
            searchingForMatching.pop();
        }
        else if ( closingBrackets.contains(c) ) {
            kDebug() << "Searching for opening " << c << "token";
            searchingForMatching.push(openingBrackets.at(closingBrackets.indexOf(c)));
        }
        else if ( ! searchingForMatching.isEmpty() ) {
            // do nothing, this is in another expression, we don't care about it
        }
        else if ( openingBrackets.contains(c) ) {
            scanned[0] = ' ';
            break;
        }
        else if ( ! ignoreWhitespace && previousWasSpace && ! c.isSpace() && ! stopAtSpaceWithout.contains(c) ) {
            kDebug() << "Previous char was space, current is " << c << " -- break";
            scanned[0] = ' ';
            break;
        }
        else if ( stopTokens.contains(c) ) break;
        if ( c.isSpace() ) previousWasSpace = true;
        else previousWasSpace = false;
        i -= 1;
    }
    if ( success ) {
        m_guessTypeOfExpression = '\n' + scanned;
        // remove spaces at the beginning of the expression, they are treated as an INDENT token by the parser
        // and are thus a syntax error; also remove trailing dot
        m_guessTypeOfExpression.replace(QRegExp("\n[\\s]*"), "").replace(QRegExp("\\.[\\s]*$"), "");
        kDebug() << "Guess type of this expression: " << m_guessTypeOfExpression;
        m_remainingExpression = line.remove(i, line.length() - 1 - i);
        kDebug() << "Remaining expression: " << m_remainingExpression;
    }
    return success;
}

}
