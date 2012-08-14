/*****************************************************************************
 * This file is part of KDevelop                                             *
 * Copyright 2010 (c) Miquel Canes Gonzalez <miquelcanes@gmail.com>          *
 * Copyright 2011 by Sven Brauch <svenbrauch@googlemail.com>                 *
 *                                                                           *
 * Permission is hereby granted, free of charge, to any person obtaining     *
 * a copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including       *
 * without limitation the rights to use, copy, modify, merge, publish,       *
 * distribute, sublicense, and/or sell copies of the Software, and to        *
 * permit persons to whom the Software is furnished to do so, subject to     *
 * the following conditions:                                                 *
 *                                                                           *
 * The above copyright notice and this permission notice shall be            *
 * included in all copies or substantial portions of the Software.           *
 *                                                                           *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,           *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF        *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                     *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE    *
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION    *
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION     *
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.           *
 *****************************************************************************/

#include <KLocalizedString>

#include <language/duchain/types/unsuretype.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/declaration.h>
#include <language/interfaces/iproblem.h>
#include <language/duchain/types/typeregister.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/typesystemdata.h>
#include <language/duchain/types/unsuretype.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/aliasdeclaration.h>

#include "pythonduchainexport.h"
#include "expressionvisitor.h"
#include "pythoneditorintegrator.h"
#include "types/variablelengthcontainer.h"
#include "types/indexedcontainer.h"
#include "helpers.h"
#include "declarations/functiondeclaration.h"

using namespace KDevelop;
using namespace Python;
using namespace KTextEditor;

namespace Python {

REGISTER_TYPE(IntegralTypeExtended);

QHash<KDevelop::Identifier, KDevelop::AbstractType::Ptr> ExpressionVisitor::s_defaultTypes;

AbstractType::Ptr ExpressionVisitor::encounterPreprocess(AbstractType::Ptr type, bool merge)
{
    type = Helper::resolveType(type);
    AbstractType::Ptr res;
    if ( merge and not m_lastType.isEmpty() ) {
        res = Helper::mergeTypes(m_lastType.pop(), type);
    }
    else {
        res = type;
    }
    return res;
}

void ExpressionVisitor::encounter(AbstractType::Ptr type, EncounterFlags flags)
{
//     if ( type )
//         kDebug() << "type encountered: " << type->toString();
//     else
//         kDebug() << "unknown type encountered";
    if ( flags & AutomaticallyDetermineDeclaration ) {
        StructureType::Ptr t = type.cast<StructureType>();
        if ( t ) {
            encounterDeclaration(t->declaration(m_ctx->topContext()));
        }
        else {
            encounterDeclaration(0);
        }
    }
    m_lastType.push(encounterPreprocess(type, flags & MergeTypes));
}

template<typename T> void ExpressionVisitor::encounter(TypePtr< T > type, EncounterFlags flags)
{
    encounter(AbstractType::Ptr::staticCast(type), flags);
}

void ExpressionVisitor::encounterDeclaration(DeclarationPointer ptr, bool isAlias)
{
    m_isAlias = isAlias;
    m_lastDeclaration.push(QList<DeclarationPointer>() << ptr);
}

void ExpressionVisitor::encounterDeclarations(QList< DeclarationPointer > ptrs, bool isAlias)
{
    m_isAlias = isAlias;
    m_lastDeclaration.push(ptrs);
}

void ExpressionVisitor::encounterDeclaration(Declaration* ptr, bool isAlias)
{
    m_isAlias = isAlias;
    m_lastDeclaration.push(QList<DeclarationPointer>() << DeclarationPointer(ptr));
}

ExpressionVisitor::ExpressionVisitor(DUContext* ctx, PythonEditorIntegrator* editor)
    : m_forceGlobalSearching(false)
    , m_reportUnknownNames(false)
    , m_scanUntilCursor(CursorInRevision::invalid())
    , m_isAlias(false)
    , m_ctx(ctx)
    , m_editor(editor)
    , m_shouldBeKnown(true)
    , m_parentVisitor(0)
{
    if ( s_defaultTypes.isEmpty() ) {
        s_defaultTypes.insert(KDevelop::Identifier("True"), AbstractType::Ptr(new IntegralType(IntegralType::TypeBoolean)));
        s_defaultTypes.insert(KDevelop::Identifier("False"), AbstractType::Ptr(new IntegralType(IntegralType::TypeBoolean)));
        s_defaultTypes.insert(KDevelop::Identifier("None"), AbstractType::Ptr(new IntegralType(IntegralType::TypeVoid)));
    }
    Q_ASSERT(m_ctx);
    Q_ASSERT(m_ctx->topContext());
}

ExpressionVisitor::ExpressionVisitor(ExpressionVisitor* parent)
    : m_forceGlobalSearching(parent->m_forceGlobalSearching)
    , m_reportUnknownNames(parent->m_reportUnknownNames)
    , m_scanUntilCursor(parent->m_scanUntilCursor)
    , m_isAlias(false)
    , m_ctx(parent->m_ctx)
    , m_editor(parent->m_editor)
    , m_shouldBeKnown(true)
    , m_parentVisitor(parent)
{

}

AbstractType::Ptr ExpressionVisitor::unknownType()
{
    return AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed));
}

void ExpressionVisitor::unknownTypeEncountered() {
    m_isAlias = false;
    encounterDeclaration(0);
    encounter(unknownType());
}

QList< TypePtr< StructureType > > ExpressionVisitor::possibleStructureTypes(AbstractType::Ptr type)
{
    QList< TypePtr< StructureType > > result;
    type = Helper::resolveType(type);
    if ( ! type ) {
        return result;
    }
    if ( type->whichType() == KDevelop::AbstractType::TypeUnsure ) {
        AbstractType::Ptr current;
        UnsureType::Ptr possible = type.cast<UnsureType>();
        int amount = possible->typesSize();
        for ( int i = 0; i < amount; i++ ) {
            StructureType::Ptr current = Helper::resolveType(possible->types()[i].abstractType()).cast<StructureType>();
            if ( current ) {
                result << current;
            }
        }
    }
    else {
        StructureType::Ptr c = type.cast<StructureType>();
        if ( c ) {
            result << c;
        }
    }
    return result;
}

QList< TypePtr< StructureType > > ExpressionVisitor::typeListForDeclarationList(QList< DeclarationPointer > decls)
{
    QList<StructureType::Ptr> result;
    DUChainReadLocker lock;
    foreach ( const DeclarationPointer& ptr, decls ) {
        result.append(possibleStructureTypes(ptr->abstractType()));
    }
    return result;
}

void ExpressionVisitor::visitAttribute(AttributeAst* node)
{
    Python::AstDefaultVisitor::visitAttribute(node);
    
    ExpressionAst* accessingAttributeOf = node->value;

    ExpressionVisitor v(this);
    v.visitNode(accessingAttributeOf);
    AbstractType::Ptr accessedType = v.lastType();
    QList<AbstractType::Ptr> accessingAttributeOfType;
    if ( accessedType && accessedType->whichType() == AbstractType::TypeUnsure ) {
        UnsureType::Ptr unsure = accessedType.cast<UnsureType>();
        int size = unsure->typesSize();
        DUChainReadLocker lock;
        for ( int i = 0; i < size; i++ ) {
            accessingAttributeOfType << unsure->types()[i].abstractType();
        }
    }
    else {
        accessingAttributeOfType << accessedType;
    }
    
    QList<Declaration*> foundDecls;
    
    // Step 1: Find all matching declarations which are made inside the type of which the accessed object is.
    // Like, for A.B.C where B is an instance of foo, when processing C, find all properties of foo which are called C.
    
    // maybe our attribute isn't a class at all, then that's an error by definition for now
    bool success = false;
    bool haveOneUsefulType = false;
    if ( ! accessingAttributeOfType.isEmpty() ) {
        foreach ( AbstractType::Ptr current, accessingAttributeOfType ) {
            if ( current && current->whichType() != AbstractType::TypeStructure ) {
                continue;
            }
            StructureType::Ptr currentStructure = current.cast<StructureType>();
            if ( Helper::isUsefulType(current) ) {
                haveOneUsefulType = true;
            }
            DUChainReadLocker lock;
            QList<DUContext*> searchContexts = Helper::internalContextsForClass(currentStructure, m_ctx->topContext());
            kDebug() << "Searching declarations in contexts: " << searchContexts;
            foreach ( DUContext* currentInternalContext, searchContexts ) {
                if ( currentInternalContext ) {
                    foundDecls.append(currentInternalContext->findDeclarations(QualifiedIdentifier(node->attribute->value),
                                                                               CursorInRevision::invalid(), AbstractType::Ptr(),
                                                                               0, DUContext::DontSearchInParent));
                    success = true;
                }
            }
        }
    }
    if ( ! success ) {
        foundDecls.clear();
    }
    if ( ! haveOneUsefulType ) {
        m_shouldBeKnown = false;
    }
    
    // Step 2: Construct the type of the declaration which was found.
    Declaration* d;
    if ( foundDecls.length() > 0 ) {
        d = DeclarationPointer(Helper::resolveAliasDeclaration(foundDecls.last())).data();
        if ( ! d ) {
            return unknownTypeEncountered();
        }
        bool isAlias =     dynamic_cast<AliasDeclaration*>(d) || d->isFunctionDeclaration()
                        || dynamic_cast<ClassDeclaration*>(d);
        kDebug() << dynamic_cast<AliasDeclaration*>(d) <<  d->isFunctionDeclaration() << dynamic_cast<ClassDeclaration*>(d);
        encounterDeclarations(toSharedPtrList(foundDecls), isAlias);
        encounter(foundDecls.last()->abstractType());
    }
    else {
        kDebug() << "No declaration found for attribute";
        return unknownTypeEncountered();
    }
    DUChainReadLocker lock;
    kDebug() << "Last encountered type: " << ( lastType().unsafeData() ? lastType()->toString() : "<none>" );
    kDebug() << "VisitAttribute end";
}

void ExpressionVisitor::visitCall(CallAst* node)
{
    KDEBUG_BLOCK
    kDebug();
    
    foreach ( ExpressionAst* c, node->arguments ) {
        Python::AstDefaultVisitor::visitNode(c);
    }
    AstDefaultVisitor::visitNode(node->keywordArguments);
    AstDefaultVisitor::visitNode(node->starArguments);
    
    ExpressionVisitor v(this);
    v.visitNode(node->function);
    kDebug() << "function being called: " << v.lastDeclaration();
    Declaration* actualDeclaration = 0;
    if ( ! v.m_isAlias ) {
        kDebug() << "non-function / structure type is being called";
    }
    else {
        actualDeclaration = v.lastDeclaration().data();
    }
    
    if ( not actualDeclaration ) {
//         kDebug() << "No declaration for " << functionName;
        m_shouldBeKnown = false;
        return unknownTypeEncountered();
    }
    else {
        DUChainReadLocker lock;
        actualDeclaration = Helper::resolveAliasDeclaration(actualDeclaration);
        ClassDeclaration* classDecl = dynamic_cast<ClassDeclaration*>(actualDeclaration);
        QPair<FunctionDeclarationPointer, bool> d = Helper::functionDeclarationForCalledDeclaration(
                                                    DeclarationPointer(actualDeclaration));
        FunctionDeclaration* funcDecl = d.first.data();
        bool isConstructor = d.second;
        
        if ( funcDecl && funcDecl->type<FunctionType>() ) {
            AbstractType::Ptr type;
            if ( isConstructor and classDecl ) {
                type = classDecl->abstractType();
                encounterDeclaration(classDecl);
            }
            else {
                type = funcDecl->type<FunctionType>()->returnType();
                encounterDeclaration(funcDecl);
            }
            
            bool decoratorFound = false;
            bool typeFound = false;
            if ( funcDecl->decoratorsSize() > 0 ) {
                kDebug() << "Got function declaration with decorators, checking for list content type...";
                if ( Helper::findDecoratorByName<FunctionDeclaration>(funcDecl, "getsType") ) {
                    decoratorFound = true;
                    kDebug() << "Found decorator";
                    if ( node->function->astType == Ast::AttributeAstType ) {
                        lock.unlock();
                        ExpressionVisitor baseTypeVisitor(this);
                        // when calling foo.bar[3].baz.iteritems(), find the type of "foo.bar[3].baz"
                        baseTypeVisitor.visitNode(static_cast<AttributeAst*>(node->function)->value);
                        lock.lock();
                        if ( VariableLengthContainer::Ptr t = baseTypeVisitor.lastType().cast<VariableLengthContainer>() ) {
                            kDebug() << "Found container, using type";
                            AbstractType::Ptr newType = t->contentType().abstractType();
                            return encounter(newType);
                        }
                    }
                }
                if ( Helper::findDecoratorByName<FunctionDeclaration>(funcDecl, "getsList")
                    or Helper::findDecoratorByName<FunctionDeclaration>(funcDecl, "getsListOfKeys")
                ) {
                    decoratorFound = true;
                    kDebug() << "Got getsList decorator, checking container";
                    if ( node->function->astType == Ast::AttributeAstType ) {
                        lock.unlock();
                        ExpressionVisitor baseTypeVisitor(this);
                        // when calling foo.bar[3].baz.iteritems(), find the type of "foo.bar[3].baz"
                        baseTypeVisitor.visitNode(static_cast<AttributeAst*>(node->function)->value);
                        lock.lock();
                        if ( VariableLengthContainer::Ptr t = baseTypeVisitor.lastType().cast<VariableLengthContainer>() ) {
                            kDebug() << "Got container:" << t->toString();
                            VariableLengthContainer::Ptr newType = typeObjectForIntegralType<VariableLengthContainer>("list", m_ctx);
                            AbstractType::Ptr contentType;
                            if ( Helper::findDecoratorByName<FunctionDeclaration>(funcDecl, "getsList") ) {
                                contentType = t->contentType().abstractType();
                            }
                            else {
                                contentType = t->keyType().abstractType();
                            }
                            newType->addContentType(contentType);
                            AbstractType::Ptr resultingType = newType.cast<AbstractType>();
                            return encounter(resultingType);
                        }
                    }
                }
                if ( const Decorator* d = Helper::findDecoratorByName<FunctionDeclaration>(
                                              funcDecl, "returnContentEqualsContentOf") )
                {
                    kDebug() << "Found argument dependent decorator, checking argument type";
                    decoratorFound = true;
                    int argNum = d->additionalInformation().str().toInt();
                    if ( node->arguments.length() > argNum ) {
                        ExpressionAst* relevantArgument = node->arguments.at(argNum);
                        lock.unlock();
                        ExpressionVisitor v(this);
                        v.visitNode(relevantArgument);
                        lock.lock();
                        if ( v.lastType() ) {
                            VariableLengthContainer* realTarget = 0;
                            if ( VariableLengthContainer* target = dynamic_cast<VariableLengthContainer*>(type.unsafeData()) ) {
                                realTarget = target;
                            }
                            if ( VariableLengthContainer* source = dynamic_cast<VariableLengthContainer*>(v.lastType().unsafeData()) ) {
                                if ( ! realTarget ) {
                                    // if the function does not force a return type, just copy the source (like for reversed())
                                    realTarget = source;
                                }
                                VariableLengthContainer* newType = static_cast<VariableLengthContainer*>(realTarget->clone());
                                Q_ASSERT(newType);
                                newType->addContentType(source->contentType().abstractType());
                                typeFound = true;
                                return encounter(AbstractType::Ptr(newType));
                            }
                            else if ( type ) {
                                return encounter(type);
                            }
                        }
                    }
                    else if ( type ) {
                        return encounter(type);
                    }
                }
                if ( decoratorFound and not typeFound ) {
                    return unknownTypeEncountered();
                }
            }
            
            // if none of the above decorator-finding methods worked, just use the ordinary return type.
            return encounter(type);
        }
        else if ( classDecl ) {
            encounter(classDecl->abstractType());
            encounterDeclaration(classDecl);
        }
        else {
            if ( actualDeclaration ) {
                kDebug() << "Declaraton " << actualDeclaration->toString() << " is not a class or function declaration";
            }
            return unknownTypeEncountered();
        }
    }
    kDebug() << "Done";
}

void ExpressionVisitor::visitSubscript(SubscriptAst* node)
{
    AstDefaultVisitor::visitNode(node->value);
    if ( node->slice && node->slice->astType != Ast::IndexAstType ) {
        DUChainReadLocker lock; // only for debug output
        kDebug() << "Found slice, will use ListType for assignment";
        kDebug() << "LAST DECLARATION:" << lastDeclaration();
        encounterDeclaration(0);
        encounter(lastType());
    }
    else if ( node->slice ) {
        DUChainReadLocker lock;
        kDebug() << "LAST TYPE for slice access:" << lastType() << ( lastType() ? lastType()->toString() : "<null>" );
        
        if ( IndexedContainer::Ptr indexed = lastType().cast<IndexedContainer>() ) {
            encounterDeclaration(0);
            if ( IndexAst* sliceIndexAst = static_cast<IndexAst*>(node->slice) ) {
                if ( sliceIndexAst->value && sliceIndexAst->value->astType == Ast::NumberAstType ) {
                    NumberAst* number = static_cast<NumberAst*>(sliceIndexAst->value);
                    int sliceIndex = number->value;
                    if ( sliceIndex < 0 && sliceIndex + indexed->typesCount() > 0 ) {
                        sliceIndex += indexed->typesCount();
                    }
                    if ( sliceIndex < indexed->typesCount() && sliceIndex >= 0 ) {
                        return encounter(indexed->typeAt(sliceIndex).abstractType());
                    }
                }
            }
            // the exact index is unknown, use unsure
            encounter(indexed->asUnsureType().cast<AbstractType>());
        }
        else if ( VariableLengthContainer::Ptr variableLength = lastType().cast<VariableLengthContainer>() ) {
            encounterDeclaration(0);
            encounter(variableLength->contentType().abstractType());
        }
    }
    else {
        return unknownTypeEncountered();
    }
}

template<typename T> TypePtr<T> ExpressionVisitor::typeObjectForIntegralType(QString typeDescriptor, DUContext* ctx)
{
    QList<Declaration*> decls = ctx->topContext()->findDeclarations(
        QualifiedIdentifier("__kdevpythondocumentation_builtin_" + typeDescriptor));
    Declaration* decl = decls.isEmpty() ? 0 : dynamic_cast<Declaration*>(decls.first());
    AbstractType::Ptr type = decl ? decl->abstractType() : AbstractType::Ptr(0);
    return type.cast<T>();
}

void ExpressionVisitor::visitList(ListAst* node)
{
    AstDefaultVisitor::visitList(node);
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("list", m_ctx);
    lock.unlock();
    ExpressionVisitor contentVisitor(this);
    if ( type ) {
        foreach ( ExpressionAst* content, node->elements ) {
            contentVisitor.visitNode(content);
            type->addContentType(contentVisitor.lastType());
        }
    }
    else {
        unknownTypeEncountered();
        kWarning() << " [ !!! ] did not get a typetrack container object when expecting one! Fix code / setup.";
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitDictionaryComprehension(DictionaryComprehensionAst* node)
{
    AstDefaultVisitor::visitDictionaryComprehension(node);
    kDebug() << "visiting dictionary comprehension";
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("dict", m_ctx);
    if ( type ) {
        DUContext* comprehensionContext = m_ctx->findContextAt(CursorInRevision(node->startLine, node->startCol + 1));
        lock.unlock();
        Q_ASSERT(comprehensionContext);
        ExpressionVisitor v(this);
        v.m_ctx = m_forceGlobalSearching ? m_ctx->topContext() : comprehensionContext;
        v.visitNode(node->value);
        if ( v.lastType() ) {
            type->addContentType(v.lastType());
        }
        ExpressionVisitor k(this);
        v.m_ctx = m_forceGlobalSearching ? m_ctx->topContext() : comprehensionContext;
        k.visitNode(node->key);
        if ( k.lastType() ) {
            type->addKeyType(k.lastType());
        }
    }
    else {
        return unknownTypeEncountered();
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitSetComprehension(SetComprehensionAst* node)
{
    kDebug() << "visiting set comprehension";
    Python::AstDefaultVisitor::visitSetComprehension(node);
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("set", m_ctx);
    if ( type ) {
        DUContext* comprehensionContext = m_ctx->findContextAt(CursorInRevision(node->startLine, node->startCol+1), true);
        lock.unlock();
        ExpressionVisitor v(this);
        v.m_ctx = m_forceGlobalSearching ? m_ctx->topContext() : comprehensionContext;
        v.visitNode(node->element);
        if ( v.lastType() ) {
            type->addContentType(v.lastType());
        }
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitListComprehension(ListComprehensionAst* node)
{
    kDebug() << "visiting list comprehension";
    AstDefaultVisitor::visitListComprehension(node);
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("list", m_ctx);
    if ( type ) {
        DUContext* comprehensionContext = m_ctx->findContextAt(CursorInRevision(node->startLine, node->startCol + 1), true);
        lock.unlock();
        ExpressionVisitor v(this);
        v.m_ctx = comprehensionContext;
        kDebug() << "Searching iterator variable in context: " << comprehensionContext->range() 
                 << "found at" << node->startLine << node->startCol;
        v.visitNode(node->element);
        if ( v.lastType() ) {
            type->addContentType(v.lastType());
        }
    }
    else {
        return unknownTypeEncountered();
    }
    {
        DUChainReadLocker lock; // debug
        kDebug() << "Got type for List Comprehension:" << type->toString();
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitTuple(TupleAst* node) {
    AstDefaultVisitor::visitTuple(node);
    DUChainReadLocker lock;
    IndexedContainer::Ptr type = typeObjectForIntegralType<IndexedContainer>("tuple", m_ctx);
    if ( type ) {
        foreach ( ExpressionAst* expr, node->elements ) {
            ExpressionVisitor v(this);
            v.visitNode(expr);
            type->addEntry(v.lastType());
        }
        encounter(type.cast<AbstractType>(), AutomaticallyDetermineDeclaration);
    }
    else {
        kWarning() << "tuple type object is not available";
        return unknownTypeEncountered();
    }
}

void ExpressionVisitor::visitIfExpression(IfExpressionAst* node)
{
    AstDefaultVisitor::visitIfExpression(node);
    if ( node->body and node->orelse ) {
        ExpressionVisitor v(this);
        v.visitNode(node->body);
        AbstractType::Ptr first = v.lastType();
        DeclarationPointer firstDecl = v.lastDeclaration();
        v.visitNode(node->orelse);
        AbstractType::Ptr second = v.lastType();
        DeclarationPointer secondDecl = v.lastDeclaration();
        encounterDeclarations(QList<DeclarationPointer>() << firstDecl << secondDecl);
        encounter(Helper::mergeTypes(first, second));
    }
}

void ExpressionVisitor::visitSet(SetAst* node)
{
    Python::AstDefaultVisitor::visitSet(node);
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("set", m_ctx);
    lock.unlock();
    ExpressionVisitor contentVisitor(this);
    if ( type ) {
        foreach ( ExpressionAst* content, node->elements ) {
            contentVisitor.visitNode(content);
            type->addContentType(contentVisitor.lastType());
        }
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitDict(DictAst* node)
{
    AstDefaultVisitor::visitDict(node);
    DUChainReadLocker lock;
    TypePtr<VariableLengthContainer> type = typeObjectForIntegralType<VariableLengthContainer>("dict", m_ctx);
    lock.unlock();
    ExpressionVisitor contentVisitor(this);
    ExpressionVisitor keyVisitor(this);
    if ( type ) {
        Q_ASSERT(type->hasKeyType());
        foreach ( ExpressionAst* content, node->values ) {
            contentVisitor.visitNode(content);
            type->addContentType(contentVisitor.lastType());
        }
        foreach ( ExpressionAst* key, node->keys ) {
            keyVisitor.visitNode(key);
            type->addKeyType(keyVisitor.lastType());
        }
    }
    encounter<VariableLengthContainer>(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitNumber(Python::NumberAst* number)
{
    AbstractType::Ptr type;
    DUChainReadLocker lock;
    if ( number->isInt ) {
        type = typeObjectForIntegralType<AbstractType>("int", m_ctx);
    }
    else {
        type = typeObjectForIntegralType<AbstractType>("float", m_ctx);
    }
    encounter(type, AutomaticallyDetermineDeclaration);
}

void ExpressionVisitor::visitString(Python::StringAst* )
{
    DUChainReadLocker lock;
    StructureType::Ptr type = typeObjectForIntegralType<StructureType>("string", m_ctx);
    encounter(type, AutomaticallyDetermineDeclaration);
}

RangeInRevision nodeRange(Python::Ast* node)
{
    kDebug() << node->endLine;
    return RangeInRevision(node->startLine, node->startCol, node->endLine,node->endCol);
}

void ExpressionVisitor::addUnknownName(const QString& name)
{
    if ( m_parentVisitor ) {
        m_parentVisitor->addUnknownName(name);
    }
    else {
        if ( ! m_unknownNames.contains(name) ) {
            m_unknownNames.append(name);
        }
    }
}

void ExpressionVisitor::visitName(Python::NameAst* node)
{
    // "True", "False", "None" etc.
    KDevelop::Identifier id(node->identifier->value);
    QHash < KDevelop::Identifier, AbstractType::Ptr >::const_iterator defId = s_defaultTypes.constFind(id);
    if ( defId != s_defaultTypes.constEnd() ) {
        return encounter(*defId);
    }
    
    kDebug() << "Finding declaration for" << node->identifier->value;
    RangeInRevision range;
    if ( m_scanUntilCursor.isValid() ) {
        range = RangeInRevision(CursorInRevision(0, 0), m_scanUntilCursor);
    }
    else if ( m_forceGlobalSearching ) {
        range = RangeInRevision::invalid();
    }
    else {
        range = RangeInRevision(0, 0, node->endLine, node->endCol);
    }
    DUChainReadLocker lock;
    Declaration* d = Helper::declarationForName(node, QualifiedIdentifier(node->identifier->value),
                                                range, DUContextPointer(m_ctx));
    
    if ( d ) {
        /** DEBUG **/
        kDebug() << "Found declaration: " << d->toString() << d
                 << d->abstractType() << dynamic_cast<VariableLengthContainer*>(d->abstractType().unsafeData());
        /** / DEBUG **/
        bool isAlias = dynamic_cast<AliasDeclaration*>(d) || d->isFunctionDeclaration() || dynamic_cast<ClassDeclaration*>(d);
        encounterDeclaration(d, isAlias);
        return encounter(d->abstractType());
    }
    else {
        kDebug() << "VistName type not found";
        if ( m_reportUnknownNames ) {
            addUnknownName(node->identifier->value);
        }
        return unknownTypeEncountered();
    }
}

void ExpressionVisitor::visitCompare(CompareAst* node)
{
    Python::AstDefaultVisitor::visitCompare(node);
    
    encounterDeclaration(0);
    encounter(AbstractType::Ptr(new IntegralType(IntegralType::TypeBoolean)));
}

void ExpressionVisitor::visitBinaryOperation(Python::BinaryOperationAst* node)
{
    QList<Ast::BooleanOperationTypes> booleanOperators;
    
    visitNode(node->lhs);
    KDevelop::AbstractType::Ptr leftType = lastType();
    
    visitNode(node->rhs);
    KDevelop::AbstractType::Ptr rightType = lastType();
    
    if ( leftType->indexed() != rightType->indexed() ) {
        return unknownTypeEncountered();
    }
    
    encounterDeclaration(0);
    encounter(leftType); // TODO this is wrong.
}

void ExpressionVisitor::visitUnaryOperation(Python::UnaryOperationAst* node)
{
    visitNode(node->operand);
    
    //FIXME: m_lastValue = m_lastValue;
}

void ExpressionVisitor::visitBooleanOperation(Python::BooleanOperationAst* node)
{
    foreach (ExpressionAst* expression, node->values) {
        visitNode(expression);
    }
    
    encounterDeclaration(0);
    encounter(AbstractType::Ptr(new IntegralType(IntegralType::TypeBoolean)));
}

}

