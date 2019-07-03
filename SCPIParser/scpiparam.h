/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.3.5-CPP (C++ version of Parser V1.3.5)		*/
/*																																										*/
/* (C) JPA Consulting Ltd., 2016	(www.jpacsoft.com)																	*/
/*																																										*/
/* View this file with tab spacings set to 2																					*/
/*																																										*/
/* scpiparam.h																																				*/
/* ===========																																				*/
/*																																										*/
/* Module Description																																	*/
/* ------------------																																	*/
/* Header file for scpiparam.c, SCPIParam class used in JPA-SCPI Parser								*/
/*																																										*/
/* Do not modify this file except where instructed in the code and/or documentation		*/
/*																																										*/
/* Refer to the JPA-SCPI Parser documentation for instructions as to using this code	*/
/* and notes on its design.																														*/
/*																																										*/
/* Module Revision History																														*/
/* -----------------------																														*/
/* V1.0.0:29/07/16:Initial Version																										*/
/**************************************************************************************/


/* Only include this header file once */
#ifndef SCPIPARAM_H
#define SCPIPARAM_H

#include <cstdint>

/**************************************************************************************/
/****************** Enumerations and Definitions used in Class ************************/
/**************************************************************************************/

/* Numeric Value type parameter Attribute flags */
#define SCPI_NUM_ATTR_NEG				(1)
#define SCPI_NUM_ATTR_REAL			(2)


#ifdef SUPPORT_CHAN_LIST_ADV
/* Channel List Entry Types */
enum enChanListEntryType
{
	E_CHANNEL_RANGE,														/* Channel Range */
	E_MODULE_CHANNEL,														/* Module Channel */
	E_PATH_NAME,																/* Path Name */
};
#endif


/**************************************************************************************/
/************** Structures relating to returned (populated) Parameters ****************/
/**************************************************************************************/

/* Attributes of a Numeric Value type parameter */
struct strAttrNumericVal
{
	unsigned long								ulSigFigs;			/* Significant Figures component of value 				*/
	unsigned char								Exp;						/* Exponent component of value 										*/
	BOOL												bNeg;						/* 1=Value is negative, 0=value is positive 			*/
	BOOL												bNegExp;				/* 1=Exponent is negative, 1=exponent is positive */
	enum enUnits								eUnits;					/* Type of units 																	*/
};

/* Attributes of a Booolean type parameter */
struct strAttrBoolean
{
	BOOL												bVal;						/* Value: 1=On, 0=Off */
};

/* Attributes of a Character Data type parameter */
struct strAttrCharData
{
	unsigned char								ItemNum;				/* Number of Character Data Item, within Sequence */
	const char 									*SSeq;					/* Character Data Sequence */
};

/* Attributes of a String, Unquoted String, Expression, Channel List or Numeric List type parameter */
struct strAttrString
{
	char												*pSString;			/* Pointer to start of string */
	SCPI_CHAR_IDX								Len;						/* Length of string 					*/
	char												Delimiter;			/* Delimiting character				*/
};

/* Union of all types of attributes */
union uniParamAttr
{
	struct strAttrNumericVal	sNumericVal;
	struct strAttrBoolean			sBoolean;
	struct strAttrCharData		sCharData;
	struct strAttrString			sString;
};


/**************************************************************************************/
/************************* SCPI Parameter Class Declaration ***************************/
/**************************************************************************************/

class SCPIParam
{
public:

	SCPIParam();																/* Constructor */

	/**************************************************************************************/
	/****************** Declarations of JPA-SCPI Parser Access Functions ******************/
	/**************************************************************************************/

	UCHAR SCPI_Type(enum enParamType *pePType, UCHAR *pNumSubtype);
	UCHAR SCPI_Units(enum enUnits *peUnits);
	UCHAR SCPI_ToCharDataItem(UCHAR *pItemNum);
	UCHAR SCPI_ToBOOL(BOOL *pbVal);
	UCHAR SCPI_ToUnsignedInt(unsigned int *puiVal);
	UCHAR SCPI_ToInt(int *piVal);
	UCHAR SCPI_ToUnsignedLong(unsigned long *pulVal);
	UCHAR SCPI_ToLong(long *plVal);
	UCHAR SCPI_ToDouble(double *pfdVal);
	UCHAR SCPI_ToString(char **pSString, SCPI_CHAR_IDX *pLen, char *pDelimiter);

	UCHAR SCPI_ToArbitraryBlock(uint8_t** ppBlock, SCPI_CHAR_IDX *pLen);	
	
#ifdef SUPPORT_NUM_LIST
	UCHAR SCPI_GetNumListEntry(UCHAR Index, BOOL *pbRange, SCPIParam *psFirst, SCPIParam *psLast);
#endif
#ifdef SUPPORT_CHAN_LIST_ADV
	UCHAR SCPI_GetChanListEntryType(UCHAR Index, enum enChanListEntryType *peEType);	/* Was SCPI_GetChanListEntryType */
	UCHAR SCPI_GetChanListModuleEntry(UCHAR Index, SCPIParam *ppsParModuleSpec, SCPIParam *ppsParChanRange);	/* Was SCPI_GetChanListModuleEntry */
	UCHAR SCPI_GetChanListPathEntry(UCHAR Index, char **pSPath, SCPI_CHAR_IDX *pPathLen);		/* Was SCPI_GetChanListPathEntry */
#endif
#ifdef SUPPORT_CHAN_LIST
	UCHAR SCPI_GetChanListEntry(UCHAR Index, UCHAR *pDims, BOOL *pbRange, SCPIParam sFirst[], SCPIParam sLast[]);	/* Was SCPI_GetChanListEntry */
#endif


	/**************************************************************************************/
	/*************** Declarations of Other Public Class Member Functions ******************/
	/**************************************************************************************/

#ifdef SUPPORT_CHAN_LIST
	UCHAR LocateEntryInChannelList(char *SStr, SCPI_CHAR_IDX Len, UCHAR Index, SCPI_CHAR_IDX *pStart, SCPI_CHAR_IDX *pEnd);
#endif
	UCHAR PopulateNumericValue(char *SNum, SCPI_CHAR_IDX Len, SCPI_CHAR_IDX *pNextPos);


	/**************************************************************************************/
	/***************** Declarations of Data Property Set/Get Functions ********************/
	/**************************************************************************************/

	enum enParamType getType(void);
	void setType(enum enParamType eType);
	void setCharDataItemNum (unsigned char NewItemNum);
	void setCharDataSeq (const char *SNewSeq);
	void setBooleanVal(BOOL bNewVal);
	struct strAttrNumericVal getNumericVal(void);
	void setNumericValSigFigs(unsigned long ulSigFigs);
	unsigned long getNumericValSigFigs(void);
	void setNumericValExp(unsigned char Exp);
	unsigned char getNumericValExp(void);
	void setNumericValNeg(BOOL bNeg);
	BOOL getNumericValNeg(void);
	void setNumericValNegExp(BOOL bNegExp);
	BOOL getNumericValNegExp(void);
	void setNumericValUnits(enum enUnits	eUnits);
	enum enUnits getNumericValUnits(void);
	void setString(char *pSNewString);
	void setStringLen(SCPI_CHAR_IDX NewLen);
	void setStringDelimiter(char Delimiter);


	/**************************************************************************************/
	/**************************** Private Member Variables ********************************/
	/**************************************************************************************/

private:
	enum	enParamType						eType;					/* Type of parameter 														*/
	union uniParamAttr					unAttr;					/* Attributes specific to the type of parameter */
};


#endif	/* SCPIPARAM_H */
