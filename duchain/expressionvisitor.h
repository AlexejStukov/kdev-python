/*****************************************************************************
 * Copyright 2010 (c) Miquel Canes Gonzalez <miquelcanes@gmail.com>          *
 * Copyright 2011 Sven Brauch <svenbrauch@googlemail.com>                    *
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

#ifndef EXPRESSIONVISITOR_H
#define EXPRESSIONVISITOR_H

#include <QHash>
#include <KLocalizedString>

#include <language/duchain/types/abstracttype.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/typesystemdata.h>
#include <language/duchain/types/typeregister.h>
#include <language/duchain/duchainpointer.h>
#include <language/duchain/declaration.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/functiondeclaration.h>

#include "astdefaultvisitor.h"
#include "pythonduchainexport.h"
#include "pythoneditorintegrator.h"

#include "duchain/declarations/classdeclaration.h"
#include "duchain/declarations/functiondeclaration.h"
#include "types/variablelengthcontainer.h"

namespace KDevelop {
    class Identifier;
}

using namespace KDevelop;

namespace Python
{
    
typedef DUChainPointer<FunctionDeclaration> FunctionDeclarationPointer;
    
typedef KDevelop::IntegralTypeData IntegralTypeExtendedData;
class KDEVPYTHONDUCHAIN_EXPORT IntegralTypeExtended : public KDevelop::IntegralType {
public:
    typedef TypePtr<IntegralTypeExtended> Ptr;
    enum PythonIntegralTypes {
        TypeList = KDevelop::IntegralType::TypeLanguageSpecific,
        TypeDict = KDevelop::IntegralType::TypeLanguageSpecific + 1
    };
    IntegralTypeExtended(uint type = TypeNone) : IntegralType(createData<IntegralTypeExtended>()) {
        setDataType(type);
        setModifiers(ConstModifier);
    };
    IntegralTypeExtended(IntegralTypeExtendedData& data) : IntegralType(data) { }
    
    virtual QString toString() const {
        switch ( d_func()->m_dataType ) {
            case TypeList: return i18n("list");
            case TypeDict: return i18n("dictionary");
            default: break;
        }
        
        return KDevelop::IntegralType::toString();
    };
    
    enum {
        Identity = 60 // TODO ok?
    };
    
    typedef KDevelop::IntegralTypeData Data;
    typedef KDevelop::IntegralType BaseType;
protected:
    TYPE_DECLARE_DATA(IntegralTypeExtended);
};

/**
 * @brief The DUChain must be read-locked in order to run the visitor.
 **/

class KDEVPYTHONDUCHAIN_EXPORT ExpressionVisitor : public AstDefaultVisitor
{
    public:
        ExpressionVisitor(KDevelop::DUContext* ctx, PythonEditorIntegrator* editor = 0);
        // use this to construct the expression-visitor recursively
        ExpressionVisitor(ExpressionVisitor* parent);
        
        virtual void visitBinaryOperation(BinaryOperationAst* node);
        virtual void visitUnaryOperation(UnaryOperationAst* node);
        virtual void visitBooleanOperation(BooleanOperationAst* node);
        virtual void visitCompare(CompareAst* node);
        
        virtual void visitString(StringAst* node);
        virtual void visitNumber(NumberAst* node);
        virtual void visitName(NameAst* node);
        virtual void visitList(ListAst* node);
        virtual void visitDict(DictAst* node);
        virtual void visitSet(SetAst* node);
        virtual void visitSubscript(SubscriptAst* node);
        virtual void visitCall(CallAst* node);
        virtual void visitAttribute(AttributeAst* node);
        virtual void visitTuple(TupleAst* node);
        virtual void visitListComprehension(ListComprehensionAst* node);
        virtual void visitDictionaryComprehension(DictionaryComprehensionAst* node);
        virtual void visitSetComprehension(SetComprehensionAst* node);
        virtual void visitIfExpression(IfExpressionAst* node);
        
        void addUnknownName(const QString& name);
        
        // whether type of expression should be known or not, i.e. if at the point where the chain breaks the previous type
        // was already unknown, then this is an IDE error, otherwise probably the user's code is wrong; used for error reporting
        inline bool shouldBeKnown() const {
            return m_shouldBeKnown;
        };
        
        /**
         * @brief Resolve the given declaration if it is an alias declaration.
         *
         * @param decl the declaration to resolve
         * @return :Declaration* decl if not an alias declaration, decl->aliasedDeclaration().data otherwise
         * DUChain must be read locked
         **/
        Declaration* resolveAliasDeclaration(Declaration* decl);
        
        inline KDevelop::AbstractType::Ptr lastType() const {
            return ( m_lastType.isEmpty() ? AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed)) : m_lastType.last());
        }
        inline DeclarationPointer lastDeclaration() const {
            if ( m_lastDeclaration.isEmpty() ) {
                return DeclarationPointer(0);
            }
            return m_lastDeclaration.last().last();
        }
        inline QList<DeclarationPointer> lastDeclarations() const {
            if ( m_lastDeclaration.isEmpty() ) {
                return QList<DeclarationPointer>() << DeclarationPointer(0);
            }
            return m_lastDeclaration.last();
        }
        
        template<typename T> static TypePtr<T> typeObjectForIntegralType(QString typeDescriptor, DUContext* ctx);
        
        // used by autocompletion to disable range checks on declaration searches
        bool m_forceGlobalSearching;
        // used by autocompletion to detect unknown NameAst elements in expressions
        bool m_reportUnknownNames;
        CursorInRevision m_scanUntilCursor;
        QList<QString> m_unknownNames;
        
        // this tells the difference between "class foo" and "instance of foo" -- TODO need a better solution!
        bool m_isAlias;
    
    private:
        static QHash<KDevelop::Identifier, KDevelop::AbstractType::Ptr> s_defaultTypes;
        
        KDevelop::DUContext* m_ctx;
        PythonEditorIntegrator* m_editor;
        
        enum EncounterFlags {
            MergeTypes = 1,
            AutomaticallyDetermineDeclaration = 2
        };
        
        void encounter(AbstractType::Ptr type, EncounterFlags flags = (EncounterFlags) 0);
        AbstractType::Ptr encounterPreprocess(AbstractType::Ptr type, bool merge = false);
        template<typename T> void encounter(TypePtr<T> type, EncounterFlags flags = (EncounterFlags) 0);
        void encounterDeclaration(DeclarationPointer ptr, bool isAlias = false);
        void encounterDeclaration(Declaration* ptr, bool isAlias = false);
        void encounterDeclarations(QList<DeclarationPointer> ptrs, bool isAlias = false);
        
        void unknownTypeEncountered();
        AbstractType::Ptr unknownType();
        QList< TypePtr< StructureType > > possibleStructureTypes(AbstractType::Ptr type);
        QList< TypePtr< StructureType > > typeListForDeclarationList(QList< DeclarationPointer > decls);
        
        inline QList< DeclarationPointer > toSharedPtrList(QList< Declaration* > foundDecls) {
            QList<DeclarationPointer> result;
            foreach ( Declaration* d, foundDecls ) {
                result << DeclarationPointer(d);
            }
            return result;
        };
        
        bool m_shouldBeKnown;
        
        QStack<KDevelop::AbstractType::Ptr> m_lastType;
        QStack< QList<DeclarationPointer> > m_lastDeclaration;
        QStack<AbstractType::Ptr> m_callTypeStack;
        QStack<DeclarationPointer> m_callStack;
        
        ExpressionVisitor* m_parentVisitor;
        int m_depth;
};

}

#endif // EXPRESSIONVISITOR_H
