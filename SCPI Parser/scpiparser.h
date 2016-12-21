/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.4.0CPP (C++ version of Parser V1.3.5)		*/
/*																																										*/
/* (C) JPA Consulting Ltd., 2016	(www.jpacsoft.com)																	*/
/*																																										*/
/* View this file with tab spacings set to 2																					*/
/*																																										*/
/* scpi.h																																							*/
/* ======																																							*/
/*																																										*/
/* Module Description																																	*/
/* ------------------																																	*/
/* Header file for scpiparser.c, SCPI Parser class used in JPA-SCPI Parser						*/
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
#ifndef SCPIPARSER_H
#define SCPIPARSER_H

/**************************************************************************************/
/****************** Enumerations and Definitions used in Class ************************/
/**************************************************************************************/

/* Types of Keyword */
enum enKeywordType
{
	KEYWORD_COMMAND,														/* Keyword in command section					 */
	KEYWORD_CHAR_DATA														/* Keyword in character data parameter */
};


/**************************************************************************************/
/************************* SCPI Parser Class Declaration ******************************/
/**************************************************************************************/

class SCPIParser
{
public:

	SCPIParser(const char **pSKeywords, const struct strSpecCommand *sCommand);		/* Constructor */


/**************************************************************************************/
/****************** Declarations of JPA-SCPI Parser Access Functions ******************/
/**************************************************************************************/

#ifdef SUPPORT_NUM_SUFFIX
	UCHAR SCPI_Parse(char **pSInput, BOOL bResetTree, SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[],
		UCHAR *pNumSufCnt, unsigned int uiNumSuf[]);
#else
	UCHAR Parse(char **pSInput, BOOL bResetTree, SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[]);
#endif


	/**************************************************************************************/
	/********************* Declarations of Private Member Functions ***********************/
	/**************************************************************************************/

private:
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR ParseSingleCommand(char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, BOOL bResetTree,
		SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[], UCHAR *pNumSufCnt, unsigned int uiNumSuf[]);
#else
	UCHAR ParseSingleCommand(char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, BOOL bResetTree,
		SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[]);
#endif
	SCPI_CHAR_IDX LenOfKeywords(char *SInpCmd, SCPI_CHAR_IDX InpCmdLen);
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR KeywordsMatchSpec(const char *SSpec, SCPI_CHAR_IDX LenSpec, char *SInp, SCPI_CHAR_IDX LenInp, enum enKeywordType eKeyword,
		unsigned int uiNumSuf[], UCHAR *pNumSuf, SCPI_CHAR_IDX *pTreeSize);
#else
	UCHAR KeywordsMatchSpec(const char *SSpec, SCPI_CHAR_IDX LenSpec, char *SInp, SCPI_CHAR_IDX LenInp, enum enKeywordType eKeyword,
		SCPI_CHAR_IDX *pTreeSize);
#endif
	SCPI_CHAR_IDX SkipToNextRequiredChar(const char *SSpec, SCPI_CHAR_IDX Pos);
#ifdef SUPPORT_NUM_SUFFIX
	SCPI_CHAR_IDX SkipToEndOfOptionalKeyword(const char *SSpec, SCPI_CHAR_IDX Pos, UCHAR *pNumSufIndex, unsigned int uiNumSuf[]);
#else
	SCPI_CHAR_IDX SkipToEndOfOptionalKeyword(const char *SSpec, SCPI_CHAR_IDX Pos);
#endif
	UCHAR MatchesParamsCount(SCPI_CMD_NUM CmdSpecNum, UCHAR InpCmdParamsCnt);
	void GetParamsInfo(char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, SCPI_CHAR_IDX InpCmdKeywordsLen, UCHAR *pParamsCnt, char **SParams,
		SCPI_CHAR_IDX *pParamsLen);
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR TranslateParameters(SCPI_CMD_NUM CmdSpecNum, char *SInpParams,
		SCPI_CHAR_IDX InpParamsLen, SCPIParam sParam[], unsigned int uiNumSuf[], UCHAR *pNumSuf);
#else
	UCHAR SCPIParser::TranslateParameters(SCPI_CMD_NUM CmdSpecNum, char *SInpParams, SCPI_CHAR_IDX InpParamsLen,
		SCPIParam sParam[]);
#endif
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR TranslateCharDataParam(char *SParam, SCPI_CHAR_IDX ParLen,
		const struct strSpecParam *psSpecParam, SCPIParam *psParam, unsigned int uiNumSuf[], UCHAR *pNumSuf);
#else
	UCHAR TranslateCharDataParam(char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecParam *psSpecParam, SCPIParam *psParam);
#endif
	UCHAR TranslateBooleanParam(char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrBoolean *psSpecAttr, SCPIParam *psParam);
	UCHAR TranslateNumericValueParam(char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrNumericVal *psSpecAttr, SCPIParam *psParam);
	UCHAR TranslateStringParam(char *SParam, SCPI_CHAR_IDX ParLen, const enum enParamType ePType, SCPIParam *psParam);
#ifdef SUPPORT_EXPR
	UCHAR TranslateExpressionParam(char *SParam, SCPI_CHAR_IDX ParLen, SCPIParam *psParam);
#endif
#ifdef SUPPORT_NUM_LIST
	UCHAR TranslateNumericListParam(char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrNumList *pSpecAttr, SCPIParam *psParam);
#endif
#ifdef SUPPORT_CHAN_LIST
	UCHAR TranslateChannelListParam(char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrChanList *pSpecAttr, SCPIParam *psParam);
#endif
	void ResetCommandTree(void);
#ifdef SUPPORT_OPT_NODE_ORIG
	void SetCommandTree(SCPI_CMD_NUM CmdSpecNum);
#else
	void SetCommandTree(SCPI_CMD_NUM CmdSpecNum, SCPI_CHAR_IDX TreeSize);
#endif
	UCHAR CharFromFullCommand(char *SInpCmd, SCPI_CHAR_IDX Pos, SCPI_CHAR_IDX Len);


	/**************************************************************************************/
	/**************************** Private Member Variables ********************************/
	/**************************************************************************************/

private:
	const char **mSSpecCmdKeywords;								/* Array of command keywords for command specifications						*/
	const struct strSpecCommand *msSpecCommand;		/* Array of command specifications																*/
	const char *mSCommandTree;										/* Pointer to start of command tree																*/
	SCPI_CHAR_IDX mCommandTreeLen;								/* Length of command tree (size minus number of square brackets)	*/
#ifndef SUPPORT_OPT_NODE_ORIG
	BOOL mbCommandTreeColonReqd;									/* Flag: TRUE if command tree requires a colon after its contents	*/
#endif
#ifdef SUPPORT_NUM_SUFFIX
	unsigned int muiNumSufPrev[MAX_NUM_SUFFIX];		/* Numeric suffices from the previous command (when compound command entered) */
#endif
};


/**************************************************************************************/
/******************* Declarations of Non-Class-Member Functions ***********************/
/**************************************************************************************/

UCHAR TranslateUnits(char *SUnits, SCPI_CHAR_IDX UnitsLen, const struct strSpecAttrNumericVal *pSpecAttr,
	enum enUnits *peUnits, signed char *pUnitExp);
BOOL StringsEqual(const char *S1, SCPI_CHAR_IDX Len1, const char *S2, SCPI_CHAR_IDX Len2);
long round(double fdVal);
#ifdef SUPPORT_NUM_SUFFIX
SCPI_CHAR_IDX DigitCount(unsigned int uiNum);
char GetDigitChar(unsigned int uiNum, SCPI_CHAR_IDX Pos);
#endif


#endif	/* SCPIPARSER_H */
