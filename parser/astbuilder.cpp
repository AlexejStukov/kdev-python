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

#include <QDebug>
#include <QStringList>

#include "pythonparser.h"
#include "ast.h"
#include <kdev-pg-token-stream.h>

namespace Python
{


template <typename T> T* safeNodeCast( Ast* node )
{
    T* ast = dynamic_cast<T*>(node);
    Q_ASSERT(ast);
    return ast;
}

template <typename T> QList<T*> generateSpecializedList( const QList<Ast*>& list )
{
    QList<T*> l;
    foreach( Ast* ast, list )
    {
        T* temp = safeNodeCast<T>( ast );
        l << temp;
    }
    return l;
}

IdentifierAst* AstBuilder::createIdentifier( Ast* parent, qint64 idx )
{
    IdentifierAst* ast = new IdentifierAst( parent );
    ast->start = parser->tokenStream->token( idx ).begin;
    ast->end = parser->tokenStream->token( idx ).end;
    parser->tokenStream->startPosition( idx, &ast->startLine, &ast->startCol );
    parser->tokenStream->endPosition( idx, &ast->endLine, &ast->endCol );
    ast->identifier = tokenText( idx );
    return ast;
}

QList<IdentifierAst*> AstBuilder::identifierListFromTokenList( Ast* parent, const KDevPG::ListNode<qint64>* sequence )
{
    QList<IdentifierAst*> identifiers;
    for( int i = 0; i < sequence->count(); i++ )
    {
        identifiers << createIdentifier( parent, sequence->at(i)->element );
    }
    return identifiers;
}

void AstBuilder::setStartEnd( Ast* ast, PythonParser::AstNode* node )
{
    ast->start = parser->tokenStream->token( node->startToken ).begin;
    ast->end = parser->tokenStream->token( node->endToken ).end;
    parser->tokenStream->startPosition( node->startToken, &ast->startLine, &ast->startCol );
    parser->tokenStream->endPosition( node->endToken, &ast->endLine, &ast->endCol );
}

QString AstBuilder::tokenText( qint64 tokenidx )
{
    // -1 means this is not a valid token idx and thus return an empty string;
    if( tokenidx == -1 )
        return "";
    KDevPG::TokenStream::Token token = parser->tokenStream->token( tokenidx );
    return parser->tokenText( token.begin, token.end );
}

AstBuilder::AstBuilder(PythonParser::Parser* p)
    : parser(p)
{
}

void AstBuilder::visitAndExpr(PythonParser::AndExprAst *node)
{
    qDebug() << "visitAndExpr start";
    visitNode( node->andExpr );
    if( node->anddShifExprSequence->count() > 0 )
    {
        BinaryExpressionAst* ast = createAst<BinaryExpressionAst>( node );
        ast->opType = ArithmeticExpressionAst::BinaryAnd;
        ast->lhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
        int count = node->anddShifExprSequence->count();
        BinaryExpressionAst* curast = ast;
        for( int i = 0; i < count; i++ )
        {
            visitNode( node->anddShifExprSequence->at(i)->element );
            if( i+1 < count )
            {
                BinaryExpressionAst* tmp = createAst<BinaryExpressionAst>( 
                        node->anddShifExprSequence->at(i)->element );
                curast->opType = ArithmeticExpressionAst::BinaryAnd;
                tmp->lhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
                curast->rhs = tmp;
                curast = tmp;
            }else
            {
                curast->rhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
            }
        }
        mNodeStack.push( ast );
    }
    qDebug() << "visitAndExpr end";
}

void AstBuilder::visitAndTest(PythonParser::AndTestAst *node)
{
    qDebug() << "visitAndTest start";
    visitNode( node->notTestSequence->at(0)->element );
    if( node->notTestSequence->count() > 1 )
    {
        BooleanAndOperationAst* ast = createAst<BooleanAndOperationAst>( node );
        ast->lhs = safeNodeCast<BooleanOperationAst>( mNodeStack.pop() );
        int count = node->notTestSequence->count();
        BooleanAndOperationAst* curast = ast;
        for( int i = 1; i < count; i++ )
        {
            visitNode( node->notTestSequence->at(i)->element );
            if( i+1 < count )
            {
                BooleanAndOperationAst* tmp = createAst<BooleanAndOperationAst>( 
                        node->notTestSequence->at(i)->element );
                tmp->lhs = safeNodeCast<BooleanOperationAst>( mNodeStack.pop() );
                curast->rhs = tmp;
                curast = tmp;
            }else
            {
                curast->rhs = safeNodeCast<BooleanOperationAst>( mNodeStack.pop() );
            }
        }
        mNodeStack.push( ast );
    }
    qDebug() << "visitAndTest end";
}

void AstBuilder::visitArglist(PythonParser::ArglistAst *node)
{
    qDebug() << "visitArglist start";
    QList<Ast*> args;
    visitNode( node->argListBegin );
    if( dynamic_cast<GeneratorAst*>( mNodeStack.top() ) )
    {
        // Early return because a Generator expression was found, thats the only
        // thing in this "argumentlist" then
        return;
    }
    if( node->argListBegin )
    {
        args += mListStack.pop();
    }
    if( node->arglistStar )
    {
        ArgumentAst* ast = createAst<ArgumentAst>( node->arglistStar );
        ast->argumentType = ArgumentAst::ListArgument;
        visitNode( node->arglistStar );
        ast->argumentExpression = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        args << ast;
    }
    if( node->arglistDoublestar )
    {
        ArgumentAst* ast = createAst<ArgumentAst>( node->arglistDoublestar );
        ast->argumentType = ArgumentAst::DictArgument;
        visitNode( node->arglistDoublestar );
        ast->argumentExpression = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        args << ast;
    }
    mListStack.push( args );
    qDebug() << "visitArglist end";
}

void AstBuilder::visitArgument(PythonParser::ArgumentAst *node)
{
    qDebug() << "visitArgument start";
    visitNode( node->argumentTest );
    if( node->argumentEqualTest )
    {
        ArgumentAst* ast = createAst<ArgumentAst>( node );
        ast->argumentType = ArgumentAst::KeywordArgument;
        ast->keywordName = safeNodeCast<IdentifierAst>( mNodeStack.pop() );
        visitNode( node->argumentEqualTest );
        ast->argumentExpression = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        mNodeStack.push( ast );
    }else if( node->genFor )
    {
        GeneratorAst* ast = createAst<GeneratorAst>( node );
        ast->generatedValue = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        visitNode( node->genFor );
        ast->generator = safeNodeCast<GeneratorForAst>( mNodeStack.pop() );
        mNodeStack.push( ast );
    }else
    {
        ArgumentAst* ast = createAst<ArgumentAst>( node );
        ast->argumentType = ArgumentAst::PositionalArgument;
        mNodeStack.push( ast );
    }
    qDebug() << "visitArgument end";
}

void AstBuilder::visitArithExpr(PythonParser::ArithExprAst *node)
{
    qDebug() << "visitArithExpr start";
    visitNode( node->arithTerm );
    if( node->arithOpListSequence->count() > 0 && node->arithTermListSequence->count() > 0 )
    {
        Q_ASSERT_X( node->arithOpListSequence->count() == node->arithTermListSequence->count(),
                    "visitArithExpr", "different number of operators and operands" );
        BinaryExpressionAst* ast = createAst<BinaryExpressionAst>( node );
        ast->lhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
        BinaryExpressionAst* cur = ast;
        int count = node->arithOpListSequence->count();
        for( int i = 0; i < count; i++ )
        {
            switch( node->arithOpListSequence->at(i)->element->arithOp )
            {
                case PythonParser::PlusOp:
                    cur->opType = BinaryExpressionAst::BinaryPlus;
                    break;
                case PythonParser::MinusOp:
                    cur->opType = BinaryExpressionAst::BinaryMinus;
                    break;
            }
            visitNode( node->arithTermListSequence->at(i)->element );
            if( i+1 < count )
            {
                BinaryExpressionAst* tmp = createAst<BinaryExpressionAst>( node->arithTermListSequence->at(i)->element );
                cur->rhs = tmp;
                cur = tmp;
                cur->lhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
            }else
            {
                cur->rhs = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
            }
        }
        mNodeStack.push( ast );
    }
    qDebug() << "visitArithExpr end";
}

void AstBuilder::visitAssertStmt(PythonParser::AssertStmtAst *node)
{
    qDebug() << "visitAssertStmt start";
    AssertAst* ast = createAst<AssertAst>( node );
    visitNode( node->assertNotTest );
    ast->assertTest = safeNodeCast<ExpressionAst>(mNodeStack.pop());
    if( node->assertRaiseTest )
    {
        visitNode( node->assertRaiseTest );
        ast->exceptionValue = safeNodeCast<ExpressionAst>(mNodeStack.pop());
    }
    mNodeStack.push(ast);
    qDebug() << "visitAssertStmt end";
}

void AstBuilder::visitAtom(PythonParser::AtomAst *node)
{
    qDebug() << "visitAtom start";
    AtomAst* ast = createAst<AtomAst>( node );
    if( node->atomIdentifierName >= 0 || node->number || node->stringliteralSequence->count() > 0 )
    {
        if( node->atomIdentifierName >= 0 )
        {
            IdentifierAst* id = createIdentifier( ast, node->atomIdentifierName );
            ast->identifier = id;
        }else if( node->number )
        {
            visitNode( node->number );
            ast->literal = safeNodeCast<LiteralAst>( mNodeStack.pop() );
        }else
        {
            LiteralAst* lit = createAst<LiteralAst>( node );
            lit->parent = ast;
            lit->value = createAst<IdentifierAst>( node );
            lit->value->parent = lit;
            for( int i = 0; i < node->stringliteralSequence->count(); i++ )
            {
                lit->value->identifier += tokenText( node->stringliteralSequence->at(i)->element );
            }
        }
    }else if( node->listmaker )
    {
        EnclosureAst* enc = createAst<EnclosureAst>( node->listmaker );
        enc->parent = ast;
        visitNode( node->listmaker );
        enc->encType = EnclosureAst::List;
        enc->list = safeNodeCast<ListAst>( mNodeStack.pop() );
        ast->enclosure = enc;
    }else if( node->codeexpr )
    {
        EnclosureAst* enc = createAst<EnclosureAst>( node->codeexpr );
        visitNode( node->codeexpr );
        enc->parent = ast;
        enc->encType = EnclosureAst::StringConversion;
        enc->stringConversion = generateSpecializedList<ExpressionAst>( mListStack.pop() );
        ast->enclosure = enc;
    }else if( node->dictmaker )
    {
        EnclosureAst* enc = createAst<EnclosureAst>( node->dictmaker );
        enc->parent = ast;
        visitNode( node->dictmaker );
        enc->encType = EnclosureAst::Dictionary;
        enc->dict = safeNodeCast<DictionaryAst>( mNodeStack.pop() );
        ast->enclosure = enc;
    }else if( node->yield )
    {
        visitNode( node->yield );
        EnclosureAst* enc = createAst<EnclosureAst>( node->yield );
        enc->parent = ast;
        enc->encType = EnclosureAst::Yield;
        enc->yield = safeNodeCast<YieldAst>( mNodeStack.pop() );
        ast->enclosure = enc;
    }else
    {
        EnclosureAst* enc;
        visitNode( node->testlist );
        if( node->genFor )
        {
            enc = createAst<EnclosureAst>( node );
            enc->encType = EnclosureAst::Generator;
            QList<ExpressionAst*> l = generateSpecializedList<ExpressionAst>( mListStack.pop() );
            GeneratorAst* gen = createAst<GeneratorAst>( node );
            gen->generatedValue = l.first();
            visitNode( node->genFor );
            gen->generator = safeNodeCast<GeneratorForAst>( mNodeStack.pop() );
            enc->generator = gen;
            enc->parent = ast;
            ast->enclosure = enc;
        }else
        {
            enc = createAst<EnclosureAst>( node );
            enc->encType = EnclosureAst::ParenthesizedForm;
            enc->parent = ast;
            enc->parenthesizedform = generateSpecializedList<ExpressionAst>( mListStack.pop() );
            ast->enclosure = enc;
        }
    }
    mNodeStack.push( ast );
    qDebug() << "visitAtom end";
}

void AstBuilder::visitBreakStmt(PythonParser::BreakStmtAst *node)
{
    qDebug() << "visitBreakStmt start";
    StatementAst* ast = createAst<StatementAst>( node, Ast::BreakAst );
    mNodeStack.push( ast );
    qDebug() << "visitBreakStmt end";
}

void AstBuilder::visitClassdef(PythonParser::ClassdefAst *node)
{
    qDebug() << "visitClassdef start";
    ClassDefinitionAst* ast = createAst<ClassDefinitionAst>( node );
    ast->className = createIdentifier( ast, node->className );
    visitNode( node->testlist );
    ast->inheritance = generateSpecializedList<ExpressionAst>( mListStack.pop() );
    visitNode( node->classSuite );
    ast->classBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    mNodeStack.push(ast);
    qDebug() << "visitClassdef end";
}

void AstBuilder::visitComparison(PythonParser::ComparisonAst *node)
{
    qDebug() << "visitComparison start";
    ComparisonAst* ast = createAst<ComparisonAst>( node );
    visitNode( node->compExpr );
    ast->firstComparator = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
    if( node->compOpSequence->count() > 0 && node->compOpExprSequence->count() > 0 )
    {
        Q_ASSERT( node->compOpSequence->count() == node->compOpExprSequence->count() );
        int count = node->compOpSequence->count();
        for( int i = 0; i < count; i++ )
        {
            QPair<ComparisonAst::ComparisonOperator, ArithmeticExpressionAst*> pair;
            switch( node->compOpSequence->at(i)->element->compOp )
            {
                case PythonParser::LessOp:
                    pair.first = ComparisonAst::LessThanOp;
                    break;
                case PythonParser::GreaterOp:
                    pair.first = ComparisonAst::GreaterThanOp;
                    break;
                case PythonParser::IsEqualOp:
                    pair.first = ComparisonAst::EqualOp;
                    break;
                case PythonParser::GreaterEqOp:
                    pair.first = ComparisonAst::GreaterEqualOp;
                    break;
                case PythonParser::LessEqOp:
                    pair.first = ComparisonAst::LessEqualOp;
                    break;
                case PythonParser::UnEqualOp:
                    pair.first = ComparisonAst::UnequalOp;
                    break;
                case PythonParser::InOp:
                    pair.first = ComparisonAst::InOp;
                    break;
                case PythonParser::NotInOp:
                    pair.first = ComparisonAst::NotInOp;
                    break;
                case PythonParser::IsNotOp:
                    pair.first = ComparisonAst::IsNotOp;
                    break;
                case PythonParser::IsOp:
                    pair.first = ComparisonAst::IsOp;
                    break;
            }
            visitNode( node->compOpExprSequence->at(i)->element );
            pair.second = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
            ast->comparatorList << pair;
        }
    }
    mNodeStack.push( ast );
    qDebug() << "visitComparison end";
}

void AstBuilder::visitCompoundStmt(PythonParser::CompoundStmtAst *node)
{
    qDebug() << "visitCompoundStmt start";
    PythonParser::DefaultVisitor::visitCompoundStmt( node );
    qDebug() << "visitCompoundStmt end";
}

void AstBuilder::visitContinueStmt(PythonParser::ContinueStmtAst *node)
{
    qDebug() << "visitContinueStmt start";
    StatementAst* ast = createAst<StatementAst>( node, Ast::ContinueAst );
    mNodeStack.push( ast );
    qDebug() << "visitContinueStmt end";
}

void AstBuilder::visitDecorator(PythonParser::DecoratorAst *node)
{
    qDebug() << "visitDecorator start";
    DecoratorAst* ast = createAst<DecoratorAst>( node );
    visitNode( node->decoratorName );
    ast->dottedName = generateSpecializedList<IdentifierAst>( mListStack.pop() );
    if( node->arguments )
    {
        visitNode( node->arguments );
        ast->arguments = generateSpecializedList<ArgumentAst>( mListStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitDecorator end";
}

void AstBuilder::visitDecorators(PythonParser::DecoratorsAst *node)
{
    qDebug() << "visitDecorators start";
    qDebug() << "visitDecorators end";
}

void AstBuilder::visitDefparam(PythonParser::DefparamAst *node)
{
    qDebug() << "visitFpdef start";
    qDebug() << "visitFpdef start";
}

void AstBuilder::visitDelStmt(PythonParser::DelStmtAst *node)
{
    qDebug() << "visitDelStmt start";
    DelAst* ast = createAst<DelAst>( node );
    visitNode( node->delList );
    ast->deleteObjects = generateSpecializedList<TargetAst>( mListStack.pop() );
    mNodeStack.push( ast );
    qDebug() << "visitDelStmt end";
}

void AstBuilder::visitDictmaker(PythonParser::DictmakerAst *node)
{
    qDebug() << "visitDictmaker start";
    qDebug() << "visitDictmaker end";
}

void AstBuilder::visitDottedAsName(PythonParser::DottedAsNameAst *node)
{
    qDebug() << "visitDottedAsName start";
    qDebug() << "visitDottedAsName end";
}

void AstBuilder::visitDottedAsNames(PythonParser::DottedAsNamesAst *node)
{
    qDebug() << "visitDottedAsNames start";
    qDebug() << "visitDottedAsNames end";
}

void AstBuilder::visitDottedName(PythonParser::DottedNameAst *node)
{
    qDebug() << "visitDottedName start";
    // This visit should never be reached as the dottedName members are always
    // evaluated directly where they are used, i.e. in the decorator and import visits
    Q_ASSERT( false );
    qDebug() << "visitDottedName end";
}

void AstBuilder::visitExceptClause(PythonParser::ExceptClauseAst *node)
{
    qDebug() << "visitExceptClause start";
    qDebug() << "visitExceptClause end";
}

void AstBuilder::visitExecStmt(PythonParser::ExecStmtAst *node)
{
    qDebug() << "visitExecStmt start";
    ExecAst* ast = createAst<ExecAst>( node );
    visitNode( node->execCode );
    ast->executable = safeNodeCast<ArithmeticExpressionAst>( mNodeStack.pop() );
    if( node->globalDictExec )
    {
        visitNode( node->globalDictExec );
        ast->globalsAndLocals = safeNodeCast<DictionaryAst>( mNodeStack.pop() );
    }
    if( node->localDictExec )
    {
        visitNode( node->localDictExec );
        ast->localsOnly = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitExecStmt end";
}

void AstBuilder::visitExpr(PythonParser::ExprAst *node)
{
    qDebug() << "visitExpr start";
    qDebug() << "visitExpr end";
}

void AstBuilder::visitExprStmt(PythonParser::ExprStmtAst *node)
{
    qDebug() << "visitExprStmt start";
    qDebug() << "visitExprStmt end";
}

void AstBuilder::visitExprlist(PythonParser::ExprlistAst *node)
{
    qDebug() << "visitExprlist start";
    qDebug() << "visitExprlist end";
}

void AstBuilder::visitFactOp(PythonParser::FactOpAst *node)
{
    qDebug() << "visitFactOp start";
    qDebug() << "visitFactOp end";
}

void AstBuilder::visitFactor(PythonParser::FactorAst *node)
{
    qDebug() << "visitFactor start";
    qDebug() << "visitFactor end";
}

void AstBuilder::visitFlowStmt(PythonParser::FlowStmtAst *node)
{
    qDebug() << "visitFlowStmt start";
    PythonParser::DefaultVisitor::visitFlowStmt( node );
    qDebug() << "visitFlowStmt end";
}

void AstBuilder::visitForStmt(PythonParser::ForStmtAst *node)
{
    qDebug() << "visitForStmt start";
    ForAst* ast = createAst<ForAst>( node );
    visitNode( node->forExpr );
    ast->assignedTargets = generateSpecializedList<TargetAst>( mListStack.pop() );
    visitNode( node->forTestlist );
    ast->iterable = generateSpecializedList<ExpressionAst>( mListStack.pop() );
    visitNode( node->forSuite );
    ast->forBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    if( node->forElseSuite )
    {
        visitNode( node->forElseSuite );
        ast->elseBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitForStmt end";
}

void AstBuilder::visitFpDef(PythonParser::FpDefAst *node)
{
    qDebug() << "visitFpDef start";
    qDebug() << "visitFpDef end";
}

void AstBuilder::visitFplist(PythonParser::FplistAst *node)
{
    qDebug() << "visitFplist start";
    qDebug() << "visitFplist end";
}

void AstBuilder::visitFunPosParam(PythonParser::FunPosParamAst *node)
{
    qDebug() << "visitFunPosParam start";
    qDebug() << "visitFunPosParam end";
}

void AstBuilder::visitFuncdecl(PythonParser::FuncdeclAst *node)
{
    qDebug() << "visitFuncdecl start";
    FunctionDefinitionAst* ast = createAst<FunctionDefinitionAst>( node );
    if( node->decorators )
    {
        visitNode( node->decorators );
        ast->decorators = generateSpecializedList<DecoratorAst>( mListStack.pop() );
    }
    ast->functionName = createIdentifier( ast,  node->funcName );
    if( node->funArgs )
    {
        visitNode( node->funArgs );
        ast->parameters = generateSpecializedList<ParameterAst>( mListStack.pop() );
    }
    visitNode( node->funSuite );
    ast->functionBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    mNodeStack.push( ast );
    qDebug() << "visitFuncdecl end";
}

void AstBuilder::visitFuncDef(PythonParser::FuncDefAst *node)
{
    qDebug() << "visitFuncDef start";
    qDebug() << "visitFuncDef end";
}

void AstBuilder::visitGenFor(PythonParser::GenForAst *node)
{
    qDebug() << "visitGenFor start";
    qDebug() << "visitGenFor end";
}

void AstBuilder::visitGenIf(PythonParser::GenIfAst *node)
{
    qDebug() << "visitGenIf start";
    qDebug() << "visitGenIf end";
}

void AstBuilder::visitGenIter(PythonParser::GenIterAst *node)
{
    qDebug() << "visitGenIter start";
    qDebug() << "visitGenIter end";
}

void AstBuilder::visitGlobalStmt(PythonParser::GlobalStmtAst *node)
{
    qDebug() << "visitGlobalStmt start";
    GlobalAst* ast = createAst<GlobalAst>( node );
    ast->identifiers = identifierListFromTokenList( ast, node->globalNameSequence );
    mNodeStack.push( ast );
    qDebug() << "visitGlobalStmt end";
}

void AstBuilder::visitIfStmt(PythonParser::IfStmtAst *node)
{
    qDebug() << "visitIfStmt start";
    IfAst* ast = createAst<IfAst>( node );
    visitNode( node->ifTest );
    ast->ifCondition = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    visitNode( node->ifSuite );
    ast->ifBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    Q_ASSERT( node->elifTestSequence->count() == node->elifSuiteSequence->count() );
    int count = node->elifTestSequence->count();
    for( int i = 0; i < count; i++)
    {
        visitNode( node->elifTestSequence->at(i)->element );
        ExpressionAst* expr = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        visitNode( node->elifSuiteSequence->at(i)->element );
        ast->elseIfBodies.append(
                                 qMakePair( expr ,
                                            generateSpecializedList<StatementAst>(
                                                    mListStack.pop() ) ) );
    }
    if( node->ifElseSuite )
    {
        visitNode( node->ifElseSuite );
        ast->elseBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitIfStmt end";
}

void AstBuilder::visitImportAsName(PythonParser::ImportAsNameAst *node)
{
    qDebug() << "visitImportAsName start";
    // This visit should never be reached as the dottedName members are always
    // evaluated directly where they are used, i.e. in the decorator and import visits
    Q_ASSERT( false );
    qDebug() << "visitImportAsName end";
}

void AstBuilder::visitImportAsNames(PythonParser::ImportAsNamesAst *node)
{
    qDebug() << "visitImportAsNames start";
    // This visit should never be reached as the dottedName members are always
    // evaluated directly where they are used, i.e. in the decorator and import visits
    Q_ASSERT( false );
    qDebug() << "visitImportAsNames end";
}

void AstBuilder::visitImportFrom(PythonParser::ImportFromAst *node)
{
    qDebug() << "visitImportFrom start";
    if( !node->importAsNames )
    {
        StarImportAst* ast = createAst<StarImportAst>( node );
        ast->modulePath = identifierListFromTokenList( ast, node->importFromName->dottedNameSequence );
        mNodeStack.push( ast );
    }else
    {
        FromImportAst* ast = createAst<FromImportAst>( node );
        ast->modulePath = identifierListFromTokenList( ast, node->importFromName->dottedNameSequence );
        const KDevPG::ListNode<PythonParser::ImportAsNameAst*>* idNames;
        idNames = node->importAsNames->importAsNameSequence;
        int count = idNames->count();
        for(int i = 0; i < count; i++)
        {
            PythonParser::ImportAsNameAst* namenode = idNames->at(i)->element;
            qDebug() << "Fetching from-as:" << tokenText( namenode->importedName );
            ast->identifierAsName[ createIdentifier( ast, namenode->importedName ) ] = createIdentifier( ast, namenode->importedAs );
        }
        mNodeStack.push( ast );
    }
    qDebug() << "visitImportFrom end";
}

void AstBuilder::visitImportName(PythonParser::ImportNameAst *node)
{
    qDebug() << "visitImportName start";
    PlainImportAst* ast = createAst<PlainImportAst>( node );
    const KDevPG::ListNode<PythonParser::DottedAsNameAst*>* importedmodules;
    importedmodules = node->importName->dottedAsNameSequence;
    int count = importedmodules->count();
    for( int i = 0; i < count ; i++ )
    {
        PythonParser::DottedAsNameAst* import = importedmodules->at(i)->element;
        QList<IdentifierAst*> modulepath = identifierListFromTokenList( ast, import->importDottedName->dottedNameSequence );
        ast->modulesAsName[ i ] = qMakePair( modulepath, createIdentifier( ast, import->importedAs ) );
    }
    mNodeStack.push( ast );
    qDebug() << "visitImportName end";
}

void AstBuilder::visitImportStmt(PythonParser::ImportStmtAst *node)
{
    qDebug() << "visitImportStmt start";
    PythonParser::DefaultVisitor::visitImportStmt( node );
    qDebug() << "visitImportStmt end";
}

void AstBuilder::visitLambdaDef(PythonParser::LambdaDefAst *node)
{
    qDebug() << "visitLambdaDef start";
    qDebug() << "visitLambdaDef end";
}

void AstBuilder::visitListFor(PythonParser::ListForAst *node)
{
    qDebug() << "visitListFor start";
    qDebug() << "visitListFor end";
}

void AstBuilder::visitListIf(PythonParser::ListIfAst *node)
{
    qDebug() << "visitListIf start";
    qDebug() << "visitListIf end";
}

void AstBuilder::visitListIter(PythonParser::ListIterAst *node)
{
    qDebug() << "visitListIter start";
    qDebug() << "visitListIter end";
}

void AstBuilder::visitListmaker(PythonParser::ListmakerAst *node)
{
    qDebug() << "visitFpDef start";
    qDebug() << "visitFpDef end";
}

void AstBuilder::visitListMakerTest(PythonParser::ListMakerTestAst *node)
{
    qDebug() << "visitListMakerTest start";
    qDebug() << "visitListMakerTest end";
}

void AstBuilder::visitNotTest(PythonParser::NotTestAst *node)
{
    qDebug() << "visitNotTest start";
    qDebug() << "visitNotTest end";
}

void AstBuilder::visitNumber(PythonParser::NumberAst *node)
{
    qDebug() << "visitNumber start";
    qDebug() << "visitNumber end";
}

void AstBuilder::visitPassStmt(PythonParser::PassStmtAst *node)
{
    qDebug() << "visitPassStmt start";
    StatementAst* ast = createAst<StatementAst>( node,  Ast::PassAst );
    mNodeStack.push( ast );
    qDebug() << "visitPassStmt end";
}

void AstBuilder::visitPower(PythonParser::PowerAst *node)
{
    qDebug() << "visitPower start";
    qDebug() << "visitPower end";
}

void AstBuilder::visitPlainArgumentsList(PythonParser::PlainArgumentsListAst *node)
{
    qDebug() << "visitPlainArgumentsList start";
    QList<Ast*> l;
    int count = node->argumentsSequence->count();
    for( int i = 0; i < count; i++ )
    {
        visitNode( node->argumentsSequence->at(i)->element );
        if( dynamic_cast<ArgumentAst*>( mNodeStack.top() ) )
        {
            l << safeNodeCast<ArgumentAst>( mNodeStack.pop() );
        }else if( dynamic_cast<GeneratorAst*>( mNodeStack.top() ) )
        {
            //Early return, we found a generator expression on the stack
            return;
        }
    }
    mListStack.push( l );
    qDebug() << "visitPlainArgumentsList end";
}

void AstBuilder::visitPrintStmt(PythonParser::PrintStmtAst *node)
{
    qDebug() << "visitPrintStmt start";
    PrintAst* ast = createAst<PrintAst>( node );
    if( node->printArgsSequence->count() > 0 )
    {
        int count = node->printArgsSequence->count();
        for( int i = 0; i < count; i++ )
        {
            visitNode( node->printArgsSequence->at(i)->element );
            ast->printables.append( safeNodeCast<ExpressionAst>( mNodeStack.pop() ) );
        }
    }else
    {
        visitNode( node->rshiftArgsSequence->at(0)->element );
        ast->outfile = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
        int count = node->rshiftArgsSequence->count();
        for( int i = 1; i < count; i++ )
        {
            visitNode( node->printArgsSequence->at(i)->element );
            ast->printables.append( safeNodeCast<ExpressionAst>( mNodeStack.pop() ) );
        }
    }
    mNodeStack.push( ast );
    qDebug() << "visitPrintStmt end";
}

void AstBuilder::visitProject(PythonParser::ProjectAst *node)
{
    qDebug() << "visitProject start";
    CodeAst* code = new CodeAst();
    setStartEnd( code, node );
    mNodeStack.push( code );
    mListStack.push( QList<Ast*>() );
    PythonParser::DefaultVisitor::visitProject( node );
    code->statements = generateSpecializedList<StatementAst>( mListStack.pop() );
    qDebug() << "visitProject end";
}

void AstBuilder::visitRaiseStmt(PythonParser::RaiseStmtAst *node)
{
    qDebug() << "visitRaiseStmt start";
    RaiseAst* ast = createAst<RaiseAst>( node );
    if( node->type )
    {
        visitNode( node->type );
        ast->exceptionType = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    }
    if( node->value )
    {
        visitNode( node->value );
        ast->exceptionValue = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    }
    if( node->traceback )
    {
        visitNode( node->traceback );
        ast->traceback = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitRaiseStmt end";
}

void AstBuilder::visitReturnStmt(PythonParser::ReturnStmtAst *node)
{
    qDebug() << "visitReturnStmt start";
    ReturnAst* ast = createAst<ReturnAst>( node );
    visitNode( node->returnExpr );
    ast->returnValues = generateSpecializedList<ExpressionAst>( mListStack.pop() );
    mNodeStack.push( ast );
    qDebug() << "visitReturnStmt end";
}

void AstBuilder::visitShiftExpr(PythonParser::ShiftExprAst *node)
{
    qDebug() << "visitShiftExpr start";
    qDebug() << "visitShiftExpr end";
}

void AstBuilder::visitShiftOp(PythonParser::ShiftOpAst *node)
{
    qDebug() << "visitShiftOp start";
    qDebug() << "visitShiftOp end";
}

void AstBuilder::visitSimpleStmt(PythonParser::SimpleStmtAst *node)
{
    qDebug() << "visitSimpleStmt start";
    PythonParser::DefaultVisitor::visitSimpleStmt( node );
    qDebug() << "visitSimpleStmt end";
}

void AstBuilder::visitSliceop(PythonParser::SliceopAst *node)
{
    qDebug() << "visitSliceop start";
    qDebug() << "visitSliceop end";
}

void AstBuilder::visitSmallStmt(PythonParser::SmallStmtAst *node)
{
    qDebug() << "visitSmallStmt start";
    PythonParser::DefaultVisitor::visitSmallStmt( node );
    qDebug() << "visitSmallStmt end";
}

void AstBuilder::visitStmt(PythonParser::StmtAst *node)
{
    qDebug() << "visitStmt start";
    if( node->simpleStmt || node->compoundStmt )
    {
        PythonParser::DefaultVisitor::visitStmt( node );
        StatementAst* ast = safeNodeCast<StatementAst>(mNodeStack.pop());
        QList<Ast*>& stmtlist = mListStack.top();
        stmtlist << ast;
    }else
    {
        qDebug() << "Found linebreak";
    }
    qDebug() << "visitStmt end";
}

void AstBuilder::visitSubscript(PythonParser::SubscriptAst *node)
{
    qDebug() << "visitSubscript start";
    qDebug() << "visitSubscript end";
}

void AstBuilder::visitSubscriptlist(PythonParser::SubscriptlistAst *node)
{
    qDebug() << "visitSubscriptlist start";
    qDebug() << "visitSubscriptlist end";
}

void AstBuilder::visitSuite(PythonParser::SuiteAst *node)
{
    qDebug() << "visitSuite start";
    PythonParser::DefaultVisitor::visitSuite( node );
    qDebug() << "visitSuite end";
}

void AstBuilder::visitTerm(PythonParser::TermAst *node)
{
    qDebug() << "visitTerm start";
    qDebug() << "visitTerm end";
}

void AstBuilder::visitTermOp(PythonParser::TermOpAst *node)
{
    qDebug() << "visitTermOp start";
    qDebug() << "visitTermOp end";
}

void AstBuilder::visitTest(PythonParser::TestAst *node)
{
    qDebug() << "visitTest start";
    qDebug() << "visitTest end";
}

void AstBuilder::visitTestlist(PythonParser::TestlistAst *node)
{
    qDebug() << "visitTestlist start";
    qDebug() << "visitTestlist end";
}

void AstBuilder::visitCodeexpr(PythonParser::CodeexprAst *node)
{
    qDebug() << "visitCodeexpr start";
    qDebug() << "visitCodeexpr end";
}

void AstBuilder::visitTestlistSafe(PythonParser::TestlistSafeAst *node)
{
    qDebug() << "visitTestlistSafe start";
    qDebug() << "visitTestlistSafe end";
}

void AstBuilder::visitTrailer(PythonParser::TrailerAst *node)
{
    qDebug() << "visitTrailer start";
    qDebug() << "visitTrailer end";
}

void AstBuilder::visitTryStmt(PythonParser::TryStmtAst *node)
{
    qDebug() << "visitTryStmt start";
    TryAst* ast = createAst<TryAst>( node );
    visitNode( node->trySuite );
    ast->tryBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    if( node->finallySuite )
    {
        ast->finallyBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    }else
    {
        int count = node->exceptClauseSequence->count();
        for( int i = 1; i < count; i++ )
        {
            visitNode( node->exceptClauseSequence->at(i)->element );
            ExceptAst* ex = safeNodeCast<ExceptAst>( mNodeStack.pop() );
            visitNode( node->exceptSuiteSequence->at(i)->element );
            ex->exceptionBody = generateSpecializedList<StatementAst>( mListStack.pop() );
            ast->exceptions.append( ex );
        }
        if( node->tryElseSuite )
        {
            visitNode( node->tryElseSuite );
            ast->elseBody = generateSpecializedList<StatementAst>( mListStack.pop() );
        }
    }
    mNodeStack.push( ast );
    qDebug() << "visitTryStmt end";
}

void AstBuilder::visitVarargslist(PythonParser::VarargslistAst *node)
{
    qDebug() << "visitVarargslist start";
    qDebug() << "visitVarargslist end";
}

void AstBuilder::visitWhileStmt(PythonParser::WhileStmtAst *node)
{
    qDebug() << "visitWhileStmt start";
    WhileAst* ast = createAst<WhileAst>( node );
    visitNode( node->whileTest );
    ast->condition = safeNodeCast<ExpressionAst>( mNodeStack.pop() );
    visitNode( node->whileSuite );
    ast->whileBody = generateSpecializedList<StatementAst>( mListStack.pop() );;
    if( node->whileElseSuite )
    {
        visitNode( node->whileElseSuite );
        ast->elseBody = generateSpecializedList<StatementAst>( mListStack.pop() );
    }
    mNodeStack.push( ast );
    qDebug() << "visitWhileStmt end";
}

void AstBuilder::visitXorExpr(PythonParser::XorExprAst *node)
{
    qDebug() << "visitXorExpr start";
    qDebug() << "visitXorExpr end";
}

void AstBuilder::visitYieldExpr( PythonParser::YieldExprAst * node )
{
    qDebug() << "visitYieldExpr start";
    YieldAst* ast = createAst<YieldAst>( node );
    visitNode( node->expr );
    ast->yieldValue = generateSpecializedList<ExpressionAst>( mListStack.pop() );
    mNodeStack.push( ast );
    qDebug() << "visitYieldExpr end";
}

void AstBuilder::visitYieldStmt(PythonParser::YieldStmtAst *node)
{
    qDebug() << "visitYieldStmt start";
    visitNode( node->yield );
    qDebug() << "visitYieldStmt end";
}

CodeAst* AstBuilder::codeAst()
{
    return safeNodeCast<CodeAst>( mNodeStack.top() );
}


}
