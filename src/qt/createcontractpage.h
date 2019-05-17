#ifndef CREATECONTRACTPAGE_H
#define CREATECONTRACTPAGE_H

#include <QWidget>
#include <QMessageBox>

class PlatformStyle;
class WalletModel;
class ClientModel;
class ExecRPCCommand;
class ABIFunctionField;
class ContractABI;


namespace Ui
{
class CreateContractPage;
}

class CreateContractPage : public QWidget
{
    Q_OBJECT
public:
    explicit CreateContractPage(QWidget *parent = nullptr);
    ~CreateContractPage();

    void setModel(WalletModel *_model);

signals:

public slots:


private:
    Ui::CreateContractPage* ui;
    WalletModel* m_model;
    ClientModel* m_clientModel;
    

    
    ExecRPCCommand* m_execRPCCommand;
    ABIFunctionField* m_ABIFunctionField;
    ContractABI* m_contractABI;
    int m_results;
    

    
    ExecRPCCommand* m_execRPCCommand_sendto;
    ABIFunctionField* m_ABIFunctionField_sendto;
    ContractABI* m_contractABI_sendto;
    int m_results_sendto;
    


    
    ExecRPCCommand* m_execRPCCommand_call;
    ABIFunctionField* m_ABIFunctionField_call;
    ContractABI* m_contractABI_call;
    int m_results_call;
    

    const PlatformStyle* platformStyle;


    QString toDataHex(int func, QString& errorMessage);
    QString toDataHex_Sendto(int func, QString& errorMessage);
    QString toDataHex_Call(int func, QString& errorMessage);

    bool isFunctionPayable();
    bool isValidContractAddress();

private slots:
    
    void on_createContractClicked();
    void on_updateCreateButton();
    void on_newContractABI();
    

    
    void on_sendToContractClicked();
    void on_updateSendToContractButton();
    void on_newContractABI_sendto();
    void on_functionChanged();
    void on_contractAddressChanged();
    


    
    void on_callContractClicked();
    void on_updateCallContractButton();
    void on_newContractABI_call();
    


};

#endif 
