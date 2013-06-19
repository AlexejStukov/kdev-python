/*
    This file is part of kdev-python, the python language plugin for KDevelop
    Copyright (C) 2012  Sven Brauch <svenbrauch@googlemail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "variablecontroller.h"
#include "variable.h"
#include "debugsession.h"
#include "pdbframestackmodel.h"
#include <codehelpers.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/declaration.h>
#include <language/duchain/duchain.h>
#include <interfaces/ilanguagecontroller.h>

#include <debugger/variable/variablecollection.h>
#include <debugger/framestack/framestackmodel.h>
#include <interfaces/icore.h>
#include <QStack>
#include <KMessageBox>
#include <KLocalizedString>

using namespace KDevelop;

namespace Python {

VariableController::VariableController(IDebugSession* parent) : IVariableController(parent)
{

}

void VariableController::addWatch(KDevelop::Variable* variable)
{
    variableCollection()->watches()->add(variable->expression());
}

void VariableController::addWatchpoint(KDevelop::Variable* /*variable*/)
{
    kWarning() << "addWatchpoint requested (not implemented)";
}

void VariableController::handleEvent(IDebugSession::event_t event)
{
    if ( event == IDebugSession::thread_or_frame_changed ) {
        DebugSession* s = static_cast<DebugSession*>(session());
        PdbFrameStackModel* model = static_cast<PdbFrameStackModel*>(s->frameStackModel());
        int delta = model->currentFrame() - model->debuggerAtFrame();
        model->setDebuggerAtFrame(model->currentFrame());
        bool positive = delta > 0;
        kDebug() << "changing frame by" << delta;
        for ( int i = delta; i != 0; i += ( positive ? -1 : 1 ) ) {
            kDebug() << ( positive ? "up" : "down" ) << model->currentFrame() << model->debuggerAtFrame();
            s->addSimpleInternalCommand(positive ? "up" : "down");
        }
    }
    KDevelop::IVariableController::handleEvent(event);
}

KDevelop::Variable* VariableController::createVariable(KDevelop::TreeModel* model, KDevelop::TreeItem* parent, const QString& expression, const QString& display)
{
    return new Variable(model, parent, expression, display);
}

QString VariableController::expressionUnderCursor(KTextEditor::Document* doc, const KTextEditor::Cursor& cursor)
{
    QString prefix = "";
    DUChainReadLocker lock;
    if ( ! doc->isModified() ) {
        if ( TopDUContext* context = DUChain::self()->chainForDocument(doc->url()) ) {
            DUContext* contextAtCursor = context->findContextAt(CursorInRevision(cursor.line(), cursor.column()));
            if ( contextAtCursor and contextAtCursor->type() == DUContext::Class ) {
                if ( contextAtCursor->owner() and ! contextAtCursor->owner()->identifier().isEmpty() ) {
                    prefix = contextAtCursor->owner()->identifier().toString() + ".";
                }
            }
        }
    }
    else {
        kDebug() << "duchain unavailable for document" << doc->url() << "or document out of date";
    }
    
    TextDocumentLazyLineFetcher linefetcher(doc);
    return prefix + CodeHelpers::expressionUnderCursor(linefetcher, cursor);
}

void VariableController::localsUpdateReady(QByteArray rawData)
{
    QRegExp formatExtract("([a-zA-Z0-9_]+) \\=\\> (.*)");
    QList<QByteArray> data = rawData.split('\n');
    kDebug() << "locals update:" << data;
    
    int i = 0;
    QStringList vars;
    QMap<QString, QString> values;
    while ( i < data.length() ) {
        QByteArray d = data.at(i);
        if ( formatExtract.exactMatch(d) ) {
            QString key = formatExtract.capturedTexts().at(1);
            vars << key;
            values[key] = formatExtract.capturedTexts().at(2);
        }
        else kWarning() << "mismatch:" << d;
        i++;
    }
    QList<KDevelop::Variable*> variableObjects = KDevelop::ICore::self()->debugController()->variableCollection()
                                                 ->locals()->updateLocals(vars);
    for ( int i = 0; i < variableObjects.length(); i++ ) {
        KDevelop::Variable* v = variableObjects[i];
        v->setValue(values[v->expression()]);
        v->setHasMoreInitial(true);
    }
}

void VariableController::update()
{
    kDebug() << "update requested";
    DebugSession* d = static_cast<DebugSession*>(parent());
    if (autoUpdate() & UpdateWatches) {
        variableCollection()->watches()->reinstall();
    }

   if (autoUpdate() & UpdateLocals) {
        // TODO find a more elegant solution for this import!
        InternalPdbCommand* import = new InternalPdbCommand(0, 0, "import __kdevpython_debugger_utils\n");
        InternalPdbCommand* cmd = new InternalPdbCommand(this, "localsUpdateReady",
                                  "__kdevpython_debugger_utils.format_locals(__kdevpython_debugger_utils.__kdevpython_builtin_locals())\n");
        d->addCommand(import);
        d->addCommand(cmd);
   }
}

}

#include "variablecontroller.moc"
