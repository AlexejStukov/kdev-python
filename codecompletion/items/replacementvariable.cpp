/*****************************************************************************
 * Copyright (c) 2013 Atanas Gospodinov <atanas.r.gospodinov@gmail.com>      *
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

#include "replacementvariable.h"

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KTextEditor/CodeCompletionModel>
#include <KTextEditor/TemplateInterface2>
#include <language/duchain/ducontext.h>
#include <language/codecompletion/codecompletionmodel.h>

using namespace KDevelop;
using namespace KTextEditor;

namespace Python {

ReplacementVariableItem::ReplacementVariableItem(const ReplacementVariable &variable, const QString &description, bool hasEditableFields, Range position)
    : m_variable(variable),
      m_description(description),
      m_hasEditableFields(hasEditableFields),
      m_position(position)
{
}

void ReplacementVariableItem::execute(Document *document, const Range &word)
{
    if ( ! m_position.isValid() ) {
        m_position = word;
    }

    if ( m_hasEditableFields ) {
        TemplateInterface2 *templateInterface = qobject_cast<TemplateInterface2 *>(document->activeView());
        if ( templateInterface ) {
            document->removeText(m_position);
            templateInterface->insertTemplateText(m_position.start(), m_variable.toString(), QMap<QString, QString>(), NULL);
        }
    }
    else {
        document->replaceText(m_position, m_variable.toString());
    }
}

QVariant ReplacementVariableItem::data(const QModelIndex &index, int role, const KDevelop::CodeCompletionModel *model) const
{
    switch (role) {
    case KDevelop::CodeCompletionModel::IsExpandable:
        return QVariant(false);
    case Qt::DisplayRole:
        if (index.column() == KTextEditor::CodeCompletionModel::Name) {
            return QVariant(m_variable.toString());
        }
        else if ( index.column() == KTextEditor::CodeCompletionModel::Prefix ) {
            return QVariant(m_description);
        }
        else {
            return QVariant("");
        }
        break;
    case KTextEditor::CodeCompletionModel::ItemSelected:
        return QVariant("");
    case KTextEditor::CodeCompletionModel::InheritanceDepth:
        return QVariant(0);
    case KDevelop::CodeCompletionModel::BestMatchesCount:
        return 5;
    case KDevelop::CodeCompletionModel::MatchQuality:
        return 0;
    default:
        //pass
        break;
    }

    return CompletionTreeItem::data(index, role, model);
}

}
