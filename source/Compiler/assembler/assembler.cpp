/*
 * Turbo Rascal Syntax error, “;” expected but “BEGIN” (TRSE, Turbo Rascal SE)
 * 8 bit software development IDE for the Commodore 64
 * Copyright (C) 2018  Nicolaas Ervik Groeneboom (nicolaas.groeneboom@gmail.com)
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (LICENSE.txt).
 *   If not, see <https://www.gnu.org/licenses/>.
*/

#include "assembler.h"
#include "source/Compiler/ast/node.h"
//QMap<QString, bool> LabelStack::sNumbersUsed;

int Appendix::s_id = 0;

RegisterStack Assembler::m_internalZP;


Assembler::Assembler()
{
  //  LabelStack::sNumbersUsed.clear();
    m_symTab = QSharedPointer<SymbolTable>(new SymbolTable());
    m_tempVarsBlock = QSharedPointer<Appendix>(new Appendix());
}

Assembler::~Assembler()
{
}

void Assembler::Save(QString filename) {
    if (QFile::exists(filename))
        QFile::remove(filename);

    QFile file(filename);
    if (file.open(QIODevice::ReadWrite)) {
        QTextStream stream(&file);
        for (QString& s: m_source)
            stream<< s<< "\n";
    }
    file.close();
}

void Assembler::Nl()
{
    m_source << "";
}

void Assembler::Write(QString str, int level)
{
//    qDebug() << "ASM : " <<str;
    if (m_currentBlock==nullptr) {

        QString s ="";
        for (int i=0;i<level;i++)
            s+="\t";
        s+=str;
        m_source.append(s);
    }
    else {
        m_currentBlock->Append(str,level);
//        qDebug() << "CURRENTBLOCK: " << str;
    }

    if (m_countCycles) {
        int cnt = CountCycles(str);

        for (int i=0;i<m_cycleCounter.count();i++)
            m_cycleCounter[i] += cnt;

        m_cycles[Node::m_currentLineNumber] += cnt;
    }
}

bool caseInsensitiveLessThan(const QSharedPointer<Appendix> s1, const QSharedPointer<Appendix> s2)
{
    QString sa = s1->m_pos.toLower().replace("$","0x");
    QString sb = s2->m_pos.toLower().replace("$","0x");

    int a,b;
    bool ok;

//    qDebug() << sa << " vs " << sb;

    if (sa.contains("0x")) a=sa.toInt(&ok, 16);
    else
        a = sa.toInt(&ok, 10);

    if (sb.contains("0x")) b=sb.toInt(&ok, 16);
    else
        b = sb.toInt(&ok, 10);


    return a < b;
}

/*int doSomething()
{
    QStringList list;
    list << "AlPha" << "beTA" << "gamma" << "DELTA";
    qStableSort(list.begin(), list.end(), caseInsensitiveLessThan);
    // list: [ "AlPha", "beTA", "DELTA", "gamma" ]
}*/
void Assembler::StartExistingBlock(QSharedPointer<Appendix> block)
{
    m_blockStack.append(m_currentBlock);
    m_currentBlock = block;
}

void Assembler::EndCurrentBlock()
{
    m_currentBlock = m_blockStack.last();
    m_blockStack.pop_front();
}

void Assembler::StartMemoryBlock(QString pos) {

    if (m_currentBlock!=nullptr) {
        if (m_currentBlock->m_pos==pos)
            return;
//        else ErrorHandler("")
    }
    for (QSharedPointer<Appendix> app: m_appendix) {

        if (app->m_pos == pos)
        {
            m_currentBlock = app;
            m_blockStack.append(m_currentBlock);
//            qDebug() << "LAST : " <<m_source.l
            if (m_currentBlock->m_source.last().toLower().startsWith("endblock"))
                m_currentBlock->m_source.removeLast();
            m_currentBlock->m_extraOutput = false;


            Comment("Resuming memory block at "+pos);
            m_currentBlock->m_id++;
            m_currentBlock->s_id++;
            return;

        }
    }

    Appendix app(pos);
    m_currentBlock = QSharedPointer<Appendix>(new Appendix(pos));
    m_appendix.append(m_currentBlock);
    m_currentBlock->Append(GetOrg(Util::NumberFromStringHex(pos)),1);
    m_blockStack.append(m_currentBlock);
    Comment("Starting new memory block at "+pos);
    QString p = pos;
    p = p.remove("$");
    Label("StartBlock"+p);
//    qDebug() << "Starting new memory block at "+pos;
    //        m_currentBlockCount = m_appendix.count()-1;
}

void Assembler::EndMemoryBlock() {
    //        qDebug() << "Trying to end memory block.. ";
    Comment("Ending memory block");
    if (m_currentBlock!=nullptr && m_currentBlock->m_extraOutput == false) {
//        Label("EndBlock"+QString::number(m_currentBlock->m_id));
        QString s = m_currentBlock->m_pos;
        s = s.remove("$");
        Label("EndBlock"+s);
        m_currentBlock->m_extraOutput = true;

    }
    m_currentBlock=nullptr;

    if (m_blockStack.count()>0)
        m_blockStack.removeLast();

    if (m_blockStack.count()!=0) {
        m_currentBlock = m_blockStack.last();
        // qDebug() << "STILL STACK : " << m_blockStack.count();
    }
  //  if (m_currentBlock!=nullptr)
    //    qDebug() << "AT END: " << m_blockStack.count() << m_currentBlock->m_pos;;
//    else m_currentBlock = nullptr;

}

void Assembler::SortAppendix()
{
    /*   for (int i=0;i<m_appendix.count();i++)
        qDebug() << "appos:" << m_appendix[i].m_pos;*/
std::sort(m_appendix.begin(), m_appendix.end(), caseInsensitiveLessThan);
}

void Assembler::PushCounter()
{
    m_cycleCounter.append(0);
}

void Assembler::PopCounter(int ln)
{
    if (m_cycleCounter.count()==0)
        return;
    int i = m_cycleCounter.last();
    m_cycleCounter.removeLast();
    m_blockCycles[ln]=+i;
    //    if (i>m_cycles[ln]) // Only count largest number
    //       m_cycles[ln] += i;
    //   return i;
//    qDebug() << "POP <<"<<ln << m_cycles[ln];
  //  qDebug() << m_cycles.keys();

}

void Assembler::PushBlock(int ln)
{
    if (m_blockIndent.contains(ln))
        //PushBlock(ln+1);
        m_blockIndent[ln] = m_blockIndent[ln]+1;
    else
        m_blockIndent[ln] = 1;
    //   qDebug()<< "push: " <<m_blockCounter.count()  << " at " << ln;

}

void Assembler::PopBlock(int ln)
{
    if (m_blockIndent.contains(ln))
//        PopBlock(ln-1);
        m_blockIndent[ln] = m_blockIndent[ln]-1;

    else
        m_blockIndent[ln] = -1;

}

int Assembler::m_prevCycles = 0;

int Assembler::getLineCount() {
    int lc = 0;
    for (QString s: m_source) {
        s=s.remove("\n");
        s=s.remove("\t");
        s=s.trimmed().simplified();
        if (s=="")
            continue;
        if (s.startsWith(";"))
            continue;
        if (s.startsWith("//"))
            continue;
        lc++;
    }
    return lc;

}

int Assembler::CountCycles(QString s)
{
    QStringList lst = s.split("\n");
    int n=0;
    for (QString s : lst) {
        //        s=s.remove("\t");
        s=s.trimmed();
        if (s=="") continue;
        if (s.startsWith(";")) continue;
        int i = s.indexOf(";");
        if (i!=-1) { // Remove all comments
            s = s.remove(i, s.length());
        }
        //if (s[0]!="\t") continue; // is label
        QStringList l = s.split(" ");
        if (l[0].count()==3) {
            //qDebug() << "Adding :" << s;
            int cnt = CountInstructionCycle(l);
            n+=cnt;
//            if (cnt!=0) {
  //              qDebug() << "Counted for : '" <<s <<"' : " <<cnt;
    //        }
        }

    }
    return n;

}

void Assembler::Delete()
{
  //delete m_symTab;
    //m_symTab = nullptr;

    m_source = QStringList();

//    qDebug() << "Assembler::delete no blocks " <<blocks.count();
  /*  for (MemoryBlock* mb: blocks)
        if (mb!=nullptr)
            delete mb;*/
    blocks.clear();
}

QString Assembler::getLabel(QString s) {
    QString pre ="";
    if (m_currentBlockName!="")
        pre = m_currentBlockName+"_";
    return pre+s+m_labelStack[s].m_current;
}

QString Assembler::NewLabel(QString s) {
    m_labelStack[s].push();
    QString pre ="";
    if (m_currentBlockName!="")
        pre = m_currentBlockName+"_";
    return pre+ s+m_labelStack[s].m_current;
}

void Assembler::PopLabel(QString s) {
    m_labelStack[s].pop();
}



void Assembler::Asm(QString s, QString comment)
{
    QString c = "\t;" + comment;
    if (comment=="") c="";
    if (s=="" && comment=="") return;
    Write(s+c ,1);
 /*   if (m_currentBlock!=nullptr)
        qDebug() << m_currentBlock->m_pos + " : "+ s;
    else
        qDebug() << "NULL : "+ s;
*/
}

void Assembler::Label(QString s)
{
    Write(s,0);
}

void Assembler::IncludeFile(QString pfile)
{
    QFile file(pfile);
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open core include file: " +pfile;
        exit(1);
        return;
    }

    QTextStream in(&file);

    QStringList source;

    while(!in.atEnd()) {
        QString line = in.readLine();
        for (QString key: m_replaceValues.keys())
            line = line.replace(key, m_replaceValues[key]); // Replace stuff like zps

        /*        if (line.startsWith(" ") | line.startsWith("\t"))
                Asm(line);
                else
                Label(line);*/
        source << line;
        //      QStringList fields = line.split(",");
    }
    file.close();

    if (m_currentBlock==nullptr)
        m_source<<source;
    else
        m_currentBlock->m_source<<source;;

}

bool Assembler::DeclareRecord(QString name, QString type, int count, QStringList data, QString pos)
{
    if (m_symTab->m_records.contains(type)) {
        if (m_symTab->m_records[type]->m_isClass)
            return false;
        if (pos!="") {
            int p = Util::NumberFromStringHex(pos);

            QSharedPointer<SymbolTable>  st = m_symTab->m_records[type];
            for (QString v : st->m_orderedByDefinition) {
                QSharedPointer<Symbol> s = st->m_symbols[v];
                //qDebug() << "WTF " <<s->m_name <<s->m_type;
                // Build the name
                QString n = name + "_" + st->m_name+"_"+s->m_name;
                QString w = n+"";
                //            QString t = byte;
                //
                //                  t= word;

                w = w+ "\t EQU \t" + Util::numToHex(p);


                //if (s->m_type.toLower()=="integer")
                //    ErrorHandler::e.Error("Record types does not support integer (yet) for record : " + type);
                if (s->m_type.toLower()=="string")
                    ErrorHandler::e.Error("Record types does not support strings (yet) for record : " + type);
                Write(w);
                int scale = 1;
                if (s->m_type.toLower()=="integer")
                    scale=2;
                p+=count*scale;

            }


            return true;
        }
        //          StartMemoryBlock(pos);

        //        ErrorHandler::e.Error("Record types not implemented yet: " + type);
        QSharedPointer<SymbolTable>  st = m_symTab->m_records[type];
        for (QSharedPointer<Symbol> s : st->m_symbols) {
//            qDebug() << "WTF " <<s->m_name <<s->m_type;
            // Build the name
            QString n = name + "_" + st->m_name+"_"+s->m_name;
            QString w = n;
            QString t = byte;
            if (s->m_type.toLower()=="integer")
                t= word;
            if (s->m_type.toLower()=="pointer")
                t= ppointer;
            if (s->m_type.toLower()=="long")
                t= llong;

            w = w+ "\t"+t + "\t0";


            //if (s->m_type.toLower()=="integer")
            //    ErrorHandler::e.Error("Record types does not support integer (yet) for record : " + type);
            if (s->m_type.toLower()=="string")
                ErrorHandler::e.Error("Record types does not support strings (yet) for record : " + type);
            Write(w);
            int scale = 1;

            QString bytes = "";
            for (int i=0;i<count-1;i++)
                bytes+="0,";
            bytes.remove(bytes.count()-1,1);
            if (count!=1)
                Asm("    "+t+" "+bytes);
            //                Asm("org "+n+"+" +QString::number(count*scale));

        }
        //    if (pos!="")
        //      EndMemoryBlock();

        return true;
    }
    return false;
}

bool Assembler::DeclareClass(QString name, QString type, int count, QStringList data, QString pos)
{
    if (m_symTab->m_records.contains(type)) {
        if (!m_symTab->m_records[type]->m_isClass)
            return false;

        QSharedPointer<SymbolTable>  st = m_symTab->m_records[type];
        Label(name);
        Asm("org "+name+"+" +QString::number(st->getSize()*count));

        return true;
    }
    return false;

}

void Assembler::WriteConstants()
{
    for (QString key: m_symTab->m_constants.keys()) {
        QSharedPointer<Symbol> s = m_symTab->m_constants[key];
        m_source << key + " EQU " + Util::numToHex(s->m_value->m_fVal);
    }
}

void Assembler::Term(QString s, bool write)
{
    m_term+=s;
    if (write) {
        Asm(m_term);
        ClearTerm();

    }
}

void Assembler::Term()
{
    Asm(m_term);
    ClearTerm();
}

void Assembler::Connect()
{
    // Connect with temp vars


    if (m_mainBlock==nullptr) {

        QStringList newSource;
        for (int i=0;i<m_varDeclEndsLineNumber;i++) {
            newSource<<m_source[i];
        }
        newSource << " ; Temp vars section";
        newSource << m_tempVars;
        if (m_tempVarsBlock!=nullptr)
            newSource << m_tempVarsBlock->m_source;

        newSource << " ; Temp vars section ends";
        for (int i=m_varDeclEndsLineNumber;i<m_source.count(); i++) {
            newSource<<m_source[i];
        }
        m_source = newSource;
    }
    else {
        for (QString t : m_tempVars) {
            m_mainBlock->Append(t,0);
        }
        if (m_tempVarsBlock!=nullptr)
            m_mainBlock->m_source<<m_tempVarsBlock->m_source;

    }

    //    m_appendix.append(m_ extraBlocks);
    SortAppendix();

    //  qDebug() << m_appendix[0].m_source;
    QStringList pre;
    for (int i=0;i<m_appendix.count();i++) {
        //                qDebug() << (m_appendix[i].m_pos);
        if (Util::NumberFromStringHex(m_appendix[i]->m_pos)<Syntax::s.m_currentSystem->m_programStartAddress)
            pre <<m_appendix[i]->m_source;
        else m_source << m_appendix[i]->m_source;

    }

    m_source = QStringList() << " processor 6502" <<pre << m_source;
    m_source.removeAll("");
    // Delete appendix
    //    qDebug() << "Deleting appendices : "<<m_appendix.count() << m_blockStack.count();
    /*    for (QSharedPointer<Appendix> a: m_appendix)
        delete a;*/
    //   m_appendix.clear();
    /*    for (Appendix* a: m_blockStack)
        delete a;
    m_blockStack.clear();*/
}





int LabelStack::m_labelCount =0;
void LabelStack::push() {
    bool ok=false;
    while (!ok) {
        m_labelCount++;
        m_current = QString::number(m_labelCount);//QString::number(rand()%100000);
        ok=true;
        if (sNumbersUsed.contains(m_current))
            ok = false;
    }
    sNumbersUsed[m_current] = true;

    m_vars.push_back(m_current);
}

void LabelStack::pop() {
    if (m_vars.count()==0) {
                    ErrorHandler::e.Error("Trying to pop labelstack from zero : " + m_current);
        return;
    }
    m_vars.remove(m_vars.count()-1);
    if (m_vars.count()!=0)
        m_current = m_vars[m_vars.count()-1];

}

MemoryBlock::~MemoryBlock() {
}
