/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.4.1-CPP (C++ version of Parser V1.3.5)		*/
/*																																										*/
/* (C) JPA Consulting Ltd., 2019	(www.jpacsoft.com)																	*/
/*																																										*/
/* View this file with tab spacings set to 2																					*/
/*																																										*/
/* scpiparser.c																																				*/
/* ============																																				*/
/*																																										*/
/* Module Description																																	*/
/* ------------------																																	*/
/* Implementation of SCPIParser class, part of JPA-SCPI Parser												*/
/*																																										*/
/* Do not modify this file except where instructed in the code and/or documentation		*/
/*																																										*/
/* Refer to the JPA-SCPI Parser documentation for instructions as to using this code	*/
/* and notes on its design.																														*/
/*																																										*/
/* Module Revision History																														*/
/* -----------------------																														*/
/* V1.0.0:29/07/16:Initial Version 																										*/
/* V1.0.1:17/07/18:Fixed name of SCPIParser::SCPI_Parse() function used when SUPPORT_-*/
/*                 NUM_SUFFIX is not defined (was Parse())														*/
/* V1.0.2:16/02/19:Add input parameter count parameter to SCPIParser::TranslateParam- */
/*								 eters() and fix method to return parameter type as P_NONE if a			*/
/*								 trailing optional unquoted string parameter is not present in			*/
/*								 input string. (Was returning as P_UNQ_STR of zero length.)					*/
/**************************************************************************************/


/* USER: Include any headers required by your compiler here														*/
#include "cmds.h"
#include "scpi.h"
#include "scpiparam.h"
#include "scpiparser.h"
#include "scpipriv.h"


/**************************************************************************************/
/*********************** Constants used in this module ********************************/
/**************************************************************************************/

const struct strSpecAttrNumericVal sSpecAttrBoolNum = {U_NONE, (const enum enUnits *)0, 0};
																							/* Parameter Spec's Numeric Value attributes used to translate
																							   Numeric Values into Booleans 															 */


/**************************************************************************************/
/*********************** SCPIParser Class Implementation ******************************/
/**************************************************************************************/

/**************************************************************************************/
/* Constructor																																				*/
/*																																										*/
/* Parameters:																																				*/
/*  [in] pKeywords		-	Pointer to array of command keyword specifications.						*/
/*	[in] sCommand			- Pointer to array of command specifications.										*/
/**************************************************************************************/
SCPIParser::SCPIParser(const char **SKeywords, const struct strSpecCommand *sCommand)
{
	mSSpecCmdKeywords = SKeywords;
	msSpecCommand = sCommand;
};


/**************************************************************************************/
/******************** JPA-SCPI Parser Access Functions ********************************/
/**************************************************************************************/


/**************************************************************************************/
/* Parses an Input Command in the Input String.																				*/
/* If a match is found then returns number of matching Command Spec, and returns			*/
/* values and attributes of any Input Parameters.																			*/
/*																																										*/
/* Parameters:																																				*/
/*	[in/out] pSInput 	-	Pointer to first char of Input String to be parsed.						*/
/*											Input String must be a null-terminated string.								*/
/*											This is returned modified, so as to point to first char of		*/
/*											the next Input Command of the Input String to be parsed.			*/
/*	[in] bResetTree	 	-	If TRUE then the Command Tree is reset to the Root node;			*/
/*											if FALSE then the Command Tree stays at the node set by the		*/
/*											previous command.																							*/
/*											Note: Set this to TRUE when parsing the first command of the	*/
/*											Input String,	and FALSE otherwise.														*/
/*	[out] pCmdSpecNum - Pointer to returned number of the	Command Spec that matches		*/
/*											the Input Command in the Input String that was parsed.				*/
/*											Value is undefined if no matching Command Spec is found.			*/
/*	[out] sParam			-	Array [0..MAX_PARAM-1] of returned parameters containing the	*/
/*											parsed Input Parameter values and attributes.									*/
/*											Contents of returned parameters are undefined if no matching	*/
/*											Command Spec is found.																				*/
/*  ONLY PRESENT IF SUPPORT_NUM_SUFFIX IS DEFINED:																		*/
/*	[out] pNumSufCnt	- Pointer to returned count of numeric suffices encountered			*/
/*	[out] uiNumSuf		- Array [0..MAX_NUM_SUFFIX-1] of returned numeric suffices			*/
/*											present	in the Input Command.																	*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			A matching Command Spec was found.							*/
/*	SCPI_ERR_NO_COMMAND			- Error:	There was no command to be parsed in the Input 	*/
/*																		String.																					*/
/*	SCPI_ERR_INVALID_CMD		- Error:	The Input Command keywords did not match any 		*/
/*																		Command Spec command keywords.									*/
/*	SCPI_ERR_PARAM_CNT			- Error:	The Input Command keywords matched a Command		*/
/*																		Spec but the wrong number of parameter given in */
/*																		the Input Command for the Command Spec.					*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter within the Input Command did not match*/
/*																		a	valid type of parameter in the Command Spec.	*/
/*	SCPI_ERR_PARAM_UNITS		- Error:	A parameter within the Input Command had the		*/
/*																		wrong type of units for the Command Spec.				*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	The Input Command contained a parameter of type */
/*																		Numeric Value that could not be stored				``*/
/*																		internally. This occurs if the value has an 		*/
/*																		exponent greater than +/-43.										*/
/*	SCPI_ERR_UNMATCHED_BRACKET-Error: The parameters in the Input Command contained		*/
/*																		an unmatched bracket														*/
/*	SCPI_ERR_UNMATCHED_QUOTE- Error:	The parameters in the Input Command contained		*/
/*																		an unmatched single or double quote.						*/
/*	SCPI_ERR_TOO_MANY_NUM_SUF-Error:	Too many numeric suffices in Input Command to		*/
/*																		be returned in return parameter array.					*/
/*	SCPI_ERR_NUM_SUF_INVALID-	Error:  Numeric suffix in Input Command is invalid			*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in a numeric/channel list is	*/
/*																		invalid, e.g.	floating point when not allowed		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
UCHAR SCPIParser::SCPI_Parse (char **pSInput, BOOL bResetTree, SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[],
 UCHAR *pNumSufCnt, unsigned int uiNumSuf[])
#else
UCHAR SCPIParser::SCPI_Parse (char **pSInput, BOOL bResetTree, SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[])
#endif
{
	UCHAR Err;
	SCPI_CHAR_IDX Pos = 0;
	char Ch;
	BOOL bDone = FALSE;
	SCPI_CHAR_IDX Len = 0;
	BOOL bWithinQuotes = FALSE;
	BOOL bDoubleQuotes = FALSE;

	/* Set Pos to beginning of Input Command in Input String, skipping whitespace and command separators */
	while (!bDone)
	{
		Ch = (*pSInput)[Pos];											/* Read character from string											*/
		if ((Ch == CMD_SEP) || (iswhitespace (Ch))) /* If found command separator or whitespace			*/
			Pos++;																	/* then try next character												*/
		else																			/* If found any other character										*/
			bDone = TRUE;														/* then this is the first char of Input Command 	*/
	}

	/* Set Len to length of the Input Command */
	bDone = FALSE;
	while (!bDone)
	{
		Ch = (*pSInput)[Pos+Len];									/* Read char from string */
		switch (Ch)
		{
			case DOUBLE_QUOTE :												/* Double-quote encountered										*/
				if (!(bWithinQuotes && !bDoubleQuotes)) /* If it is not embedded within single-quotes */
				{
					bWithinQuotes = !bWithinQuotes;				/* then toggle the in/out-of-quotes state			*/
					bDoubleQuotes = TRUE;
				}
				Len++;
				break;
			case SINGLE_QUOTE :												/* Single-quote encountered 									*/
				if (!(bWithinQuotes && bDoubleQuotes))	/* If it is not embedded within double-quotes	*/
				{
					bWithinQuotes = !bWithinQuotes;				/* then toggle the in/out-of-quotes state			*/
					bDoubleQuotes = FALSE;
				}
				Len++;
				break;
			case CMD_SEP:														/* Found command separator											*/
				if (!bWithinQuotes)										/* and it's not inside quotes										*/
					bDone = TRUE;												/* so we've reached the end of the Input Command*/
				else
					Len++;
				break;
			case '\0':															/* Found end of Input String										*/
				bDone = TRUE;													/* so we've reached the end of the Input Command*/
				break;
			default:																/* Any other character													*/
				Len++;

		}
		if (!Len)																	/* Prevent perpetual looping if Input Command exceeded limit */
			break;
	}

	if (Len)																		/* If Input Command is not zero-length then parse it */
#ifdef SUPPORT_NUM_SUFFIX
		Err = ParseSingleCommand (&((*pSInput)[Pos]), Len, bResetTree, pCmdSpecNum, sParam, pNumSufCnt, uiNumSuf);
#else
		Err = ParseSingleCommand(&((*pSInput)[Pos]), Len, bResetTree, pCmdSpecNum, sParam);
#endif
	else																				/* If Input Command is zero-length		*/
		Err = SCPI_ERR_NO_COMMAND;								/* Return error - no command to parse */

	*pSInput = &((*pSInput)[Pos+Len]);					/* Set returned pointer to first char of next command in Input String */

	return Err;
}


/**************************************************************************************/
/**************************** Private Member Functions ********************************/
/**************************************************************************************/


/**************************************************************************************/
/* Parses a single Input Command.																											*/
/* Finds the Command Spec that matches the Input Command and returns its number.			*/
/* Also returns the parsed parameters of the Input Command, if there are any.					*/
/* If no match is found, then returns an error code indicating nature of mis-match.		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SInpCmd -			Pointer to start of Input Command to be parsed.								*/
/*	[in] InpCmdLen -		Number of characters that make up the Input Command.					*/
/*	[in] bResetTree -		If TRUE then resets the Command Tree to the root before				*/
/*											parsing the Input Command; if FALSE then keeps the Command		*/
/*											Tree at the node set by the previous command.									*/
/*											Note: Set bReset to TRUE when parsing the first Input Command */
/*											in the Input String, otherwise set to FALSE.									*/
/*	[out] pCmdSpecNum - Pointer to returned number of the Command Spec that	matches		*/
/*											the Input Command. This value is undefined if no match found. */
/*	[out] sParam			- Array [0..MAX_PARAM-1] of returned parameters parsed in the		*/
/*											Input Command. Their contents is undefined if no match found.	*/
/*  ONLY PRESENT IF SUPPORT_NUM_SUFFIX IS DEFINED:																		*/
/*	[out] pNumSufCnt	- Pointer to returned count of numeric suffices encountered			*/
/*	[out] uiNumSuf		- Array [0..MAX_NUM_SUFFIX-1] of returned numeric suffices			*/
/*											present in the Input Command.																	*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			A matching Command Spec was found								*/
/*	SCPI_ERR_INVALID_CMD		- Error:	The Input Command keywords did not match any		*/
/*																		Command Spec command keywords										*/
/*	SCPI_ERR_PARAM_CNT			- Error:	The Input Command keywords matched a Command		*/
/*																		Spec but the wrong number of parameters were 		*/
/*																		present for the Command Spec										*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	A parameter within the Input Command did not 		*/
/*																		match the type of parameter in the Command Spec */
/*	SCPI_ERR_PARAM_UNITS		- Error:	A parameter within the Input Command had the 		*/
/*																		wrong type of units for the Command Spec				*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	The Input Command contained a parameter of type */
/*																		Numeric Value that could not be stored. This		*/
/*																		occurs if the value has an exponent greater			*/
/*																		than +/-43.																			*/
/*	SCPI_ERR_UNMATCHED_BRACKET-Error: The parameters in the Input Command contained		*/
/*																		an unmatched bracket														*/
/*	SCPI_ERR_UNMATCHED_QUOTE- Error:	The parameters in the Input Command contained 	*/
/*																		an unmatched single or double quote							*/
/*	SCPI_ERR_TOO_MANY_NUM_SUF-Error:	Too many numeric suffices in Input Command to		*/
/*																		be returned in return parameter array.					*/
/*	SCPI_ERR_NUM_SUF_INVALID- Error:  Numeric suffix in Input Command is invalid			*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in a numeric/channel list is	*/
/*																		invalid, e.g.	floating point when not allowed		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
UCHAR SCPIParser::ParseSingleCommand (char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, BOOL bResetTree,
 SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[], UCHAR *pNumSufCnt, unsigned int uiNumSuf[])
#else
UCHAR SCPIParser::ParseSingleCommand (char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, BOOL bResetTree,
 SCPI_CMD_NUM *pCmdSpecNum, SCPIParam sParam[])
#endif
{
	UCHAR RetErr;
	UCHAR Err;
	SCPI_CMD_NUM CmdSpecNum;
	SCPI_CHAR_IDX InpCmdKeywordsLen;
	UCHAR InpCmdParamsCnt;
	char *SInpCmdParams;
	SCPI_CHAR_IDX InpCmdParamsLen;
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR NumSufIndex;
#endif
	SCPI_CHAR_IDX TreeSize;

	if (SInpCmd[0] == CMD_ROOT)									/* If Input Command starts with colon								*/
	{
		bResetTree = TRUE;												/* then in SCPI this means "reset command tree"			*/
		SInpCmd = &(SInpCmd[1]);									/* Set start of Input Command string to second char	*/
		InpCmdLen--;															/* and also decrement Input Command length					*/
	}

	if (bResetTree)															/* Reset the command tree if required */
		ResetCommandTree ();

	RetErr = SCPI_ERR_INVALID_CMD;

	InpCmdKeywordsLen = LenOfKeywords (SInpCmd, InpCmdLen);	/* Get length of keywords in Input Command (plus Command Tree) */
	GetParamsInfo (SInpCmd, InpCmdLen, InpCmdKeywordsLen, &InpCmdParamsCnt, &SInpCmdParams, &InpCmdParamsLen);
																													/* Get information about parameters in the Input Command */

	/* Loop thru all Command Specs until a match is found or all possible matches have failed */
	for (CmdSpecNum = 0; (RetErr != SCPI_ERR_NONE) && (mSSpecCmdKeywords[CmdSpecNum][0]); CmdSpecNum++)
	{
#ifdef SUPPORT_NUM_SUFFIX
		NumSufIndex = 0;

		Err = KeywordsMatchSpec (mSSpecCmdKeywords[CmdSpecNum], strlen(mSSpecCmdKeywords[CmdSpecNum]), SInpCmd,
		 InpCmdKeywordsLen, KEYWORD_COMMAND, uiNumSuf, &NumSufIndex, &TreeSize);
#else
		Err = KeywordsMatchSpec (mSSpecCmdKeywords[CmdSpecNum], strlen(mSSpecCmdKeywords[CmdSpecNum]), SInpCmd,
		 InpCmdKeywordsLen, KEYWORD_COMMAND, &TreeSize);
#endif

		if (Err == SCPI_ERR_NONE)									/* If the command keywords match */
		{
			Err = MatchesParamsCount (CmdSpecNum, InpCmdParamsCnt);
																							/* Compare number of parameters in Input Command with Command Spec */
			if (Err == SCPI_ERR_NONE)								/* If parameter counts match */
			{
#ifdef SUPPORT_NUM_SUFFIX
				Err = TranslateParameters (CmdSpecNum, SInpCmdParams, InpCmdParamsLen, InpCmdParamsCnt, sParam, uiNumSuf, &NumSufIndex);
#else
				Err = TranslateParameters (CmdSpecNum, SInpCmdParams, InpCmdParamsLen, InpCmdParamsCnt, sParam);
#endif																				/* Translate parameters in Input Command */

				if (Err == SCPI_ERR_NONE)							/* If parameters were translated ok 														*/
				{
					*pCmdSpecNum = CmdSpecNum;					/* set returned value to the number of the matching Command Spec */
#ifdef SUPPORT_NUM_SUFFIX
					*pNumSufCnt = NumSufIndex;					/* set returned value of numeric suffices 											 */
#endif																				/* Translate parameters in Input Command */
					Err = SCPI_ERR_NONE;
					if (SInpCmd[0] != CMD_COMMON_START)	/* If Input Command is not a common command */
#ifdef SUPPORT_OPT_NODE_ORIG
						SetCommandTree (CmdSpecNum);			/* then set the Command Tree for use with the next Input Command  */
#else
						SetCommandTree (CmdSpecNum, TreeSize);	/* then set the Command Tree for use with the next Input Command  */
#endif
				}
			}
		}

		switch (Err)
		{
			case SCPI_ERR_NONE : RetErr = SCPI_ERR_NONE; break;		/* If a matching Command Spec was found then exit loop 		*/
			default						 : if (RetErr > Err) RetErr = Err;	/* If no match was found then set returned error code to
																															 the least significant error code encountered (error
																															 values are in order of significance)										*/
		}
	}

	return RetErr;
}


/**************************************************************************************/
/* Returns the length of the command keywords in an Input Command.										*/
/* The count includes the keywords of the current Command Tree.												*/
/* Note: The end of the command keywords is determined to be the first whitespace or	*/
/* when the end of the string is reached, whichever is occurs first.									*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SInpCmd -			Pointer to start of Input Command string											*/
/*	[in] InpCmdLen -		Length of Input Command string																*/
/*																																										*/
/* Return Value:																																			*/
/*	Length of command keywords																												*/
/**************************************************************************************/
SCPI_CHAR_IDX SCPIParser::LenOfKeywords (char *SInpCmd, SCPI_CHAR_IDX InpCmdLen)
{
	SCPI_CHAR_IDX Len;
	char Ch;

	Len = 0;
	do
	{
		Ch = CharFromFullCommand (SInpCmd, Len, mCommandTreeLen + InpCmdLen);	/* Get a char from the concatenation of the
																																						 Command Tree and the Input Command keywords */
		if ((iswhitespace(Ch)) || !Ch)						/* If found whitespace or reached end of string */
			break;																	/* then reached end of keywords sequence        */
		Len++;
	} while (Len);															/* Stop looping if Len overflows back to 0 */
	return Len;
}


/**************************************************************************************/
/* Determines if there is an exact match between the keywords of a Command Spec or a	*/
/* Character Data Spec and the keywords of the Input Command.													*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SSpec							- Start of Specification string being compared						*/
/*	[in] LenSpec						- Length of Specification string (>=1)										*/
/*	[in] SInp								- Pointer to start of part of Input Command being compared*/
/*	[in] LenInp							- Length of part of Input Command													*/
/*	[in] eKeyword						- Type of keywords being compared - Command or Char Data	*/
/*  ONLY PRESENT IF SUPPORT_NUM_SUFFIX IS DEFINED:																		*/
/*	[out] uiNumSuf					- Array [0..MAX_NUM_SUFFIX-1] of returned numeric 				*/
/*														suffices present	in the Input Command.									*/
/*	[in/out] *pNumSuf				- Pointer to index of first element in uiNumSuf array to 	*/
/*														populate; if this function returns error code						*/
/*                            SCPI_ERR_NONE then this is returned as next element to	*/
/*														populate, otherwise it is returned unchanged.						*/
/*  [out] *pSizeTree        - Pointer to number of chars in the command spec that			*/
/*														comprise the command tree (only populated if 						*/
/*														SUPPORT_OPT_NODE_ORIG is not #defined)									*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						 - OK:		Match found																			*/
/*	SCPI_ERR_INVALID_CMD 		 - Error: No match found																	*/
/*	SCPI_ERR_TOO_MANY_NUM_SUF- Error:	Too many numeric suffices in Input Command to		*/
/*																		be returned in return parameter array.					*/
/*	SCPI_ERR_NUM_SUF_INVALID - Error: Numeric suffix in Input Command is invalid			*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
UCHAR SCPIParser::KeywordsMatchSpec (const char *SSpec, SCPI_CHAR_IDX LenSpec, char *SInp, SCPI_CHAR_IDX LenInp, enum enKeywordType eKeyword, unsigned int uiNumSuf[],
 UCHAR *pNumSuf, SCPI_CHAR_IDX *pSizeTree)
#else
UCHAR SCPIParser::KeywordsMatchSpec (const char *SSpec, SCPI_CHAR_IDX LenSpec, char *SInp, SCPI_CHAR_IDX LenInp, enum enKeywordType eKeyword, SCPI_CHAR_IDX *pSizeTree)
#endif
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPI_CHAR_IDX PosSpec = 0;
	SCPI_CHAR_IDX PosInp = 0;
	SCPI_CHAR_IDX OldPosInp;
	char ChSpec;
	char ChInp;
	BOOL bOptional = FALSE;
	BOOL bLongForm = FALSE;
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR NumSufIndex = *pNumSuf;
	BOOL bNumSufBegun = FALSE;
#endif
#ifndef SUPPORT_OPT_NODE_ORIG
  SCPI_CHAR_IDX PosInpTree = 0;
	*pSizeTree = 0;															/* Reset size of Command Tree for following command */
#endif

	if (eKeyword == KEYWORD_COMMAND)						/* If keyword being matched is from the command keywords */
		if (SInp[0] == CMD_COMMON_START)					/* If Input Command is a common command (its first char being an asterisk) */
			PosInp += mCommandTreeLen;							/* then ignore the current Command Tree, since does not apply to common commands */

	/* Loop while a match is possible */
	while (Err != SCPI_ERR_INVALID_CMD)
	{
		if (eKeyword == KEYWORD_COMMAND)					/* If keyword being matched is from the command keywords  */
			ChInp = CharFromFullCommand (SInp, PosInp, LenInp);
																							/* Get character from Input Command (including Command Tree) */
		else																			/* If keyword being matched is in the parameter section */
		{
			if (PosInp < LenInp)										/* If still within Input Command 							*/
				ChInp = SInp[PosInp];									/* then get character from Input Command 			*/
			else
				ChInp = '\0';													/* otherwise treat character as end-of-string */
		}

		if (PosSpec < LenSpec)										/* If still within Specification 		 */
			ChSpec = SSpec[PosSpec];								/* read character from Specification */
		else
			break;																	/* otherwise exit the loop 					 */

#ifndef SUPPORT_OPT_NODE_ORIG
		if (eKeyword == KEYWORD_COMMAND)					/* If keyword being matched is from the command keywords  */
			if ((ChInp == KEYW_SEP) && ((ChSpec == KEYW_SEP) || (ChSpec == '[')))	/* If got to end of keyword in Input Command */
																																						/* and got to end of keyword in Spec */
				if (PosInp != PosInpTree)							/* If also moved on in the Input Command since last time here */
				{
					*pSizeTree = PosSpec;								/* Store current position in Spec as the length of the Command Tree */
					PosInpTree = PosInp;								/* Remember current position in Input Command */
				}
#endif

		if (ChSpec == '[')												/* If found beginning of optional keyword in Spec 				*/
		{
			bOptional = TRUE;												/* Flag the current keyword in Spec as optional 					*/
			OldPosInp = PosInp;											/* Remember position in Input Command where this happend 	*/
			PosSpec++;															/* Go to next character in Spec														*/
			continue;
		}

		if (ChSpec == ']')												/* If found end of optional keyword in Spec		*/
		{
			bOptional = FALSE;											/* Flag current keyword in Spec as required		*/
			PosSpec++;															/* Go to next character in Spec								*/
			continue;
		}

#ifdef SUPPORT_NUM_SUFFIX
		if (ChSpec == NUM_SUF_SYMBOL)							/* If found numeric suffix symbol in Specification */
		{
			if (NumSufIndex < MAX_NUM_SUFFIX)				/* If not too many numeric suffices in Specification */
			{
				if (isdigit (ChInp))									/* If input character is a decimal digit */
				{
					if (!bNumSufBegun)									/* If this is the start of a new numeric suffix */
						uiNumSuf[NumSufIndex] = 0;				/* then initialise it to zero				 						*/

					bNumSufBegun = TRUE;								/* Set flag to indicate have begun parsing numeric suffix digits */

					if (!AppendToUInt (&(uiNumSuf[NumSufIndex]), ChInp-'0'))	/* If unable to append input digit to numeric suffix */
						Err = SCPI_ERR_NUM_SUF_INVALID;													/* then it is invalid 															 */

					PosInp++;														/* Go to next character in Input Command */
					continue;
				}
				else																	/* If input character is not a decimal digit */
				{
					if (!bNumSufBegun)															/* If never started reading digits of numeric suffix  */
						uiNumSuf[NumSufIndex] = NUM_SUF_DEFAULT_VAL;	/* then return it as the default numeric suffix value */
					else
						if ((uiNumSuf[NumSufIndex] < NUM_SUF_MIN_VAL) || (uiNumSuf[NumSufIndex] > NUM_SUF_MAX_VAL))
																							/* If numeric suffix is outside allowed values */
							Err = SCPI_ERR_NUM_SUF_INVALID;	/* then it is invalid 												 */

					bNumSufBegun = FALSE;								/* Clear flag for next time */
					NumSufIndex++;											/* Increment numeric suffix counter */
					PosSpec++;													/* Go to next character in Specification */
					continue;
				}
			}
			else																		/* If there are more '#'s in the specification than allowed */
			{
				Err = SCPI_ERR_TOO_MANY_NUM_SUF;			/* then return error code																		*/
				break;
			}
		}
#endif

		if (ChSpec == KEYW_SEP)										/* If found keyword separator in Specification														*/
			bLongForm = FALSE;											/* then this is a new keyword starting so reset "Long-Form required" flag */

		if (tolower (ChSpec) == tolower (ChInp))	/* If characters in Spec and Input Command match */
		{
			if (islower (ChSpec))										/* If character in Spec is only applicable to Long Form */
				bLongForm = TRUE;											/* then we now require the Long Form from the Input Command */

			PosSpec++;															/* Go to next character in Spec 					*/
			PosInp++;																/* Go to next character in Input Command 	*/
		}

		else																			/* If characters in Spec and Input Command do not match */
		{
			if (islower (ChSpec))										/* If Spec character is only applicable to Long Form */
			{
				if (!bLongForm)												/* If we do not require Long Form from the Input Command */
				{
					PosSpec = SkipToNextRequiredChar (SSpec, PosSpec); /* then move to next required character in Specification */
				}
				else																	/* If we require Long Form from the Input Command */
				{
					if (bOptional)											/* If keyword in the Specification is optional */
					{
						PosInp = OldPosInp;								/* Revert to character position in Input Command before attempting
																									 to match with the optional keyword															 			*/
#ifdef SUPPORT_NUM_SUFFIX
						PosSpec = SkipToEndOfOptionalKeyword (SSpec, PosSpec, &NumSufIndex, uiNumSuf);
#else
						PosSpec = SkipToEndOfOptionalKeyword (SSpec, PosSpec);
																							/* and move position in Command Spec to the end of the optional keyword */
#endif
					}
					else																/* If keyword in Spec is not optional									 				 */
					{
						Err = SCPI_ERR_INVALID_CMD;				/* then this is a definte mis-match, so set flag and exit loop */
					}
				}
			}
			else																		/* If character in Spec is required for Long or Short Form */
			{
				if (bOptional)												/* If keyword in Command Spec is optional */
				{
					PosInp = OldPosInp;									/* Revert to character position in Input Command before attempting
																								 to match with the optional keyword															 */
#ifdef SUPPORT_NUM_SUFFIX
					PosSpec = SkipToEndOfOptionalKeyword (SSpec, PosSpec, &NumSufIndex, uiNumSuf);
#else
					PosSpec = SkipToEndOfOptionalKeyword (SSpec, PosSpec);
																							/* and move position in Command Spec to the end of the optional keyword */
#endif
				}
				else																	/* If keyword in Spec is not optional 								 					*/
					Err = SCPI_ERR_INVALID_CMD;					/* then this is a definite mis-match, so set flag and exit loop */
			}
		}
	}

	if (ChInp && (Err != SCPI_ERR_TOO_MANY_NUM_SUF))	/* If not reached the end of the keyword characters in Input Command */
		Err = SCPI_ERR_INVALID_CMD;											/* then this can't be a match																		 		 */

#ifdef SUPPORT_NUM_SUFFIX
	if (Err == SCPI_ERR_NONE)										/* If no error occurred 																		*/
	{
		*pNumSuf = NumSufIndex;										/* then return new index of next numeric suffix to populate */
		for (; NumSufIndex != 0; NumSufIndex--)
			muiNumSufPrev[NumSufIndex-1] = uiNumSuf[NumSufIndex-1];		/* Store numeric suffices for use in next command (if using same tree) */
	}
#endif

	return Err;
}


/**************************************************************************************/
/* Given the current position within the command keywords of a Command Specification,	*/
/* returns the position of the next character that is required for a match						*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SSpec 			-		Pointer to first char in command keywords of Command Spec			*/
/*	[in] Pos				-		Current position (where 0 is first character) within					*/
/*											command	keywords of Command Spec															*/
/*																																										*/
/* Return Value:																																			*/
/*	Position of first character in command keywords after end of the present keyword	*/
/**************************************************************************************/
SCPI_CHAR_IDX SCPIParser::SkipToNextRequiredChar (const char *SSpec, SCPI_CHAR_IDX Pos)
{
	char Ch;

	do
	{
		Pos++;
		Ch = SSpec[Pos];													/* Get character from specification */
	} while (islower (Ch));											/* Loop while character is only required in Long Form */

	return Pos;
}


/**************************************************************************************/
/* Given the current position within the command keywords of a Command Spec,					*/
/* returns position of the first char after the end of the present optional keyword.	*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SSpec 			-		Pointer to first char in command keywords of Command Spec			*/
/*	[in] Pos				-		Current position (where 0 is first char) within								*/
/*											command keywords of Command Spec															*/
/*  [in/out] pNumSufIndex - Pointer to value of numeric suffix index. Incremented if	*/
/*											a numeric suffix is found before end of optional keyword			*/
/*	[in/out] uiNumSuf[] - Array of numeric suffices, appended to with default value 	*/
/*											if numeric suffix found before end of optional keyword				*/
/*																																										*/
/* Return Value:																																			*/
/*	Position of first character in command keywords after the end of the present			*/
/*  optional keyword																																	*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
SCPI_CHAR_IDX SCPIParser::SkipToEndOfOptionalKeyword (const char *SSpec, SCPI_CHAR_IDX Pos, UCHAR *pNumSufIndex, unsigned int uiNumSuf[])
#else
SCPI_CHAR_IDX SCPIParser::SkipToEndOfOptionalKeyword (const char *SSpec, SCPI_CHAR_IDX Pos)
#endif
{
	char Ch;

	do
	{
		Pos++;
		Ch = SSpec[Pos];													/* Get character from specification */
#ifdef SUPPORT_NUM_SUFFIX
		if (Ch == NUM_SUF_SYMBOL)									/* If numeric suffix is in specification */
		{
			uiNumSuf[*pNumSufIndex] = NUM_SUF_DEFAULT_VAL;	/* Add default numeric suffix to list of suffices */
			(*pNumSufIndex) ++;											/* Increment count of numeric suffices */
		}
#endif
	} while ((Ch != ']') && Ch);								/* Loop until closing square bracket or end of string */

	return Pos;
}


/**************************************************************************************/
/* Determines if the parameter count of an Input Command is a valid match for a 			*/
/* Command Spec																																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] CmdSpecNum -				Number of Command Spec being matched to Input Command			*/
/*	[in] InpCmdParamsCnt -	Number of parameters in the Input Command									*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE					- OK:			Parameter count is a valid match									*/
/*	SCPI_ERR_PARAM_COUNT	- Error:	Parameter count is not a valid match							*/
/**************************************************************************************/
UCHAR SCPIParser::MatchesParamsCount (SCPI_CMD_NUM CmdSpecNum, UCHAR InpCmdParamsCnt)
{
	UCHAR Err;
	UCHAR ParIdx;
	UCHAR MinCnt = 0;
	UCHAR MaxCnt = 0;
	const struct strSpecParam *psSpec;

	for (ParIdx = 0; (ParIdx < MAX_PARAMS); ParIdx++) /* Loop thru all the Command Spec's parameters */
	{
		psSpec = &(msSpecCommand[CmdSpecNum].sSpecParam[ParIdx]);	/* Create pointer to Parameter Spec of Command Spec */
		if (psSpec->eType == P_NONE)							/* If Parameter Spec is type No Parameter 							*/
			break;																	/* then reached end of list of parameters, so exit loop */
		if (psSpec->bOptional)										/* If parameter is optional																		 */
			MaxCnt = ParIdx + 1;										/* then this is currently the maximum possible parameter count */

		else																			/* If this parameter is required																	 */
		{
			MinCnt = ParIdx + 1;										/* then all parameters up to this one are also required						 */
			MaxCnt = MinCnt;												/* and this is also currently the maximum possible parameter count */
		}
	}

	/* At this point, MinCnt is the minimum number of parameters the Command Spec allows,
		 and MaxCnt is the maximum number of parameters the Command Spec allows.						*/

	if ((InpCmdParamsCnt >= MinCnt) && (InpCmdParamsCnt <= MaxCnt)) /* If parameter count of Input Command is within
																																		 the allowable range of the Command Spec			 */
		Err = SCPI_ERR_NONE;																					/* then return no error													 */
	else
		Err = SCPI_ERR_PARAM_COUNT;																		/* otherwise return error code									 */

	return Err;
}


/**************************************************************************************/
/* Returns information on the Input Parameters within an Input Command.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SInpCmd -					 Pointer to Input Command																	*/
/*	[in] InpCmdLen -				 Length of Input Command																	*/
/*	[in] InpCmdKeywordsLen - Length of command keywords in Input Command, including		*/
/*													 Command Tree																							*/
/*	[out] pParamsCnt -			 Pointer to returned number of Input Parameters						*/
/*	[out] SParams -					 Returned pointer to first character of the Input					*/
/*													 Parameters string within Input Command										*/
/*	[out] pParamsLen -			 Pointer to returned variable containing length of Input	*/
/*													 Parameters string within Input Command										*/
/**************************************************************************************/
void SCPIParser::GetParamsInfo (char *SInpCmd, SCPI_CHAR_IDX InpCmdLen, SCPI_CHAR_IDX InpCmdKeywordsLen,
 UCHAR *pParamsCnt, char **SParams, SCPI_CHAR_IDX *pParamsLen)
{
	SCPI_CHAR_IDX ParamsStart;
	SCPI_CHAR_IDX Pos = 0;
	BOOL bWithinQuotes = FALSE;
	BOOL bDoubleQuotes = FALSE;
	UCHAR Brackets = 0;
	char Ch;

	ParamsStart = InpCmdKeywordsLen - mCommandTreeLen;	/* Calculate start position of Input Parameters string */

	*SParams = &(SInpCmd[ParamsStart]);					/* Set returned pointer to start of Input Parameters string */
	*pParamsLen = InpCmdLen - ParamsStart;			/* Set returned value of Input Parameters string length */

	*pParamsCnt = 0;

	/* Find start of first Input Parameter within Input Parameters string, if any */
	while (Pos < *pParamsLen)
	{
		if (!iswhitespace((*SParams)[Pos]))				/* If encountered a non-whitespace character */
		{
			*pParamsCnt = 1;												/* then found a parameter */
			break;																	/* and exit this loop			*/
		}
		Pos++;
	}

	/* Count subsequent Input Parameters by counting commas (don't count ones within quotes or brackets) */
	for (; Pos < *pParamsLen; Pos++)
	{
		Ch = (*SParams)[Pos];
		switch (Ch)
		{
			case DOUBLE_QUOTE :												/* Double-quote encountered */
				if (!(bWithinQuotes && !bDoubleQuotes)) /* If it is not embedded within single-quotes */
				{
					bWithinQuotes = !bWithinQuotes;				/* then toggle the in/out-of-quotes state 		*/
					bDoubleQuotes = TRUE;
				}
				break;
			case SINGLE_QUOTE :												/* Single-quote encountered */
				if (!(bWithinQuotes && bDoubleQuotes))	/* If it is not embedded within double-quotes	*/
				{
					bWithinQuotes = !bWithinQuotes;				/* then toggle the in/out-of-quotes state			*/
					bDoubleQuotes = FALSE;
				}
				break;
			case OPEN_BRACKET :											/* Opening bracket encountered */
				Brackets++;														/* Increment bracket nesting level counter */
				break;
			case CLOSE_BRACKET :										/* Closing bracket encountered */
				if (Brackets)
					Brackets--;													/* Decrement bracket nesting level counter, not further than 0 */
				break;
			case ',' :															/* If encounted a comma 							*/
				if (!bWithinQuotes && !Brackets)			/* that is outside quotes and brackets*/
					(*pParamsCnt)++;										/* then increment the parameter count	*/
		}
	}
}


/**************************************************************************************/
/* Translates the Input Parameters according to a Command Spec.												*/
/* If translation succeeds, then returns populated parameter structures.							*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] CmdSpecNum			- Number of Command Spec to use for translation								*/
/*	[in] SInpParams			- Pointer to first character of Input Parameters string				*/
/*	[in] InpParamsLen		- Number of characters in Input Parameters string							*/
/*  [in] InpParamsCnt		- Number of Input Parameters																	*/
/*	[out] sParam				- Array [0..MAX_PARAM-1] of returned parameter structures			*/
/*												(contents of structures is undefined if error code returned)*/
/*  ONLY PRESENT IF SUPPORT_NUM_SUFFIX IS DEFINED:																		*/
/*	[out] uiNumSuf			- Array [0..MAX_NUM_SUFFIX-1] of returned numeric suffices		*/
/*												present	in the Input Command.																*/
/*	[in/out]  pNumSuf		- Pointer to index of first element in uiNumSuf array to			*/
/*												populate. Returned as pointer to index of next element.			*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE 					-	OK: 		Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	One or more of the Input Parameters was the 		*/
/*																		wrong type for the Command Spec									*/
/*	SCPI_ERR_PARAM_OVERFLOW	- Error:	An overflow occurred while parsing a						*/
/*																		numeric value (exponent > +/-43)								*/
/*	SCPI_ERR_TOO_MANY_NUM_SUF- Error:	Too many numeric suffices in Input Parameter to	*/
/*																		be returned in return parameter array.					*/
/*	SCPI_ERR_UNMATCHED_BRACKET-Error: Unmatched bracket in Input Parameter						*/
/*	SCPI_ERR_UNMATCHED_QUOTE- Error:	Unmatched quote (single/double) in Input Param	*/
/*	SCPI_ERR_NUM_SUF_INVALID- Error:  Numeric suffix in Input Parameter is invalid		*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in a numeric/channel list is	*/
/*																		invalid, e.g.	floating point when not allowed		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
UCHAR SCPIParser::TranslateParameters (SCPI_CMD_NUM CmdSpecNum, char *SInpParams,
 SCPI_CHAR_IDX InpParamsLen, UCHAR InpParamsCnt, SCPIParam sParam[], unsigned int uiNumSuf[], UCHAR *pNumSuf)
#else
UCHAR SCPIParser::TranslateParameters (SCPI_CMD_NUM CmdSpecNum, char *SInpParams,
 SCPI_CHAR_IDX InpParamsLen, UCHAR InpParamsCnt, SCPIParam sParam[])
#endif
{
	UCHAR Err = SCPI_ERR_NONE;
	const struct strSpecParam *psSpecParam;
	UCHAR ParamIdx;
	SCPI_CHAR_IDX ParamLen;
	SCPI_CHAR_IDX ParamStart;
	SCPI_CHAR_IDX Pos = 0;
	char Ch;
	UCHAR State;
	UCHAR Brackets;

	/* Initially set all returned parameters to type No Parameter */
	for (ParamIdx = 0; ParamIdx < MAX_PARAMS; ParamIdx++)
		sParam[ParamIdx].setType(P_NONE);

	/* Loop once for each possible parameter, or until an error occurs */
	for (ParamIdx = 0; (ParamIdx < MAX_PARAMS) && (Err == SCPI_ERR_NONE); ParamIdx++)
	{
		psSpecParam = &(msSpecCommand[CmdSpecNum].sSpecParam[ParamIdx]); /* Set pointer to Parameter Spec in Command Spec */

		if (psSpecParam->eType == P_NONE)					/* If reached end of the parameters in the Parameter Spec */
			break;																	/* then stop translation																	*/

		Err = SCPI_ERR_NONE;
		Brackets = 0;															/* Reset bracket nesting level count */

		/* FSM (Finite State Machine) to parse a single Input Parameter in Input Parameters string */
		/* Refer to Design Notes for a description of this FSM																		 */
		State = 0;

		while (State != 9)												/* Loop until reached Exit state */
		{
			switch (State)
			{
				/* State 0 - Awaiting first character of parameter */
				case 0:																/* Start state */
					ParamStart = Pos;										/* Initially, set start of Input Parameter to current position in string */
					ParamLen = 0;												/* and set parameter length to zero */

					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						State = 9;												/* go to Exit state														 */
						break;
					}
					Ch = SInpParams[Pos];								/* Read character at current position in Input Parameters string */
					switch (Ch)													/* Examine character */
					{
						case PARAM_SEP:
							State = 5;											/* Parameter separator - go to state 5 */
							break;
						case SINGLE_QUOTE:								/* Opening single quote -	go to state 2 */
							State = 2;
							break;
						case DOUBLE_QUOTE:								/* Opening double quote - go to state 3 */
							State = 3;
							break;
						case OPEN_BRACKET:								/* Opening bracket */
							State = 6;											/* Go to state 6 */
							break;
						case CLOSE_BRACKET:								/* Closing bracket */
							Err = SCPI_ERR_UNMATCHED_BRACKET;	/* It is invalid to start parameter with a closing bracket */
							State = 9;												/* so exit FSM */
							break;
						default:
							if (iswhitespace (Ch))					/* If whitespace */
								State = 1;										/* go to state 1 */
							else														/* If any other character (i.e. a part of the parameter) */
								State = 4;										/* go to state 4 */
					}
					break;

				/* State 1 - Within leading whitespace */
				case 1:
					Pos++;															/* Go to next position in Input Parameters string */
					State = 0;													/* Always go back to state 0 */
					break;

				/* State 2 - Inside single quotes	*/
				case 2:
					ParamLen++;													/* Increment length of Input Parameter found */
					Pos++;															/* Go to next char position in Input Parameters string */
					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						Err = SCPI_ERR_UNMATCHED_QUOTE;		/* Error - no matching quote found within string */
						State = 9;												/* go to Exit state */
						break;
					}
					switch (SInpParams[Pos])
					{
						case SINGLE_QUOTE:								/* Closing single quote - go to state 4 */
							State = 4;
							break;
						default:													/* Any other character, just repeat this state */
							break;
					}
					break;

				/* State 3 - Inside double quotes	*/
				case 3:
					ParamLen++;													/* Increment length of Input Parameter found */
					Pos++;															/* Go to next char position in Input Parameters string */
					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						Err = SCPI_ERR_UNMATCHED_QUOTE;		/* Error - no matching quote found within string */
						State = 9;												/* go to Exit state */
						break;
					}
					switch (SInpParams[Pos])
					{
						case DOUBLE_QUOTE:
							State = 4;											/* Closing double quote - go to state 4 */
							break;
						default:
							break;													/* Any other character, just repeat this state */
					}
					break;

				/* State 4 - In characters of parameter (not within quotes) */
				case 4:
					ParamLen++;													/* Increment length of Input Parameter found */
					Pos++;															/* Go to next char position in Input Parameters string */
					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						State = 9;												/* go to Exit state */
						break;
					}
					switch (SInpParams[Pos])
					{
						case PARAM_SEP:										/* Parameter separator (comma) - go to state 5 */
							State = 5;
							break;
						case SINGLE_QUOTE:								/* Opening single quote - go to state 2 */
							State = 2;
							break;
						case DOUBLE_QUOTE:								/* Opening double quote - go to state 3 */
							State = 3;
							break;
						case OPEN_BRACKET:								/* Opening bracket */
							State = 6;
							break;
						case CLOSE_BRACKET:									/* Closing bracket */
							Err = SCPI_ERR_UNMATCHED_BRACKET;	/* Error - closing bracket has no matching opening bracket */
							State = 9;												/* and exit */
							break;
						default:													/* Any other character, just repeat this state */
							break;
					}
					break;

				/* State 5 - Parameter delimiter (comma) encountered */
				case 5:
					Pos++;															/* Go to next char position in Input Parameters string */
					State = 9;													/* Go to Exit state */
					break;

				/* State 6 - Opening bracket encountered */
				case 6:
					Brackets++;													/* Increment bracket nesting level count */
					ParamLen++;													/* Increment length of Input Parameter found */
					Pos++;															/* Go to next char position in Input Parameters string */
					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						Err = SCPI_ERR_UNMATCHED_BRACKET;	/* Error - opening bracket has no matching closing bracket */
						State = 9;												/* go to Exit state */
						break;
					}
					switch (SInpParams[Pos])
					{
						case OPEN_BRACKET:								/* Opening bracket */
							break;													/* then repeat this state */
						case CLOSE_BRACKET:								/* Closing bracket */
							State = 7;
							break;
						default:													/* Any other character */
							State = 8;
							break;
					}
					break;

				/* State 7 - Closing bracket encountered within brackets */
				case 7:
					Brackets--;													/* Decrement bracket nesting level count */
					if (!Brackets)											/* If no longer within brackets */
						State = 4;
					else
						State = 8;
          break;

				/* State 8 - Within brackets */
				case 8:
					ParamLen++;													/* Increment length of Input Parameter found */
					Pos++;															/* Go to next char position in Input Parameters string */
					if (Pos >= InpParamsLen)						/* If gone past end of Input Parameters string */
					{
						Err = SCPI_ERR_UNMATCHED_BRACKET;	/* Error - opening bracket has no matching closing bracket */
						State = 9;												/* go to Exit state */
						break;
					}
					switch (SInpParams[Pos])
					{
						case OPEN_BRACKET:								/* Opening bracket */
							State = 6;
							break;
						case CLOSE_BRACKET:								/* Closing bracket */
							State = 7;
							break;
						default:													/* Any other character */
							break;													/* then repeat state */
					}
					break;
			}
		};

		if (Err == SCPI_ERR_NONE)									/* If no error so far */
		{
			/* At this point: SInpParams[ParamStart] is the first character of the Input Parameter (no leading whitespace)
												ParamLen is number of characters in Input Parameter, including trailing whitespace					 */

			/* Trim trailing whitespace from Input Parameter */
			while (ParamLen > 0)
				if (iswhitespace (SInpParams[ParamStart + ParamLen - 1]))
					ParamLen--;
				else
					break;

			/* Note: ParamLen is zero if Input Parameter is zero length or Input Parameter is simply not present */

			/* Attempt to translate the Input Parameter according to its type in the Parameter Spec */
			switch (psSpecParam->eType)
			{
				case P_CH_DAT :
#ifdef SUPPORT_NUM_SUFFIX
					Err = TranslateCharDataParam (&(SInpParams[ParamStart]), ParamLen, psSpecParam, &(sParam[ParamIdx]), uiNumSuf, pNumSuf);
#else
					Err = TranslateCharDataParam (&(SInpParams[ParamStart]), ParamLen, psSpecParam, &(sParam[ParamIdx]));
#endif
					break;
				case P_BOOL :
					Err = TranslateBooleanParam (&(SInpParams[ParamStart]), ParamLen, (const struct strSpecAttrBoolean *)(psSpecParam->pAttr), &(sParam[ParamIdx]));
					break;
				case P_NUM :
					Err = TranslateNumericValueParam (&(SInpParams[ParamStart]), ParamLen, (const struct strSpecAttrNumericVal *)(psSpecParam->pAttr), &(sParam[ParamIdx]));
					break;
				case P_STR :
					Err = TranslateStringParam(&(SInpParams[ParamStart]), ParamLen, psSpecParam->eType, &(sParam[ParamIdx]));
					break;
				case P_UNQ_STR :
					if (ParamIdx >= InpParamsCnt)
					{	/* Input parameters do not include trailing optional parameter of type unquoted string */
						sParam[ParamIdx].setType(P_NONE);
						Err = SCPI_ERR_NONE;
					}
					else
					{
						Err = TranslateStringParam (&(SInpParams[ParamStart]), ParamLen, psSpecParam->eType, &(sParam[ParamIdx]));
					}
					break;
#ifdef SUPPORT_EXPR
				case P_EXPR :
					Err = TranslateExpressionParam (&(SInpParams[ParamStart]), ParamLen, &(sParam[ParamIdx]));
					break;
#endif
#ifdef SUPPORT_NUM_LIST
				case P_NUM_LIST :
					Err = TranslateNumericListParam (&(SInpParams[ParamStart]), ParamLen, (const struct strSpecAttrNumList *)(psSpecParam->pAttr), &(sParam[ParamIdx]));
					break;
#endif
#ifdef SUPPORT_CHAN_LIST
				case P_CHAN_LIST :
					Err = TranslateChannelListParam (&(SInpParams[ParamStart]), ParamLen, (const struct strSpecAttrChanList *)(psSpecParam->pAttr), &(sParam[ParamIdx]));
					break;
#endif
				default:																		/* This should never happen, unless Command Spec is defined wrongly */
					Err = SCPI_ERR_PARAM_TYPE;								/* Return error code if it does occur 															*/
			}
			if ((Err != SCPI_ERR_NONE) &&									/* Input Parameter could not be translated													*/
			 !ParamLen && psSpecParam->bOptional)					/* and the Input Parameter is empty and Command Spec says parameter */
					Err = SCPI_ERR_NONE;											/* is optional, so this is NOT an error condition										*/
		}
	}
	return Err;
}


/**************************************************************************************/
/* Translates an Input Parameter string into a Character Data parameter, translated		*/
/* according to a Parameter Spec.																											*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[in]	psSpecParam	- Pointer to Parameter Spec																			*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*  ONLY PRESENT IF SUPPORT_NUM_SUFFIX IS DEFINED:																		*/
/*	[out] uiNumSuf		- Array [0..MAX_NUM_SUFFIX-1] of returned numeric suffices			*/
/*											present	in the Input Command.																	*/
/*	[in/out] pNumSuf	- Pointer to index of first element of uiNumSuf array to				*/
/*											populate. Returned as pointer to index of next element.				*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																		Parameter Spec																	*/
/*	SCPI_ERR_PARAM_OVERFLOW	- Error:	The Input Parameter was a numeric value, 				*/
/*																		allowed by the Parameter Spec, but the value		*/
/*																		overflowed (exponent was greater than +/-43)		*/
/*	SCPI_ERR_TOO_MANY_NUM_SUF- Error:	Too many numeric suffices in Input Command to		*/
/*																		be returned in return parameter array.					*/
/*	SCPI_ERR_NUM_SUF_INVALID- Error:  Numeric suffix in Input Command is invalid 			*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in a numeric/channel list is	*/
/*																		invalid, e.g.	floating point when not allowed		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
#ifdef SUPPORT_NUM_SUFFIX
UCHAR SCPIParser::TranslateCharDataParam (char *SParam, SCPI_CHAR_IDX ParLen,
	const struct strSpecParam *psSpecParam, SCPIParam *psParam, unsigned int uiNumSuf[], UCHAR *pNumSuf)
#else
UCHAR SCPIParser::TranslateCharDataParam (char *SParam, SCPI_CHAR_IDX ParLen,
	const struct strSpecParam *psSpecParam, SCPIParam *psParam)
#endif
{
	UCHAR RetErr;
	UCHAR Err;
	const struct strSpecAttrCharData *pSpecAttr;
	UCHAR ItemNum;
	SCPI_CHAR_IDX ItemStart;
	SCPI_CHAR_IDX ItemLen;
	SCPI_CHAR_IDX SeqLen;
	const char *SSeq;
	char SeqCh;
	BOOL bMatch = FALSE;
	BOOL bItemEnded;
	enum enParamType eAlt;
	SCPI_CHAR_IDX TreeSize;

	pSpecAttr = ((const struct strSpecAttrCharData *)(psSpecParam->pAttr));	/* Set pointer to Parameter Spec
																																						 Attributes structure						*/

	if (ParLen == 0)														/* If Input Parameter string is zero-length */
	{
		if (pSpecAttr->DefItemNum != C_NO_DEF)		/* If Parameter Spec has a default Character Data Item Number */
		{
			bMatch = TRUE;													/* then this is a valid translation */
			ItemNum = pSpecAttr->DefItemNum;				/* and Item Number returned is the default Item Number */
			Err = SCPI_ERR_NONE;
		}
		else																			/* If Parameter Spec has no default 								 */
			Err = SCPI_ERR_INVALID_CMD;							/* then set error flag to force other match attempts */
	}

	else																				/* If Input Parameter string is not zero-length */
	{
		SSeq = pSpecAttr->SSeq;										/* Set pointer to Character Data Sequence */
		SeqLen = (SCPI_CHAR_IDX)strlen (SSeq);		/* Get length of sequence */

		ItemNum = 0;
		ItemStart = 0;

		/* Loop thru Character Data Items until reached end of Character Data Sequence */
		while ((ItemStart < SeqLen) && !bMatch)
		{
			ItemLen = 0;														/* Reset length counter of current item */

			bItemEnded = FALSE;

			/* Loop thru characters in Sequence until end of Sequence or end of Item reached */
			while ((ItemStart+ItemLen < SeqLen) && !bItemEnded)
			{
				SeqCh = SSeq[ItemStart+ItemLen];			/* Read character from Character Data Sequence */
				if (SeqCh == '|')											/* If reached end of Character Data Item */
					bItemEnded = TRUE;									/* then flag this and so exit loop 			 */
				else																	/* If still within the same Character Data Item */
					ItemLen++;													/* then increment item length										*/
			};

			if (ItemLen > 0)												/* If item is not zero-length	*/
#ifdef SUPPORT_NUM_SUFFIX
				Err = KeywordsMatchSpec (&(SSeq[ItemStart]), ItemLen, SParam, ParLen, KEYWORD_CHAR_DATA, uiNumSuf, pNumSuf, &TreeSize);
#else
      	Err = KeywordsMatchSpec (&(SSeq[ItemStart]), ItemLen, SParam, ParLen, KEYWORD_CHAR_DATA, &TreeSize);
#endif																				/* then check if input parameter matches character data item specification */
			bMatch = (Err == SCPI_ERR_NONE);				/* A match exists if no error occurred in the match attempt */

			if (Err != SCPI_ERR_NONE)								/* If a match was not found        */
			{
				ItemNum++;														/* then go to next item number 		 */
				ItemStart += ItemLen;									/* and move to start of next item  */
				if (ItemStart < UCHAR_MAX)
					ItemStart++;												/* Also skip leading "|" character */
			}
		}
	}

	if (Err != SCPI_ERR_NONE)										/* If no match was found between Input Parameter and a Character Data Item */
	{
		RetErr = Err;															/* Remember old error code */

		eAlt = pSpecAttr->eAltType;								/* Read the Alternative Parameter Type from the Parameter Spec */
		if (eAlt != P_NONE)												/* If there is an Alternative Parameter Type allowed */
		{
			switch (eAlt)														/* then attempt to translate Input Parameter as that type */
			{
				case P_BOOL :													/* Alternative Parameter Type is Boolean */
					Err = TranslateBooleanParam (SParam, ParLen, (const struct strSpecAttrBoolean *)(pSpecAttr->pAltAttr), psParam);
					break;
				case P_NUM :													/* Alternative Parameter Type is Numeric Value */
					Err = TranslateNumericValueParam (SParam, ParLen,
					 (const struct strSpecAttrNumericVal *)(pSpecAttr->pAltAttr), psParam);
					break;
				case P_STR :													/* Alternative Parameter Type is String or Unquoted String */
				case P_UNQ_STR :
					Err = TranslateStringParam (SParam, ParLen, eAlt, psParam);
					break;
#ifdef SUPPORT_EXPR
				case P_EXPR :
					Err = TranslateExpressionParam (SParam, ParLen, psParam);
					break;
#endif
#ifdef SUPPORT_NUM_LIST
				case P_NUM_LIST :
					Err = TranslateNumericListParam (SParam, ParLen,
					 (const struct strSpecAttrNumList *)(pSpecAttr->pAltAttr), psParam);
					break;
#endif
#ifdef SUPPORT_CHAN_LIST
				case P_CHAN_LIST :
					Err = TranslateChannelListParam (SParam, ParLen,
					 (const struct strSpecAttrChanList *)(pSpecAttr->pAltAttr), psParam);
					break;
#endif
				default:															/* This should never happen, unless the Alternative Parameter Type is */
					Err = SCPI_ERR_PARAM_TYPE;					/* defined wrongly. Return error code																	*/
			}
		}
		else																			/* No match found and no Alternative Parameter Type in Parameter Spec */
			Err = SCPI_ERR_PARAM_TYPE;							/* so error code																											*/

		switch (Err)
		{
			case SCPI_ERR_NONE : RetErr = SCPI_ERR_NONE; break;		/* If a matching parameter type was found then return no error */
			default						 : if (RetErr > Err) RetErr = Err;	/* If no match was found then set returned error code to
																															 the least significant error code encountered (error
																															 values are in order of significance)										*/
		}
	}
	else																				/* If match was found between Input Parameter and a Character Data Item */
	{
		psParam->setType(P_CH_DAT);								/* Populate returned parameter structure with Character Data information */
		psParam->setCharDataItemNum(ItemNum);
		psParam->setCharDataSeq(SSeq);

		RetErr = SCPI_ERR_NONE;
	}

	return RetErr;
}


/**************************************************************************************/
/* Translates an Input Parameter string into a Boolean parameter, translated according*/
/* to a Parameter Spec's Boolean Attributes.																					*/
/* Note: The SCPI standard defines Boolean as ON|OFF|<Numeric Value>.									*/
/*			 In the case of Numeric Value, if the integer conversion of it is non-zero		*/
/*       then the Boolean value is 1 (ON), otherwise the Boolean value is 0 (OFF).		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[in]	psSpecAttr	- Pointer to Parameter Spec's Boolean Attributes								*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the 	*/
/*																		Parameter Spec																	*/
/*	SCPI_ERR_PARAM_OVERFLOW	- Error:	The Input Parameter was a numeric value, 				*/
/*																		but the value	overflowed (exponent was greater	*/
/*																		than +/-43)																			*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateBooleanParam (char *SParam, SCPI_CHAR_IDX ParLen,
 const struct strSpecAttrBoolean *psSpecAttr, SCPIParam *psParam)
{
	UCHAR Err;
	BOOL bValid = FALSE;												/* Flag set to TRUE if Input Parameter is a valid Boolean */
	BOOL bVal;
	double fdVal;

	if (ParLen == 0)														/* If Input Parameter is zero-length */
	{
		if (psSpecAttr->bHasDef)									/* If Parameter Spec Boolean Attributes has default value */
		{
			bValid = TRUE;													/* Then Input Parameter is valid 	*/
			if (psSpecAttr->bDefOn)									/* If default value is On  				*/
				bVal = 1;															/* then Boolean value is 1 				*/
			else
				bVal = 0;															/* otherwise Boolean value is 0 	*/
		}
	}

	else																				/* If Input Parameter is not zero-length */
	{
		/* First check if Input Parameter string matches one of the string representations of a Boolean (ON or OFF) */

		if (StringsEqual (SParam, ParLen, BOOL_ON, BOOL_ON_LEN))	/* If Input Parameter matches "ON" 	*/
		{
			bValid = TRUE;													/* then Input Parameter is valid 		*/
			bVal = 1;																/* and set Boolean value to 1		 		*/
		}
		if (StringsEqual (SParam, ParLen, BOOL_OFF, BOOL_OFF_LEN))	/* If Input Parameter matches "OFF" */
		{
			bValid = TRUE;													/* then Input Parameter is valid 		*/
			bVal = 0;																/* and set Boolean value is 0 			*/
		}

		if (!bValid)															/* If Input Parameter did not match either "ON" or "OFF" */
		{
			Err = TranslateNumericValueParam (SParam, ParLen, &sSpecAttrBoolNum, psParam);
																							/* Try to translate Input Parameter as a Numeric Value */
			if (Err == SCPI_ERR_NONE)								/* If translated succeeded */
			{
				Err = psParam->SCPI_ToDouble (&fdVal);			/* Convert value to a double (double allows greatest range) */
				if (Err == SCPI_ERR_NONE)							/* If conversion went ok */
				{
					bValid = 1;													/* then the Input Parameter is a valid Boolean*/
					if (SCPIround(fdVal))										/* If integer value is non-zero	*/
						bVal = 1;													/* then Boolean value is 1			*/
					else
						bVal = 0;													/* else Boolean value is 0			*/
				}
			}
		}
	}

	if (bValid)																	/* If Input Parameter is a valid Boolean value */
	{
		psParam->setType(P_BOOL);									/* Populate returned parameter structure with */
		psParam->setBooleanVal(bVal);							/* Boolean value information									*/
		Err = SCPI_ERR_NONE;
	}

	else																				/* If Input Parameter is not a valid Boolean value */
		Err = SCPI_ERR_PARAM_TYPE;								/* then return an error code											 */

	return Err;
}


/**************************************************************************************/
/* Translates an Input Parameter string into a Numeric Value parameter, translated		*/
/* according to a Parameter Spec's Numeric Value Attributes 													*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[in]	psSpecAttr	- Pointer to Parameter Spec's Numeric Value Attributes					*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																		Parameter Spec																	*/
/*	SCPI_ERR_PARAM_OVERFLOW	- Error:	The Input Parameter value	overflowed (exponent	*/
/*																		was greater	than +/-43)													*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateNumericValueParam (char *SParam, SCPI_CHAR_IDX ParLen,
 const struct strSpecAttrNumericVal *psSpecAttr, SCPIParam *psParam)
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPI_CHAR_IDX Pos;
	signed char UnitExp;
	int iExp;
	BOOL bNegExp;
	enUnits eUnits;

	Err = psParam->PopulateNumericValue(SParam, ParLen, &Pos);	/* Translate characters into a numeric value */

	if (Err == SCPI_ERR_NONE)										/* If translation succeeded */
	{
		if (Pos < ParLen)													/* If there are characters in the parameter after the number */
		{
			Err = TranslateUnits (&(SParam[Pos]), ParLen-Pos, psSpecAttr, &eUnits, &UnitExp);
																							/* Translate following characters as the units of the parameter */
		}
		else																								/* If number was not followed by any non-numeric characters 			*/
		{
			eUnits = psSpecAttr->eUnits;											/* then use default units from the Parameter Spec	*/
			UnitExp = psSpecAttr->Exp;												/* and use the default unit exponent							*/
		}

    iExp = (int)(psParam->getNumericValExp());	/* Get exponent of numeric value (before adjusted for units) */
    if (psParam->getNumericValNegExp())
    	iExp = 0 - iExp;												/* Make exponent negative if flag is set */

    iExp += (int)UnitExp;											/* Add exponent of units */

		if (iExp > MAX_EXPONENT)									/* If resulting exponent is too big	*/
    {
				Err = SCPI_ERR_PARAM_OVERFLOW;				/* then return overflow error code	*/
    		iExp = 0;
    }
		if (iExp < MIN_EXPONENT)									/* If exponent is too small (i.e. too many decimal places */
		{			 																		/* before the first significant digit)										*/
			psParam->setNumericValSigFigs(0);				/* then the number is effectively zero										*/
			iExp = 0;
		}

		if (iExp < 0)															/* If exponent is negative							*/
		{
			iExp = 0 - iExp;												/* Make it positive											*/
			bNegExp = TRUE;													/* and set flag "exponent is negative"	*/
		}
		else																			/* If exponent is positive						*/
			bNegExp = FALSE;												/* clear flag "exponent is negative"	*/

		/* Populate members of returned numeric value structure */
		psParam->setNumericValExp(cabs((signed char)iExp));
		psParam->setNumericValNegExp(bNegExp);
		psParam->setNumericValUnits(eUnits);
	}

	return Err;
}


/**************************************************************************************/
/* Translates an Input Parameter string into a String or Unquoted String parameter.		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string 															*/
/*	[in]	ePType			- Type of parameter to translate to (String or Unquoted String)	*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_UNMATCHED_QUOTE- Error:  Unmatched quotation mark in Input Parameter			*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																	  type of parameter requested											*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateStringParam (char *SParam, SCPI_CHAR_IDX ParLen, const enum enParamType ePType, SCPIParam *psParam)
{
	UCHAR Err = SCPI_ERR_NONE;
	BOOL bInsideQuotes = TRUE;
	char ChQuote;
	SCPI_CHAR_IDX Pos;

	if (ePType == P_STR)												/* If required parameter type is a quoted string */
	{
		if (ParLen > 0)														/* If parameter is not zero length */
		{
			switch (SParam[0])											/* Look at the first character of the parameter */
			{
				case SINGLE_QUOTE:	ChQuote = SINGLE_QUOTE;	break;	/* Expression delimited by single quotes */
				case DOUBLE_QUOTE:	ChQuote = DOUBLE_QUOTE; break;	/* Expression delimited by double quotes */
				default:						Err = SCPI_ERR_PARAM_TYPE;			/* Any other character is invalid */
			}

			/* Loop through each char in the parameter, or until an error */
			for (Pos = 1; (Pos < ParLen) && (Err == SCPI_ERR_NONE); Pos++)
			{
				if (SParam[Pos] == ChQuote)						/* If a delimiting quote is encountered */
					bInsideQuotes = !bInsideQuotes;			/* then toggle "inside quotes" state 		*/

				else																	/* If encountered any other character */
					if (!bInsideQuotes)									/* and it is not inside quotes 				*/
						Err = SCPI_ERR_PARAM_TYPE;				/* then the parameter is not a valid quoted string */
			}
			if ((Err == SCPI_ERR_NONE) && bInsideQuotes)	/* If still inside quotes at the end of the parameter */
				Err = SCPI_ERR_UNMATCHED_QUOTE;							/* then the quote was not matched to another one		  */
		}
		else																			/* Parameter is zero length						*/
			Err = SCPI_ERR_PARAM_TYPE;							/* so it is not a valid quoted string */
	}

	if (Err == SCPI_ERR_NONE)										/* If tranlsation is valid */
	{
		psParam->setType(ePType);									/* Populate returned parameter structure */

		if (ePType == P_STR)											/* If quoted string 																	*/
		{
			psParam->setString(&(SParam[1])); 			/* Set pointer to first character after opening quote */
			psParam->setStringLen(ParLen - 2);			/* Set length to exclude opening & closing quotes 		*/
			psParam->setStringDelimiter(ChQuote);		/* Set character that was used to delimit the string	*/
		}
		else																			/* If unquoted string 							*/
		{
			psParam->setString(SParam); 						/* Set pointer to start of parameter */
			psParam->setStringLen(ParLen);					/* Set length to length of parameter */
			psParam->setStringDelimiter(0);					/* Clear delimiter - not applicable for unquoted strings */
		}
	}

	return Err;
}


#ifdef SUPPORT_EXPR
/**************************************************************************************/
/* Translates an Input Parameter string into an Expression parameter.									*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE							- OK:			Translation succeeded													*/
/*	SCPI_ERR_UNMATCHED_BRACKET- Error:  Unmatched bracket in Input Parameter					*/
/*	SCPI_ERR_PARAM_TYPE				-	Error:	The Input Parameter could not be translated		*/
/*																	  	as an Expression															*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateExpressionParam (char *SParam, SCPI_CHAR_IDX ParLen, SCPIParam *psParam)
{
	UCHAR Err = SCPI_ERR_NONE;
	UCHAR Brackets = 0;
	SCPI_CHAR_IDX Pos;

	if (ParLen > 0)															/* If parameter is not zero-length */
	{
		/* Loop through each character in Input Parameter, or until an error */
		for (Pos = 0; (Pos < ParLen) && (Err == SCPI_ERR_NONE); Pos++)
		{
			switch (SParam[Pos])
			{
				case OPEN_BRACKET:
					Brackets++;													/* Increment bracket nesting level counter */
					break;
				case CLOSE_BRACKET:
					if (Brackets)												/* If within one bracket or more */
					{
						Brackets--;												/* then decrement the bracket nesting level counter */
						if (!Brackets && (Pos < ParLen-1))	/* If now outside the brackets but there are more characters to go */
							Err = SCPI_ERR_PARAM_TYPE;				/* then this is not a valid expression 										 				 */
					}
					else																/* If not within a bracket */
						Err = SCPI_ERR_UNMATCHED_BRACKET;	/* then this closing bracket is unmatched */
					break;
				default:
					if (!Brackets)											/* If any other charcater encountered outside all brackets */
						Err = SCPI_ERR_PARAM_TYPE;				/* then this is not a valid expression 										 */
			}
		}
		if ((Err == SCPI_ERR_NONE) && Brackets)		/* If at end of parameter the bracket nesting level is not zero */
			Err = SCPI_ERR_UNMATCHED_BRACKET;				/* then there is at least one opening bracket without a matching close */
	}

	else																				/* If parameter is zero-length */
		Err = SCPI_ERR_PARAM_TYPE;								/* then return error code			 */

	if (Err == SCPI_ERR_NONE)										/* If the Input Parameter is a valid Expression */
	{
		psParam->setType(P_EXPR);									/* Populate returned parameter structure */
		psParam->setString(SParam);								/* Set pointer to first character of Input Parameter */
		psParam->setStringLen(ParLen);					/* Set length to length of Input Parameter */
	}

	return Err;
}
#endif


#ifdef SUPPORT_NUM_LIST
/**************************************************************************************/
/* Translates an Input Parameter string into a Numeric List parameter.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[in]	pSpecAttr		- Pointer to numeric list attributes parameter specification		*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																	  type of parameter requested											*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in the list is invalid, i.e.	*/
/*																		floating point when not allowed, out of range		*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateNumericListParam (char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrNumList *pSpecAttr,
 SCPIParam *psParam)
{
	SCPIParam sTestParam;
	long lNum;
	SCPI_CHAR_IDX NextPos;
	SCPI_CHAR_IDX Pos;
	BOOL bRange = FALSE;
	UCHAR Err = SCPI_ERR_NONE;

	if (ParLen > 2)															/* A Numeric List is at least 3 characters long */
	{
		if ((SParam[0] == OPEN_BRACKET) && (SParam[ParLen-1] == CLOSE_BRACKET))	/* Must be enclosed by brackets */
		{
			Pos = 1;
			while ((Pos < ParLen-1) && (Err != SCPI_ERR_PARAM_TYPE))	/* Loop through all characters or until error */
			{
				if (sTestParam.PopulateNumericValue(&(SParam[Pos]), ParLen - Pos, &NextPos) != SCPI_ERR_NONE)
				{																			/* If could not translate as a number 							 */
					Err = SCPI_ERR_PARAM_TYPE;					/* then not a numeric list, so set return error code */
					break;															/* and exit the loop 																 */
				}

				/* Validate number according to parameter specifications */

				if (!(pSpecAttr->bReal))								/* If real (non-integer) numbers are not allowed */
					if (sTestParam.getNumericValNegExp())	/* If number has a negative exponent i.e. has digits after decimal place */
						Err = SCPI_ERR_INVALID_VALUE;				/* then the value is invalid 																						 */

				if (!(pSpecAttr->bNeg))								/* If negative numbers are not allowed */
					if (sTestParam.getNumericValNeg())	/* If number is negative */
						Err = SCPI_ERR_INVALID_VALUE;			/* then the value is invalid	*/

				if (pSpecAttr->bRangeChk)							/* If range checking is required */
				{
					if (sTestParam.SCPI_ToLong(&lNum) == SCPI_ERR_NONE)	/* If value is a valid long number */
					{
						if ((lNum < pSpecAttr->lMin) || (lNum > pSpecAttr->lMax))	/* If value is outside allowed range */
							Err = SCPI_ERR_INVALID_VALUE;														/* then value is invalid 						 */
					}
					else																/* If value is not a valid long integer, e.g. overflows */
						Err = SCPI_ERR_INVALID_VALUE;			/* then value is invalid 																*/
				}

				Pos += NextPos;												/* Move to first character after the number */

				if (Pos < ParLen-1)										/* If there are more characters inside the brackets */
				{
					switch (SParam[Pos])
					{
						case RANGE_SEP:										/* If encountered a range separator character */
							if (!bRange)										/* If not already decoding a range */
								bRange = TRUE;								/* then set range flag 						 */
							else														/* If already in a range					 														*/
								Err = SCPI_ERR_PARAM_TYPE;		/* then error - cannot have two range separators in one entry */
							break;
						case ENTRY_SEP:										/* If encountered an entry separator character */
							bRange = FALSE;									/* Clear range flag */
							break;
						default:													/* Any other character */
							Err = SCPI_ERR_PARAM_TYPE;			/* is an error 				 */
					}
					Pos++;															/* Go to next position in parameter */
					if (Pos == ParLen-1)								/* If reached end of characters within brackets */
						Err = SCPI_ERR_PARAM_TYPE;				/* then error - must finish on a number 				*/
				}
			}
		}
		else
			Err = SCPI_ERR_PARAM_TYPE;							/* Not enclosed by brackets */
	}
	else
		Err = SCPI_ERR_PARAM_TYPE;								/* Too short to be a Numeric List */

	if (Err == SCPI_ERR_NONE)										/* If parameter translated ok as a Numeric List */
	{
		psParam->setType(P_NUM_LIST);							/* Populate returned parameter */
		psParam->setString(&(SParam[1]));					/* Set pointer to first char within brackets */
		psParam->setStringLen(ParLen - 2);				/* Set length to exclude brackets */
	}

	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST
/**************************************************************************************/
/* Translates an Input Parameter string into a Channel List parameter.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in]	ParLen			- Length of Input Parameter string															*/
/*	[in]	pSpecAttr		- Pointer to channel list attributes parameter specification		*/
/*	[out]	psParam			- Pointer to returned parameter structure												*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																	  type of parameter requested											*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in the list is invalid, i.e.	*/
/*																		floating point when not allowed, out of range		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
UCHAR SCPIParser::TranslateChannelListParam (char *SParam, SCPI_CHAR_IDX ParLen, const struct strSpecAttrChanList *pSpecAttr,
 SCPIParam *psParam)
{
	SCPI_CHAR_IDX Pos;
	SCPI_CHAR_IDX PosOrig;
	UCHAR Err = SCPI_ERR_NONE;

	if (ParLen > 3)															/* A Channel List is at least 4 characters long */
	{
		if ((SParam[0] == OPEN_BRACKET) && (SParam[ParLen-1] == CLOSE_BRACKET)	/* Must be enclosed by brackets  */
		 && (SParam[1] == '@'))																									/* and have @ in second position */
		{
			Pos = 2;
			while (Pos < ParLen - 1)								/* Loop through all characters in parameter */
			{
				PosOrig = Pos;												/* Take a copy of position index */
				Err = ParseAsChannelRange (SParam, &Pos, ParLen - 2, TRUE, pSpecAttr);	/* Attempt to parse entry as channel range */
#ifdef SUPPORT_CHAN_LIST_ADV
				if (Err != SCPI_ERR_NONE)
				{
					Pos = PosOrig;											/* Restore position index */
					if (ParseAsModuleChannel (SParam,&Pos, ParLen - 1, TRUE, pSpecAttr) != SCPI_ERR_NONE)	/* Attempt to parse entry as module channel */
					{
						Pos = PosOrig;										/* Restore position index */
						if (ParseAsPathName (SParam, &Pos, ParLen - 2) != SCPI_ERR_NONE)	/* Attempt to parse entry as path name */
							break;													/* If cannot be parsed as a valid channel list entry, then return error (use error code from channel range parsing) */
						else
							Err = SCPI_ERR_NONE;						/* Entry is valid as a path name */
					}
					else
						Err = SCPI_ERR_NONE;							/* Entry is valid as a module channel */
				}
#endif
			}
		}
		else
			Err = SCPI_ERR_PARAM_TYPE;							/* Not enclosed by brackets */
	}
	else
		Err = SCPI_ERR_PARAM_TYPE;								/* Too short to be a Channel List */

	if (Err == SCPI_ERR_NONE)										/* If parameter translated ok as a Channel List */
	{
		psParam->setType(P_CHAN_LIST);						/* Populate returned parameter */
		psParam->setString(&(SParam[2]));					/* Set pointer to first char after '@' symbol */
		psParam->setStringLen(ParLen - 3);				/* Set length to exclude brackets and '@' */
	}
	return Err;
}
#endif


/**************************************************************************************/
/* Resets Command Tree to root																												*/
/**************************************************************************************/
void SCPIParser::ResetCommandTree (void)
{
	mCommandTreeLen = 0;
}


#ifdef SUPPORT_OPT_NODE_ORIG
/**************************************************************************************/
/* Sets Command Tree attributes to the command tree of a Command Spec.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] CmdSpecNum		- Number of Command Spec																				*/
/**************************************************************************************/
void SCPIParser::SetCommandTree (SCPI_CMD_NUM CmdSpecNum)
{
	SCPI_CHAR_IDX TreeSize;
	SCPI_CHAR_IDX Pos;
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR SufIdx = 0;
#endif

	mSCommandTree = mSSpecCmdKeywords[CmdSpecNum];			/* Set start of Command Tree to first character in
																										 	 command keyowrds of Command Spec								*/

	TreeSize = (SCPI_CHAR_IDX)strlen (mSCommandTree);	/* Set size of Command Tree to length of keywords */

	/* Reduce size of Command Tree to exclude last keyword in Command Spec's command keywords */
	while (TreeSize > 0)
	{
		if (mSCommandTree[TreeSize-1] == KEYW_SEP)
			break;
		TreeSize--;
	}

	/* Determine length of Command Tree, where length equals size minus count of square-bracket characters */
	mCommandTreeLen = 0;
	for (Pos = 0; Pos < TreeSize; Pos++)
#ifdef SUPPORT_NUM_SUFFIX
		if (mSCommandTree[Pos] == NUM_SUF_SYMBOL)									/* If encounter a numeric suffic in the command specification */
		{
			mCommandTreeLen += DigitCount (muiNumSufPrev[SufIdx]);	/* Add length of numeric suffix entered previously */
			SufIdx++;
		}
		else
#endif
			if ((mSCommandTree[Pos] != '[') && (mSCommandTree[Pos] != ']'))
				mCommandTreeLen++;
}
#else
/**************************************************************************************/
/* Sets Command Tree attributes to the command tree of a Command Spec.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] CmdSpecNum		- Number of Command Spec																				*/
/*  [in] TreeSize     - Number of chars in Command Spec that comprise the tree				*/
/**************************************************************************************/
void SCPIParser::SetCommandTree (SCPI_CMD_NUM CmdSpecNum, SCPI_CHAR_IDX TreeSize)
{
	SCPI_CHAR_IDX Pos;
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR SufIdx = 0;
#endif

	mSCommandTree = mSSpecCmdKeywords[CmdSpecNum];			/* Set start of Command Tree to first character in
																										 	 command keywords of Command Spec								*/

	mbCommandTreeColonReqd = FALSE;										/* Default to not needing a colon after the command tree */

	if (TreeSize != 0)																/* If tree is not of zero length */
	{
		if (mSSpecCmdKeywords[CmdSpecNum][TreeSize] == KEYW_SEP)	/* If colon present one char after the end of command tree */
			TreeSize++;																						/* then include it in the command tree */

		if (mSSpecCmdKeywords[CmdSpecNum][TreeSize-1] != KEYW_SEP)	/* If last char in command tree is not a colon */
			mbCommandTreeColonReqd = TRUE;													/* then need to add colon after command tree */

		/* Determine length of Command Tree, where length equals size minus count of square-bracket characters */
		mCommandTreeLen = 0;
		for (Pos = 0; Pos < TreeSize; Pos++)
#ifdef SUPPORT_NUM_SUFFIX
			if (mSCommandTree[Pos] == NUM_SUF_SYMBOL)									/* If encounter a numeric suffic in the command specification */
			{
				mCommandTreeLen += DigitCount (muiNumSufPrev[SufIdx]);	/* Add length of numeric suffix entered previously */
				SufIdx++;
			}
			else
#endif
				if ((mSCommandTree[Pos] != '[') && (mSCommandTree[Pos] != ']'))
					mCommandTreeLen++;

		if (mbCommandTreeColonReqd)												/* If a colon is required after the command tree */
			mCommandTreeLen++;															/* then add one to the length */
	}
	else																								/* If tree is of zero length */
		mCommandTreeLen = 0;															/* then store that */
}
#endif


/**************************************************************************************/
/* Returns a character from a position within the Full Command version of an Input		*/
/* Command string (where position 0 is the first character).													*/
/* Note: The Full Command version of the Input Command string is the Command Tree			*/
/* concatenated with the Input Command string.																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] SInpCmd		- Pointer to start of Input Command string												*/
/*	[in] Pos				- Position within Full Command																		*/
/*	[in] Len				- Length of Full Command version of the Input Command string			*/
/*																																										*/
/* Return Value:																																			*/
/*	Character at position, or '\0' if position is beyond end of Full Command version	*/
/*  of Input Command string																														*/
/**************************************************************************************/
UCHAR SCPIParser::CharFromFullCommand (char *SInpCmd, SCPI_CHAR_IDX Pos, SCPI_CHAR_IDX Len)
{
	char Ch;
	SCPI_CHAR_IDX Cnt = 0;
	SCPI_CHAR_IDX j = 0;
#ifdef SUPPORT_NUM_SUFFIX
	UCHAR SufIdx = 0;
	UCHAR SufDig;
#endif

	if (Pos < mCommandTreeLen)									/* If character position is within Command Tree */
	{
#ifndef SUPPORT_OPT_NODE_ORIG
		if (mbCommandTreeColonReqd && (Pos == mCommandTreeLen-1))	/* If colon required at end of Command Tree */
																															/* and position given is position of that colon */
			Ch = KEYW_SEP;																					/* then return a colon */
		else
#endif
		{
			do
			{
				Ch = mSCommandTree[j];								/* Read character from Command Tree */
#ifdef SUPPORT_NUM_SUFFIX
				if (Ch == NUM_SUF_SYMBOL)							/* If reached numeric suffix symbol */
				{
					SufDig = 0;
					do
					{
						Ch = GetDigitChar (muiNumSufPrev[SufIdx], SufDig);		/* Get single digit as character from previously entered numeric suffix */
						Cnt++;														/* Increment the count of characters reached */
						SufDig++;
					} while ((Cnt < Pos + 1) && (SufDig < DigitCount (muiNumSufPrev[SufIdx])));
																							/* Loop until Pos'th char reached, or end of numeric suffix */
					SufIdx++;
				}
				else
#endif
					if ((Ch != '[') && (Ch != ']'))			/* If character is not a square bracket						*/
						Cnt++;														/* then increment the count of characters reached */
				j++;																	/* Increment character position inside Command Tree */
			}
			while (Cnt < Pos + 1);									/* Loop until reached Pos'th character in Command Tree */
		}
	}
	else																				/* If character position is beyond Command Tree */
	{
		if (Pos < Len)														/* If position within Full Command version of Input Command string 		*/
			Ch = SInpCmd[Pos - mCommandTreeLen];		/* then read character from Input Command string, taking into account
																							   the length of the Command Tree																			*/
		else																			/* If position beyond end of Full Command version of Input Command string */
			Ch = '\0';															/* then return the null char																							*/
	}

	return Ch;
}


/**************************************************************************************/
/************************ Non-Class Member Functions **********************************/
/**************************************************************************************/


/**************************************************************************************/
/* Translates an Input Units string into units, translating according to a						*/
/* Parameter Spec's Numerical Value Attributes.																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SUnits			- Pointer to start of Input Units string												*/
/*	[in]	UnitsLen		- Length of Input Units string																	*/
/*	[in]	pSpecAttr		- Pointer to Parameter Spec's Numeric Value Attributes					*/
/*	[out]	peUnits			- Pointer to returned type of units															*/
/*	[out] pUnitExp		- Pointer to returned exponent of units													*/
/*											(contents are undefined if an error code is returned)					*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_UNITS		-	Error:	The units were invalid for the Parameter Spec		*/
/**************************************************************************************/
UCHAR TranslateUnits(char *SUnits, SCPI_CHAR_IDX UnitsLen, const struct strSpecAttrNumericVal *pSpecAttr,
	enum enUnits *peUnits, signed char *pUnitExp)
{
	UCHAR Err = SCPI_ERR_NONE;
	BOOL bMatch = FALSE;
	UCHAR IdxUnit = 0;
	SCPI_CHAR_IDX PosSpec;
	SCPI_CHAR_IDX PosInpUnits;
	char ChSpec;
	char ChInpUnits;
	const enum enUnits *peAltUnits;

	while (!bMatch && (sSpecUnits[IdxUnit].SUnit[0])) /* Loop until a match is found or have tried all Unit Spec strings */
	{
		PosSpec = 0;															/* Reset position within Units Spec string to start of units */
		PosInpUnits = 0;													/* Start at first character in the Input Units string */

		ChSpec = sSpecUnits[IdxUnit].SUnit[PosSpec]; /* Get character from Unit Spec string */

		do
		{
			ChInpUnits = SUnits[PosInpUnits];				/* Get character from Input Units string */

			if (tolower(ChInpUnits) == tolower(ChSpec))	/* If characters in Input Units string and Unit Spec string match */
			{
				PosSpec++;																	/* Go to next position in Unit Spec string 												*/
				ChSpec = sSpecUnits[IdxUnit].SUnit[PosSpec];
				PosInpUnits++;															/* Go to next position in Input Units string											*/
			}
			else																		/* If characters do not match */
			{
				if (iswhitespace(ChInpUnits))				/* If character in Input Units string is whitespace */
					PosInpUnits++;											/* just skip it																			*/
				else																	/* If characters do not match and are not whitespace 				*/
					break;															/* then this Unit Spec string does not match - try next one	*/
			}
		} while ((PosInpUnits < UnitsLen) && ChSpec);	/* Loop until end of Input Units string or end of Unit Spec string */

		if ((PosInpUnits == UnitsLen) && !ChSpec)	/* If reached end of Input Units string and end of Unit Spec string */
			bMatch = TRUE;													/* then it is a successful match																		*/
		else
			IdxUnit++;															/* If not a match then try the next Unit Spec string */
	}

	if (bMatch)																	/* If a match was found */
	{
		*peUnits = sSpecUnits[IdxUnit].eUnits;		/* Get type of unit from Unit Spec */
		*pUnitExp = sSpecUnits[IdxUnit].Exp;			/* and get unit exponent from Unit Spec */

																							/* Now determine if units are valid for the Parameter Spec's Numeric Value Attributes */

		if (pSpecAttr->eUnits != *peUnits)				/* If units are not the same as Parameter Spec's default units */
		{
			/* Check if units match any of Parameter Spec's alternative units */
			bMatch = FALSE;
			peAltUnits = pSpecAttr->peAlternateUnits; /* Set pointer to start of list of spec's alternative units */
			if (peAltUnits)													/* If there are some alternate units */
			{
				while (!bMatch && (*peAltUnits != U_END)) /* Loop until match found or reached end of list of alternative units */
				{
					if (*peAltUnits == *peUnits)				/* If alternative units match the units from the Input Units string */
						bMatch = TRUE;										/* then found a match																								*/
					else
						peAltUnits++;											/* If alternative units don't match then look at next alternative units */
				};
			};
			if (!bMatch)														/* If units do not match any of the units allowed by the Parameter Spec */
				Err = SCPI_ERR_PARAM_UNITS;					/* then return error code 																							*/
		}
	}
	else																				/* If Input Unit string does not match any known units 	*/
		Err = SCPI_ERR_PARAM_UNITS;								/* then return error code																*/

	return Err;
}


/**************************************************************************************/
/* Compares two strings (case-insensitive comparison)																	*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] S1		- Pointer to start of first string to compare														*/
/*	[in] Len1	- Length of first string																								*/
/*	[in] S2		- Pointer to start of second string to compare													*/
/*	[in] Len2 - Length of second string																								*/
/*																																										*/
/* Return Values:																																			*/
/*	TRUE	-	Strings are exactly equal (using case-insensitve comparison)							*/
/*	FALSE	- Strings are not equal																											*/
/**************************************************************************************/
BOOL StringsEqual(const char *S1, SCPI_CHAR_IDX Len1, const char *S2, SCPI_CHAR_IDX Len2)
{
	BOOL bEqual;
	SCPI_CHAR_IDX Pos;

	if (Len1 != Len2)
		return FALSE;

	bEqual = TRUE;
	for (Pos = 0; Pos < Len1; Pos++)
	{
		if (tolower(S1[Pos]) != tolower(S2[Pos]))
			bEqual = FALSE;
	}

	return bEqual;
}


/**************************************************************************************/
/* Returns value of a double-precision floating-point number rounded to the nearest		*/
/* integer, according to SCPI standard (defined in IEEE488.2 specification).					*/
/* The specification says: Round to nearest integer, rouding up if greater or equal		*/
/* to half values (.5), regardless of the sign of the number													*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] fdVal	- Double-precsion number to be rounded																*/
/*																																										*/
/* Return Value:																																			*/
/*	Rounded number																																		*/
/**************************************************************************************/
long SCPIround(double fdVal)
{
	if (fdVal > 0)
		fdVal += 0.5;
	else
		fdVal -= 0.5;
	return (long)fdVal;														/* Casting to long truncates the value to an integer */
}


#ifdef SUPPORT_NUM_SUFFIX
/**************************************************************************************/
/* Returns a single digit as an ASCII character from an unsigned integer, given the		*/
/* position of the digit.																															*/
/* Note: Function assumes an unsigned integer has 16 bits (i.e. 65535 max value)			*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] uiVal			- Number																													*/
/*	[in] Pos				- Position of digit within the number, where 0 is most sig digit	*/
/* Return Value:																																			*/
/* 	Digit as ASCII character, i.e. '0'..'9', or 0 if Pos is bigger than length of 		*/
/*	number.																																						*/
/**************************************************************************************/
char GetDigitChar(unsigned int uiNum, SCPI_CHAR_IDX Pos)
{
	char Ch = '\0';
	SCPI_CHAR_IDX Len;

	Len = DigitCount(uiNum);
	if (Pos == Len - 1)
		Ch = (SCPI_CHAR_IDX)(uiNum % 10);
	else if (Pos == Len - 2)
		Ch = (SCPI_CHAR_IDX)((uiNum / 10) % 10);
	else if (Pos == Len - 3)
		Ch = (SCPI_CHAR_IDX)((uiNum / 100) % 10);
	else if (Pos == Len - 4)
		Ch = (SCPI_CHAR_IDX)((uiNum / 1000) % 10);
	else if (Pos == Len - 5)
		Ch = (SCPI_CHAR_IDX)(uiNum / 10000);
	Ch += '0';

	return Ch;
}


/**************************************************************************************/
/* Counts the number of digits in an unsigned integer																	*/
/* Note: Function assumes an unsigned integer has 16 bits (i.e. 65535 max value)			*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] uiVal			- Number																													*/
/* Return Value:																																			*/
/* 	Number of digits																																	*/
/**************************************************************************************/
SCPI_CHAR_IDX DigitCount(unsigned int uiNum)
{
	SCPI_CHAR_IDX Count;

	if (uiNum < 10)
		Count = 1;
	else if (uiNum < 100)
		Count = 2;
	else if (uiNum < 1000)
		Count = 3;
	else if (uiNum < 10000)
		Count = 4;
	else
		Count = 5;

	return Count;
}
#endif


