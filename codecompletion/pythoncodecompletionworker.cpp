#include "pythoncodecompletionworker.h"
#include "pythoncodecompletionmodel.h"
#include "pythoncodecompletioncontext.h"


namespace Python {

PythonCodeCompletionWorker::PythonCodeCompletionWorker(PythonCodeCompletionModel *parent)
    : KDevelop::CodeCompletionWorker(parent)
{

}

KDevelop::CodeCompletionContext* PythonCodeCompletionWorker::createCompletionContext(KDevelop::DUContextPointer context, const QString& contextText, const QString& followingText, const KDevelop::CursorInRevision& position) const
{
    PythonCodeCompletionContext* completionContext = new PythonCodeCompletionContext(context, contextText, position, 0);
    return completionContext;
}


}