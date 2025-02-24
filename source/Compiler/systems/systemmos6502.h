#ifndef SYSTEMMOS6502_H
#define SYSTEMMOS6502_H

#include "source/Compiler/systems/abstractsystem.h"
#include "source/Compiler/misc/machinecodeanalyzer.h"
#include <QMessageBox>
#include "source/messages.h"


class SystemMOS6502 : public AbstractSystem
{
public:
    SystemMOS6502(QSharedPointer<CIniFile> settings, QSharedPointer<CIniFile> proj) : AbstractSystem(settings, proj) {
        m_allowedGlobalTypeFlags << "compressed"<<"pure"<<"pure_variable" <<"pure_number" << "signed" <<"no_term" <<"invert";
        m_allowedProcedureTypeFlags << "pure"<<"pure_variable" <<"pure_number" << "signed" <<"no_term" <<"invert" <<"global" <<"stack";
        m_supportsExomizer = true;
        m_registers << "_a"<<"_x" <<"_y" <<"_ax" <<"_ay" <<"_xy";
        m_canRunAsmFiles = true;
        m_allowClasses = true; // EXPERIMENTAL


    }

    MachineCodeAnalyzer m_mca;


    virtual void Assemble(QString& text, QString file, QString currentDir, QSharedPointer<SymbolTable>  symTab) override;
    virtual void PostProcess(QString& text, QString file, QString currentDir) override;


    virtual bool AllowPointerInStructs() override {return false;}

    bool VerifyMachineCodeZP(QString fname);

    void CreateDisk(QString currentDir, QString filename, QString iniData, bool addPrg);
    bool BuildDiskFiles(QString currentDir, QStringList& d64Params, QString iniData);

    void DefaultValues() override;

    QString CompressFile(QString fileName) override
    {
        return CompressLZ4(fileName);
    }


};

#endif // SYSTEMMOS6502_H
