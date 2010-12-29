/***************************************************************************
 *   This file is part of KDevelop                                         *
 *   Copyright 2007 Andreas Pakulat <apaku@gmx.de>                         *
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

#include "astbuilder.h"

#include <QStringList>

#include "ast.h"

#include <kdebug.h>
#include <QProcess>
#include <QDomDocument>
#include "kurl.h"
#include <klocale.h>
#include <qxmlstream.h>
#include <QXmlStreamReader>
#include <qdir.h>
#include <language/duchain/topducontext.h>
#include <language/interfaces/iproblem.h>
#include <language/duchain/duchain.h>

#include "parserConfig.h"
#include <language/duchain/duchainlock.h>

#include <python2.6/pyport.h>
#include <python2.6/pyconfig.h>
#include <python2.6/node.h>

#include <python2.6/Python.h>

#include <python2.6/Python-ast.h>
#include <python2.6/ast.h>

#include <python2.6/graminit.h>
#include <python2.6/grammar.h>
#include <python2.6/parsetok.h>

#include <python2.6/object.h>

using namespace KDevelop;

extern grammar _PyParser_Grammar;

// remove evil macros from headers which pollute the namespace (grr!)
#undef test
#undef decorators
#undef Attribute

namespace Python
{
    
/* This code is generated by conversiongenerator.py.
 * I do not recommend editing it.
 */
    
class PythonAstTransformer {
public:
    CodeAst* ast;
    void run(mod_ty syntaxtree) {
        ast = new CodeAst();
        nodeStack.push(ast);
        ast->body = visitNodeList<_expr, Ast>(syntaxtree->v.Module.body);
    }
private:
    QStack<Ast*> nodeStack;
    
    Ast* parent() {
        return nodeStack.top();
    }
    
    template<typename T, typename K> QList<K*> visitNodeList(asdl_seq* node) {
        QList<K*> nodelist;
        for ( int i=0; i < node->size; i++ ) {
            T* currentNode = reinterpret_cast<T*>(node->elements[i]);
            Q_ASSERT(currentNode);
            K* transformedNode = dynamic_cast<K*>(visitNode(currentNode));
            Q_ASSERT(transformedNode);
            nodelist.append(transformedNode);
        }
        return nodelist;
    }



    Ast* visitNode(_expr* node) {
        switch ( node->kind ) {
        case BoolOp_kind: {
                BooleanOperationAst* v = new BooleanOperationAst(parent());
                v->type = (ExpressionAst::BooleanOperationTypes) node->v.BoolOp.op;
                v->values = visitNodeList<_expr, ExpressionAst>(node->v.BoolOp.values);
                return v;
            }
        case BinOp_kind: {
                BinaryOperationAst* v = new BinaryOperationAst(parent());
                v->type = (ExpressionAst::OperatorTypes) node->v.BinOp.op;
                v->lhs = dynamic_cast<ExpressionAst*>(visitNode(node->v.BinOp.left));
                v->rhs = dynamic_cast<ExpressionAst*>(visitNode(node->v.BinOp.right));
                return v;
            }
        case UnaryOp_kind: {
                UnaryOperationAst* v = new UnaryOperationAst(parent());
                v->type = (ExpressionAst::UnaryOperatorTypes) node->v.UnaryOp.op;
                v->operand = dynamic_cast<ExpressionAst*>(visitNode(node->v.UnaryOp.operand));
                return v;
            }
        case Lambda_kind: {
                LambdaAst* v = new LambdaAst(parent());
                v->arguments = dynamic_cast<ArgumentsAst*>(visitNode(node->v.Lambda.args));
                v->body = dynamic_cast<ExpressionAst*>(visitNode(node->v.Lambda.body));
                return v;
            }
        case IfExp_kind: {
                IfExpressionAst* v = new IfExpressionAst(parent());
                v->condition = dynamic_cast<ExpressionAst*>(visitNode(node->v.IfExp.test));
                v->body = dynamic_cast<ExpressionAst*>(visitNode(node->v.IfExp.body));
                v->orelse = dynamic_cast<ExpressionAst*>(visitNode(node->v.IfExp.orelse));
                return v;
            }
        case Dict_kind: {
                DictAst* v = new DictAst(parent());
                v->keys = visitNodeList<_expr, ExpressionAst>(node->v.Dict.keys);
                v->values = visitNodeList<_expr, ExpressionAst>(node->v.Dict.values);
                return v;
            }
        case ListComp_kind: {
                ListComprehensionAst* v = new ListComprehensionAst(parent());
                v->element = dynamic_cast<ExpressionAst*>(visitNode(node->v.ListComp.elt));
                v->generators = visitNodeList<_comprehension, ComprehensionAst>(node->v.ListComp.generators);
                return v;
            }
        case GeneratorExp_kind: {
                GeneratorExpressionAst* v = new GeneratorExpressionAst(parent());
                v->element = dynamic_cast<ExpressionAst*>(visitNode(node->v.GeneratorExp.elt));
                v->generators = visitNodeList<_comprehension, ComprehensionAst>(node->v.GeneratorExp.generators);
                return v;
            }
        case Yield_kind: {
                YieldAst* v = new YieldAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Yield.value));
                return v;
            }
        case Compare_kind: {
                CompareAst* v = new CompareAst(parent());
                v->leftmostElement = dynamic_cast<ExpressionAst*>(visitNode(node->v.Compare.left));

                for ( int _i = 0; _i < node->v.Compare.ops->size; _i++ ) {
                    v->operators.append((ExpressionAst::ComparisonOperatorTypes) node->v.Compare.ops->elements[_i]);
                }

                v->comparands = visitNodeList<_expr, ExpressionAst>(node->v.Compare.comparators);
                return v;
            }
        case Call_kind: {
                CallAst* v = new CallAst(parent());
                v->function = dynamic_cast<ExpressionAst*>(visitNode(node->v.Call.func));
                v->arguments = visitNodeList<_expr, ExpressionAst>(node->v.Call.args);
                v->keywords = visitNodeList<_keyword, KeywordAst>(node->v.Call.keywords);
                v->keywordArguments = dynamic_cast<ExpressionAst*>(visitNode(node->v.Call.kwargs));
                v->starArguments = dynamic_cast<ExpressionAst*>(visitNode(node->v.Call.starargs));
                return v;
            }
        case Repr_kind: {
                ReprAst* v = new ReprAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Repr.value));
                return v;
            }
        case Num_kind: {
                NumberAst* v = new NumberAst(parent());
                return v;
            }
        case Str_kind: {
                StringAst* v = new StringAst(parent());
                return v;
            }
        case Attribute_kind: {
                AttributeAst* v = new AttributeAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Attribute.value));
                v->attribute = new Python::Identifier(PyString_AsString(PyObject_Str(node->v.Attribute.attr)));
                v->context = (ExpressionAst::Context) node->v.Attribute.ctx;
                return v;
            }
        case Subscript_kind: {
                SubscriptAst* v = new SubscriptAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Subscript.value));
                v->slice = dynamic_cast<SliceAst*>(visitNode(node->v.Subscript.slice));
                v->context = (ExpressionAst::Context) node->v.Subscript.ctx;
                return v;
            }
        case Name_kind: {
                NameAst* v = new NameAst(parent());
                v->identifier = new Python::Identifier(PyString_AsString(PyObject_Str(node->v.Name.id)));
                v->context = (ExpressionAst::Context) node->v.Name.ctx;
                return v;
            }
        case List_kind: {
                ListAst* v = new ListAst(parent());
                v->elements = visitNodeList<_expr, ExpressionAst>(node->v.List.elts);
                v->context = (ExpressionAst::Context) node->v.List.ctx;
                return v;
            }
        case Tuple_kind: {
                TupleAst* v = new TupleAst(parent());
                v->elements = visitNodeList<_expr, ExpressionAst>(node->v.Tuple.elts);
                v->context = (ExpressionAst::Context) node->v.Tuple.ctx;
                return v;
            }
        default:
            kWarning() << "Unsupported statement AST type: " << node->kind;
            Q_ASSERT(false);
        }
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_excepthandler* node) {
        switch ( node->kind ) {
        case ExceptHandler_kind: {
                ExceptionHandlerAst* v = new ExceptionHandlerAst(parent());
                v->type = dynamic_cast<ExpressionAst*>(visitNode(node->v.ExceptHandler.type));
                v->name = dynamic_cast<ExpressionAst*>(visitNode(node->v.ExceptHandler.name));
                v->body = visitNodeList<_stmt, Ast>(node->v.ExceptHandler.body);
                return v;
            }
        default:
            kWarning() << "Unsupported statement AST type: " << node->kind;
            Q_ASSERT(false);
        }
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_comprehension* node) {
                ComprehensionAst* v = new ComprehensionAst(parent());
            v->target = dynamic_cast<ExpressionAst*>(visitNode(node->target));
            v->iterator = dynamic_cast<ExpressionAst*>(visitNode(node->iter));
            v->conditions = visitNodeList<_expr, ExpressionAst>(node->ifs);
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_alias* node) {
                AliasAst* v = new AliasAst(parent());
            v->name = new Python::Identifier(PyString_AsString(PyObject_Str(node->name)));
            v->asName = new Python::Identifier(PyString_AsString(PyObject_Str(node->asname)));
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_stmt* node) {
        switch ( node->kind ) {
        case Expr_kind: {
                ExpressionAst* v = new ExpressionAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Expr.value));
                return v;
            }
        case FunctionDef_kind: {
                FunctionDefinitionAst* v = new FunctionDefinitionAst(parent());
                v->arguments = dynamic_cast<ArgumentsAst*>(visitNode(node->v.FunctionDef.args));
                v->body = visitNodeList<_stmt, Ast>(node->v.FunctionDef.body);
                v->decorators = visitNodeList<_expr, NameAst>(node->v.FunctionDef.decorator_list);
                v->name = new Python::Identifier(PyString_AsString(PyObject_Str(node->v.FunctionDef.name)));
                return v;
            }
        case ClassDef_kind: {
                ClassDefinitionAst* v = new ClassDefinitionAst(parent());
                v->baseClasses = visitNodeList<_expr, ExpressionAst>(node->v.ClassDef.bases);
                v->body = visitNodeList<_stmt, Ast>(node->v.ClassDef.body);
                v->decorators = visitNodeList<_expr, ExpressionAst>(node->v.ClassDef.decorator_list);
                v->name = new Python::Identifier(PyString_AsString(PyObject_Str(node->v.ClassDef.name)));
                return v;
            }
        case Return_kind: {
                ReturnAst* v = new ReturnAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Return.value));
                return v;
            }
        case Delete_kind: {
                DeleteAst* v = new DeleteAst(parent());
                v->targets = visitNodeList<_expr, ExpressionAst>(node->v.Delete.targets);
                return v;
            }
        case Assign_kind: {
                AssignmentAst* v = new AssignmentAst(parent());
                v->targets = visitNodeList<_expr, ExpressionAst>(node->v.Assign.targets);
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Assign.value));
                return v;
            }
        case AugAssign_kind: {
                AugmentedAssignmentAst* v = new AugmentedAssignmentAst(parent());
                v->target = dynamic_cast<ExpressionAst*>(visitNode(node->v.AugAssign.target));
                v->op = (ExpressionAst::OperatorTypes) node->v.AugAssign.op;
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.AugAssign.value));
                return v;
            }
        case Print_kind: {
                PrintAst* v = new PrintAst(parent());
                v->destination = dynamic_cast<ExpressionAst*>(visitNode(node->v.Print.dest));
                v->values = visitNodeList<_expr, ExpressionAst>(node->v.Print.values);
                v->newline = node->v.Print.nl;
                return v;
            }
        case For_kind: {
                ForAst* v = new ForAst(parent());
                v->target = dynamic_cast<ExpressionAst*>(visitNode(node->v.For.target));
                v->iterator = dynamic_cast<ExpressionAst*>(visitNode(node->v.For.iter));
                v->body = visitNodeList<_stmt, Ast>(node->v.For.body);
                v->orelse = visitNodeList<_stmt, Ast>(node->v.For.orelse);
                return v;
            }
        case While_kind: {
                WhileAst* v = new WhileAst(parent());
                v->condition = dynamic_cast<ExpressionAst*>(visitNode(node->v.While.test));
                v->body = visitNodeList<_stmt, Ast>(node->v.While.body);
                v->orelse = visitNodeList<_stmt, Ast>(node->v.While.orelse);
                return v;
            }
        case If_kind: {
                IfAst* v = new IfAst(parent());
                v->condition = dynamic_cast<ExpressionAst*>(visitNode(node->v.If.test));
                v->body = visitNodeList<_stmt, Ast>(node->v.If.body);
                v->orelse = visitNodeList<_stmt, Ast>(node->v.If.orelse);
                return v;
            }
        case With_kind: {
                WithAst* v = new WithAst(parent());
                v->contextExpression = dynamic_cast<ExpressionAst*>(visitNode(node->v.With.context_expr));
                v->optionalVars = dynamic_cast<ExpressionAst*>(visitNode(node->v.With.optional_vars));
                v->body = visitNodeList<_stmt, Ast>(node->v.With.body);
                return v;
            }
        case Raise_kind: {
                RaiseAst* v = new RaiseAst(parent());
                v->type = dynamic_cast<ExpressionAst*>(visitNode(node->v.Raise.type));
                return v;
            }
        case TryExcept_kind: {
                TryExceptAst* v = new TryExceptAst(parent());
                v->body = visitNodeList<_stmt, Ast>(node->v.TryExcept.body);
                v->orelse = visitNodeList<_stmt, Ast>(node->v.TryExcept.orelse);
                v->handlers = visitNodeList<_excepthandler, ExceptionHandlerAst>(node->v.TryExcept.handlers);
                return v;
            }
        case TryFinally_kind: {
                TryFinallyAst* v = new TryFinallyAst(parent());
                v->body = visitNodeList<_stmt, Ast>(node->v.TryFinally.body);
                v->finalbody = visitNodeList<_stmt, Ast>(node->v.TryFinally.finalbody);
                return v;
            }
        case Assert_kind: {
                AssertionAst* v = new AssertionAst(parent());
                v->condition = dynamic_cast<ExpressionAst*>(visitNode(node->v.Assert.test));
                v->message = dynamic_cast<ExpressionAst*>(visitNode(node->v.Assert.msg));
                return v;
            }
        case Import_kind: {
                ImportAst* v = new ImportAst(parent());
                v->names = visitNodeList<_alias, AliasAst>(node->v.Import.names);
                return v;
            }
        case ImportFrom_kind: {
                ImportFromAst* v = new ImportFromAst(parent());
                v->module = new Python::Identifier(PyString_AsString(PyObject_Str(node->v.ImportFrom.module)));
                v->names = visitNodeList<_alias, AliasAst>(node->v.ImportFrom.names);
                v->level = node->v.ImportFrom.level;
                return v;
            }
        case Exec_kind: {
                ExecAst* v = new ExecAst(parent());
                v->body = dynamic_cast<ExpressionAst*>(visitNode(node->v.Exec.body));
                v->globals = dynamic_cast<ExpressionAst*>(visitNode(node->v.Exec.globals));
                v->locals = dynamic_cast<ExpressionAst*>(visitNode(node->v.Exec.locals));
                return v;
            }
        case Global_kind: {
                GlobalAst* v = new GlobalAst(parent());

                for ( int _i = 0; _i < node->v.Global.names->size; _i++ ) {
                    Python::Identifier* id = new Python::Identifier(PyString_AsString(PyObject_Str(
                                    reinterpret_cast<PyObject*>(node->v.Global.names->elements[_i])
                            )));
                    v->names.append(id);
                }

                return v;
            }
        case Break_kind: {
                BreakAst* v = new BreakAst(parent());
                return v;
            }
        case Continue_kind: {
                ContinueAst* v = new ContinueAst(parent());
                return v;
            }
        case Pass_kind: {
                PassAst* v = new PassAst(parent());
                return v;
            }
        default:
            kWarning() << "Unsupported statement AST type: " << node->kind;
            Q_ASSERT(false);
        }
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_slice* node) {
        switch ( node->kind ) {
        case Slice_kind: {
                SliceAst* v = new SliceAst(parent());
                v->lower = dynamic_cast<ExpressionAst*>(visitNode(node->v.Slice.lower));
                v->upper = dynamic_cast<ExpressionAst*>(visitNode(node->v.Slice.upper));
                v->step = dynamic_cast<ExpressionAst*>(visitNode(node->v.Slice.step));
                return v;
            }
        case ExtSlice_kind: {
                ExtendedSliceAst* v = new ExtendedSliceAst(parent());
                v->dims = visitNodeList<_slice, SliceAst>(node->v.ExtSlice.dims);
                return v;
            }
        case Index_kind: {
                IndexAst* v = new IndexAst(parent());
                v->value = dynamic_cast<ExpressionAst*>(visitNode(node->v.Index.value));
                return v;
            }
        case Ellipsis_kind: {
                EllipsisAst* v = new EllipsisAst(parent());
                return v;
            }
        default:
            kWarning() << "Unsupported statement AST type: " << node->kind;
            Q_ASSERT(false);
        }
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_arguments* node) {
                ArgumentsAst* v = new ArgumentsAst(parent());
            v->arguments = visitNodeList<_expr, ExpressionAst>(node->args);
            v->defaultValues = visitNodeList<_expr, ExpressionAst>(node->defaults);
            v->vararg = new Python::Identifier(PyString_AsString(PyObject_Str(node->vararg)));
            v->kwarg = new Python::Identifier(PyString_AsString(PyObject_Str(node->kwarg)));
        return 0; // this will never be reached, but avoids gcc warnings
    }


    Ast* visitNode(_keyword* node) {
                KeywordAst* v = new KeywordAst(parent());
            v->argumentName = new Python::Identifier(PyString_AsString(PyObject_Str(node->arg)));
            v->value = dynamic_cast<ExpressionAst*>(visitNode(node->value));
        return 0; // this will never be reached, but avoids gcc warnings
    }

};

/*
 * End generated code
 */


CodeAst* AstBuilder::parse(KUrl filename, const QString& contents)
{
//     CodeAst* ast = parseXmlAst(getXmlForFile(filename, contents));
    const char* code = contents.toUtf8();
    
    PyArena* arena = PyArena_New();
    PyCompilerFlags* flags = new PyCompilerFlags();
    
    mod_ty syntaxtree = PyParser_ASTFromString(code, "<kdev-editor-contents>", file_input, flags, arena);
    
    Q_ASSERT(syntaxtree);
    kDebug() << syntaxtree->kind << Module_kind;
    kDebug() << reinterpret_cast<_stmt*>(syntaxtree->v.Module.body->elements[2])->kind;
    
    PythonAstTransformer* t = new PythonAstTransformer();
    t->run(syntaxtree);
    kDebug() << t->ast;
    
    return t->ast;
}

}

