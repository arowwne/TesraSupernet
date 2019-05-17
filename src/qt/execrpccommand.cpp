#include <qt/execrpccommand.h>
#include <qt/rpcconsole.h>
#include <QJsonDocument>
#include <univalue.h>
#include <rpcserver.h>
#include <rpcclient.h>
#include <QDebug>

ExecRPCCommand::ExecRPCCommand(const QString &command, const QStringList &mandatory, const QStringList &optional, const QMap<QString, QString>& translations, QObject *parent)
    : QObject(parent)
{
    m_command = command;
    m_mandatoryParams = mandatory;
    m_optionalParams = optional;
    m_translations = translations;
}

bool ExecRPCCommand::exec(const QMap<QString, QString> &params, QVariant &result, QString &resultJson, QString &errorMessage)
{
    QStringList commandLine;
    commandLine.append(m_command);

    
    QStringList mandatoryNotPresent;
    for(int i = 0; i < m_mandatoryParams.count(); i++)
    {
        QString key = m_mandatoryParams[i];
        if(!params.contains(key))
        {
            mandatoryNotPresent.append(m_translations[key]);
        }
        else
        {
            commandLine.append(params[key]);
        }
    }
    if(mandatoryNotPresent.count() > 0)
    {
        errorMessage = tr("Mandatory fields are not present:\n%1").arg(mandatoryNotPresent.join(", "));
        return false;
    }

    
    bool haveOptional = false;
    int optionalParamsAt = commandLine.size();
    QStringList optionalNotPresent;
    for(int i = m_optionalParams.count() - 1; i > -1; i--)
    {
        QString key = m_optionalParams[i];
        if(params.contains(key))
        {
            if(!haveOptional) haveOptional = true;
            commandLine.insert(optionalParamsAt, params[key]);
        }
        else
        {
            if(haveOptional)
            {
                optionalNotPresent.prepend(m_translations[key]);
            }
        }
    }
    if(optionalNotPresent.count() > 0)
    {
        errorMessage = tr("Optional fields are not present:\n%1").arg(optionalNotPresent.join(", "));
        return false;
    }

    

    std::vector<std::string> args;


    try
    {
        std::string strResult;
        std::string strCommand = commandLine.join(' ').toStdString();

        if (!RPCConsole::parseCommandLine(args, strCommand)) {
            return false;
        }
        if (args.empty())
            return false; 

        UniValue result2 = tableRPC.execute(
            args[0],
            RPCConvertValues(args[0], std::vector<std::string>(args.begin() + 1, args.end())));

        if (result2.isNull())
        {
            errorMessage = tr("Parse error: unbalanced2 ' or \"");
            strResult = "";
            resultJson = strResult.c_str();
        }
        else if (result2.isStr())
        {
            strResult = result2.get_str();
            resultJson = strResult.c_str();
            QJsonDocument doc = QJsonDocument::fromJson(strResult.c_str());
            result = doc.toVariant();
            return true;
        }
        else
        {

            strResult = result2.write(2);
            errorMessage = tr("Parse error: unbalanced3 ' or \"");
            resultJson = strResult.c_str();
            qDebug()<<"resultJson:"<<resultJson;
            return true;

        }
    }
    catch (UniValue& objError)
    {
        try 
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            errorMessage = QString::fromStdString(message) + " (code " + QString::number(code) + ")";
        }
        catch (const std::runtime_error&) 
        {   
            errorMessage = QString::fromStdString(objError.write());
        }
    }
    catch (const std::exception& e)
    {
        errorMessage = QString("Error: ") + QString::fromStdString(e.what());
    }

    return false;
}

void ExecRPCCommand::appendParam(QMap<QString, QString> &params, const QString &paramName, const QString &paramValue)
{
    QString _paramValue = paramValue.trimmed();
    if(!(_paramValue.isNull() || _paramValue.isEmpty())) params[paramName] = _paramValue;
}


