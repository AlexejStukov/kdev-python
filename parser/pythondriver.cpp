/* Python Parser Test
 *
 * Copyright 2007 Andreas Pakulat <apaku@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include "pythondriver.h"
#include "python_parser.h"

#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QTextCodec>

namespace Python
{

Driver::Driver()
    : m_debug(false), m_pool(0), m_tokenstream(0)
{
}

bool Driver::readFile( const QString& filename, const char* codec )
{
    QFile f(filename);
    if( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        qDebug() << "Couldn't open project file:" << filename;
        return false;
    }
    QTextStream s(&f);
    if( codec )
        s.setCodec( QTextCodec::codecForName(codec) );
    m_content = s.readAll();
    return true;
}
void Driver::setContent( const QString& content )
{
    m_content = content;
}
void Driver::setDebug( bool debug )
{
    m_debug = debug;
}

bool Driver::parse( project_ast** ast )
{
    if(!m_tokenstream)
        m_tokenstream = new kdev_pg_token_stream();
    if(!m_pool)
        m_pool = new kdev_pg_memory_pool();

    parser pythonparser;
    pythonparser.set_token_stream(m_tokenstream);
    pythonparser.set_memory_pool(m_pool);
    pythonparser.setDebug( m_debug );

    pythonparser.tokenize(m_content);
    bool matched = pythonparser.parse_project(ast);
    if( matched )
    {
        qDebug() << "Sucessfully parsed";
//         if( m_debug )
//         {
//             DebugVisitor d(&pythonparser);
//             d.visit_project(ast);
//         }
//         *qmast = new ProjectAST();
//         BuildASTVisitor d( &qmakeparser, *qmast );
//         d.visit_project(ast);
//         kDebug(9024) << "Found" << (*qmast)->statements().count() << "Statements";
    }else
    {
        *ast = 0;
        pythonparser.yy_expected_symbol(ast_node::Kind_project, "project");
        qDebug() << "Couldn't parse content";
    }
    return matched;
}


void Driver::setTokenStream( kdev_pg_token_stream* ts )
{
    m_tokenstream = ts;
}

void Driver::setMemoryPool( kdev_pg_memory_pool* pool )
{
    m_pool = pool;
}

}

// kate: space-indent on; indent-width 4; tab-width: 4; replace-tabs on; auto-insert-doxygen on
