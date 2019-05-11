#include "Mitsubishi.h"
#include "DataPack.h"

#include <string.h>
#include <QObject>

enum _TCPUMEM
{
    CM_NON  =   0,
    CM_X    =   1,
    CM_Y    =   2,
    CM_M    =   3,
    CM_S    =   4,
    CM_D   	=   5,
    CM_T   	=   6,
    CM_C16  =  	7,
    CM_C32  =  	8   
};

typedef enum _TCPUMEM TCPUMEM;


static quint16 getBeginAddrAsCpuMem(TCPUMEM cm, TTagDataType dataType)
{
    quint16 wRet = 0;

	switch(cm)
	{
	case CM_X:
		wRet = 0x0080;
		break;
	case CM_Y:
		wRet = 0x00A0;		
		break;
	case CM_M:
		wRet = 0x0100;
		break;
	case CM_S:
		wRet = 0x0000;
		break;
	case CM_D:
		wRet = 0x1000;
		break;
	case CM_T:
        if(dataType == TYPE_BOOL)
			wRet = 0x00C0;
		else
			wRet = 0x0800;	
		break;
	case CM_C16:
        if(dataType == TYPE_BOOL)
			wRet = 0x01C0;
		else
			wRet = 0x0A00;
		break;
	case CM_C32:
        if(dataType == TYPE_BOOL)
			wRet = 0x01C0;
		else
			wRet = 0x0C00;
		break;

	default:
		break;			
	}

	return wRet;
}

//4字节ASCII
static void Mitsubishi_MakeAddress(quint16 wAddress, quint8 * pStore)
{
	SetDataAsWord(pStore, wAddress);
	RecoverSelfData(pStore, 2);
	MakeCodeToAsii(pStore, pStore, 2);
}

//2字节ASCII
static void Mitsubishi_MakeDatalen(quint16 wDataLen, quint8 * pStore)
{
    quint8 byDataLen = (quint8)wDataLen;
	MakeCodeToAsii(&byDataLen, pStore, 1);
}

static quint16 Mitsubishi_GetboolBlockByte(quint32 dwDataPos, size_t RepeatLen)
{
    int iTemp = (quint16)dwDataPos % 8;
    quint16 wDataLen = (iTemp > 0) ? 1 : 0;
	
	if(iTemp > 0)
		iTemp = 8 - iTemp;
	
	iTemp = RepeatLen - iTemp;		
	wDataLen += iTemp / 8;
	
	iTemp = iTemp % 8;
	wDataLen += (iTemp > 0) ? 1 : 0;		

	return wDataLen;
}

static quint16 Mitsubishi_GetAddressOffset(TCPUMEM cm, quint16 wDataPos)
{
    quint16 wOffSet;

	if((cm == CM_T) || (cm == CM_D)
			|| (cm == CM_C16))
	{
		wOffSet = wDataPos * 2;
	}
	else if(cm == CM_C32)
	{
		wOffSet = (wDataPos - 200) * 4;
	}
	else
	{
		//布尔块
		wOffSet = wDataPos / 8;
	}

	return wOffSet;
}

static TCPUMEM getCpuMem(const QString &szRegisterArea) {
    if(szRegisterArea == QObject::tr("X"))
        return CM_X;
    else if(szRegisterArea == QObject::tr("Y"))
        return CM_Y;
    else if(szRegisterArea == QObject::tr("M"))
        return CM_M;
    else if(szRegisterArea == QObject::tr("S"))
        return CM_S;
    else if(szRegisterArea == QObject::tr("D"))
        return CM_D;
    else if(szRegisterArea == QObject::tr("T"))
        return CM_T;
    else if(szRegisterArea == QObject::tr("C16"))
        return CM_C16;
    else if(szRegisterArea == QObject::tr("C32"))
        return CM_C32;
    else
        return CM_NON;
}


Mitsubishi::Mitsubishi(QObject *parent)
    : QObject(parent),
      iFacePort_(nullptr) {
    memset(readDataBuffer_, 0, sizeof(readDataBuffer_)/sizeof(quint8));
    memset(writeDataBuffer_, 0, sizeof(writeDataBuffer_)/sizeof(quint8));
}


Mitsubishi::~Mitsubishi() {

}


void Mitsubishi::setPort(IPort *pPort) {
    if(pPort != nullptr)
        iFacePort_ = pPort;
}


IPort *Mitsubishi::getPort() {
    return iFacePort_;
}


/**
 * @brief Mitsubishi::isCanWrite
 * @details 判断该变量定义属性是否可以写
 * @param pTag 设备变量
 * @return false-不可写，true-可写
 */
bool Mitsubishi::isCanWrite(IOTag* pTag) {
    if(getCpuMem(pTag->GetRegisterArea()) == CM_X)
        return false;
    else if(getCpuMem(pTag->GetRegisterArea()) == CM_C32) {
        if(pTag->GetDataType() == TYPE_UINT32)
            return false;
    }

    return true;
}


/**
 * @brief Mitsubishi::writeData
 * @details 写一个变量值到plc设备
 * @param pTag 设备变量
 * @return 0-失败,1-成功
 */
int Mitsubishi::writeData(IOTag* pTag) {
    DBTagObject *pDBTagObject = pTag->GetDBTagObject();
    quint16 wAddress = getBeginAddrAsCpuMem(getCpuMem(pTag->GetRegisterArea()), pTag->GetDataType());
    size_t len = 0;

    memset(writeDataBuffer_, 0, sizeof(writeDataBuffer_)/sizeof(quint8));
    memset(readDataBuffer_, 0, sizeof(readDataBuffer_)/sizeof(quint8));

    writeDataBuffer_[len++] = 0x02;
    if(pTag->GetDataType() == TYPE_BOOL) {
        if(pDBTagObject->GetWriteData().toUInt())
            writeDataBuffer_[len++] = 0x37;
        else
            writeDataBuffer_[len++] = 0x38;

        //生成地址
        wAddress = wAddress * 8 + pTag->GetRegisterAddress() + pTag->GetOffset();
        /////////////////////////////
        //强制位操作地址低字节在前(与非位操作相反)
        RecoverSelfData((quint8 *)&wAddress, 2);
        /////////////////////////////
        Mitsubishi_MakeAddress(wAddress, writeDataBuffer_ + len);
        len += 4;
        writeDataBuffer_[len++] = 0x03;

        //生成校验和
        *(quint8 *)(writeDataBuffer_ + len) = (quint8)AddCheckSum(writeDataBuffer_ + 1, len - 1);
        MakeCodeToAsii(writeDataBuffer_ + len, writeDataBuffer_ + len, 1);
        len += 2;
    } else {
        //按字节写
        quint16 wDataLen;

        writeDataBuffer_[1] = 0x31;

        //计算地址
        wAddress += Mitsubishi_GetAddressOffset(getCpuMem(pTag->GetRegisterArea()), (quint16)(pTag->GetRegisterAddress() + pTag->GetOffset()));

        //生成地址数据
        Mitsubishi_MakeAddress(wAddress, writeDataBuffer_ + 2);
        len = 8;

        //获取数据长度
        wDataLen = (quint16)pTag->GetDataTypeLength();

        //填写变量数据
        memcpy(writeDataBuffer_ + len, pDBTagObject->GetWriteDataBytes().data(), wDataLen);


        //将变量数据修正成ASCII码
        MakeCodeToAsii(writeDataBuffer_ + len, writeDataBuffer_ + len, wDataLen);

        //补充数据字节长度
        Mitsubishi_MakeDatalen(wDataLen, writeDataBuffer_ + 6);

        //填写结束符
        len += wDataLen * 2;
        writeDataBuffer_[len] = 0x03;
        len += 1;

        //生成校验和
        *(quint8 *)(writeDataBuffer_ + len) = (quint8)AddCheckSum(writeDataBuffer_ + 1, len - 1);
        MakeCodeToAsii(writeDataBuffer_ + len, writeDataBuffer_ + len, 1);
        len += 2;
    }

    //清空接收缓冲区
    memset(readDataBuffer_, 0, sizeof(readDataBuffer_)/sizeof(quint8));

    //发送写命令帧
    if(getPort() != nullptr)
        getPort()->write(writeDataBuffer_, len, 1000);

    //等待plc应答
    int resultlen = 0;
    if(getPort() != nullptr)
        resultlen = getPort()->read(readDataBuffer_, 1, 1000);

    //判断应答结果
    if(resultlen == 1 && readDataBuffer_[0] == 0x06)
        return 1;

    return 0;
}


/**
 * @brief Mitsubishi::isCanRead
 * @details 判断该变量定义属性是否可以读
 * @param pTag 设备变量
 * @return false-不可读，true-可读
 */
bool Mitsubishi::isCanRead(IOTag* pTag) {
    Q_UNUSED(pTag)
    return true;
}


/**
 * @brief Mitsubishi::readData
 * @details 从plc设备读一个变量
 * @param pTag 设备变量
 * @return 0-失败,1-成功
 */
int Mitsubishi::readData(IOTag* pTag) {
    quint16 wAddress = getBeginAddrAsCpuMem(getCpuMem(pTag->GetRegisterArea()), pTag->GetDataType());
	size_t len = 0;
    quint16 wDataLen = 0;
    quint8 byCheckSum = 0;
    quint8 *pVailableData;
	//DM特殊协议位置调整长度
	size_t iDMAreaSpecPro = 0;

    memset(writeDataBuffer_, 0, sizeof(writeDataBuffer_)/sizeof(quint8));
    memset(readDataBuffer_, 0, sizeof(readDataBuffer_)/sizeof(quint8));

    writeDataBuffer_[len++] = 0x02;
    writeDataBuffer_[len++] = 0x30;

    if(pTag->GetDataType() == TYPE_BOOL) {
		//布尔块
        wAddress += (quint16)(pTag->GetRegisterAddress() + pTag->GetOffset()) / 8;
        Mitsubishi_MakeAddress(wAddress, writeDataBuffer_ + len);
		
		//计算应读取数据字节长度
        wDataLen = Mitsubishi_GetboolBlockByte((quint32)(pTag->GetRegisterAddress() + pTag->GetOffset()), pTag->GetDataTypeLength());
	}
	else
	{
		//计算地址
        wAddress += Mitsubishi_GetAddressOffset(getCpuMem(pTag->GetRegisterArea()), (quint16)(pTag->GetRegisterAddress() + pTag->GetOffset()));
		
		//生成地址数据
        Mitsubishi_MakeAddress(wAddress, writeDataBuffer_ + len);
		
		//获取数据长度
        wDataLen = (quint16)pTag->GetDataTypeLength();
	}

	len += 4;	
    Mitsubishi_MakeDatalen(wDataLen, writeDataBuffer_ + len);
	len += 2;
	
    writeDataBuffer_[len] = 0x03;
	len += 1;

	//生成校验和
    *(quint8 *)(writeDataBuffer_ + len) = (quint8)AddCheckSum(writeDataBuffer_ + 1, len - 1);
    MakeCodeToAsii(writeDataBuffer_ + len, writeDataBuffer_ + len, 1);
	len += 2;

	//清空接收缓冲区
    memset(readDataBuffer_, 0, sizeof(readDataBuffer_)/sizeof(quint8));
	
	//发送读命令帧
    if(getPort() != nullptr)
        getPort()->write(writeDataBuffer_, len, 1000);

	//等待plc应答
    int resultlen = 0;
    if(getPort() != nullptr)
        resultlen = getPort()->read(readDataBuffer_, 4 + wDataLen * 2, 1000);
    if(resultlen != (4 + wDataLen * 2))
		return -2;

	//检查帧中关键标志
    if((readDataBuffer_[0] != 0x02) || (readDataBuffer_[1 + wDataLen * 2] != 0x03))
		return 0;

	//检查校验和
    byCheckSum = (quint8)AddCheckSum(readDataBuffer_ + 1, wDataLen * 2 + 1);
    pVailableData = readDataBuffer_ + 2 + wDataLen * 2;
	MakeAsiiToCode(pVailableData, pVailableData, 1);	
	if(byCheckSum != *pVailableData)
		return 0;

	//将ASCII码修正成变量数据
    MakeAsiiToCode(readDataBuffer_ + 1, readDataBuffer_ + 1, wDataLen);

    pVailableData = readDataBuffer_ + 1;
    len = pTag->GetDataTypeLength();
    memcpy(pTag->pReadBuf, pVailableData, len);
	return 1;
}




